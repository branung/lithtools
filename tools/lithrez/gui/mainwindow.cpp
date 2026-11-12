#include "mainwindow.h"

#include "modelview.h"
#include "soundview.h"
#include "spriteview.h"
#include "bankview.h"
#include "stringtable.h"
#include "textureview.h"
#include "util.h"
#include "wizard.h"
#include <lithtech/butes.hpp>
#include <lithtech/ps2rez.hpp>
#include <lithtech/spr.hpp>

#include <QAction>
#include <QActionGroup>
#include <QFontDatabase>
#include <QRegularExpression>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <atomic>
#include <exception>
#include <limits>
#include <map>
#include <mutex>
#include <thread>

using namespace lithtech;
using rez::Archive;
using rez::Directory;
using rez::Resource;
using util::qs;
using util::ss;

namespace {

enum Roles { KindRole = Qt::UserRole + 1, PtrRole };
enum Kind { KindDir = 1, KindRes = 2 };

class DetailsItem : public QTreeWidgetItem {
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem& o) const override {
        int c = treeWidget() ? treeWidget()->sortColumn() : 0;
        if (c == 2) {
            QVariant a = data(c, Qt::UserRole), b = o.data(c, Qt::UserRole);
            if (a.isValid() != b.isValid()) return !a.isValid();
            return a.toULongLong() < b.toULongLong();
        }
        return QTreeWidgetItem::operator<(o);
    }
};

const int kRecentMax = 8;

QTemporaryDir& tempDir() {
    static QTemporaryDir d;
    return d;
}

QString typeLabel(const QString& ext) {
    static const std::map<QString, QString> names = {
        {"DTX", "Texture"}, {"ABC", "Model"}, {"LTB", "Model"}, {"DAT", "World"}, {"WAV", "Sound"},
        {"TXT", "Text"}, {"DLL", "Library"}, {"LTO", "Object module"}, {"SPR", "Sprite"},
        {"PCX", "Image"}, {"TGA", "Image"}, {"BMP", "Image"}, {"MP3", "Sound"}, {"FXF", "Effects"},
        {"FCF", "Effects"}, {"LTA", "LithTech text"}, {"BUT", "Attributes"}, {"CFG", "Config"},
        {"SGT", "Music segment"}, {"STY", "Music style"}, {"DLS", "Instruments"}, {"LIP", "Lip sync"},
        {"LPS", "Sound bank"}};
    auto it = names.find(ext.toUpper());
    return it != names.end() ? it->second + " (" + ext.toUpper() + ")" : (ext.isEmpty() ? "File" : ext.toUpper() + " file");
}

bool isTextType(const QString& t) {
    static const QStringList text = {"TXT", "CFG", "BUT", "INI", "LTA", "FXF", "FCF", "SCC", "DEP", "CSV", "H", "LOG"};
    return text.contains(t.toUpper());
}

}  // namespace

// ===========================================================================

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setAcceptDrops(true);
    buildActions();
    buildViews();
    buildMenus();
    buildToolbar();
    status_ = new QLabel(this);
    statusBar()->addWidget(status_, 1);
    ps2Note_ = new QLabel(tr("PS2 archive: editing is experimental (not tested on a PS2)"), this);
    ps2Note_->setToolTip(tr("LITH_PS2.REZ files are named from a list of recovered names; the rest are under _UNNAMED.\n"
                            "Saving changes follows what the PS2's reader requires (docs/PS2REZ.md) but hasn't been\n"
                            "tried on a PS2 or an emulator. Keep a copy of the original."));
    ps2Note_->hide();
    statusBar()->addPermanentWidget(ps2Note_);
    QSettings s;
    restoreGeometry(s.value("geometry").toByteArray());
    restoreState(s.value("state").toByteArray());
    setViewMode(ViewMode(s.value("viewMode", int(ViewMode::Details)).toInt()));
    newRez();
    updateRecent();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildActions() {
    auto mk = [&](const QString& text, QKeySequence key, QStyle::StandardPixmap icon, auto slot) {
        auto* a = new QAction(text, this);
        if (!key.isEmpty()) a->setShortcut(key);
        if (icon != QStyle::SP_CustomBase) a->setIcon(style()->standardIcon(icon));
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    const auto none = QStyle::SP_CustomBase;
    aNew = mk(tr("&New Rez File"), QKeySequence::New, QStyle::SP_FileIcon, [this] { if (maybeSave()) newRez(); });
    aProject = mk(tr("New Rez &Project..."), {}, QStyle::SP_DirIcon, [this] { doNewProject(); });
    aWizard = mk(tr("Rez &Wizard..."), {}, QStyle::SP_FileDialogInfoView, [this] { doWizard(); });
    aOpen = mk(tr("&Open..."), QKeySequence::Open, QStyle::SP_DialogOpenButton, [this] { doOpen(); });
    aClose = mk(tr("&Close"), QKeySequence::Close, none, [this] { doClose(); });
    aSave = mk(tr("&Save"), QKeySequence::Save, QStyle::SP_DialogSaveButton, [this] { doSave(); });
    aSaveAs = mk(tr("Save &As..."), QKeySequence::SaveAs, none, [this] { doSaveAs(); });
    aImport = mk(tr("&Import Rez File..."), {}, none, [this] { doImport(); });
    aMerge = mk(tr("&Merge Rez Files..."), {}, none, [this] { doMerge(); });
    aInfo = mk(tr("Rez File &Information..."), {}, QStyle::SP_MessageBoxInformation, [this] { doInfo(); });
    aExit = mk(tr("E&xit"), QKeySequence::Quit, none, [this] { close(); });
    aTexViewer = mk(tr("&Texture Viewer..."), {}, none, [] {
        auto* w = new TextureWindow;
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->show();
    });
    aStrings = mk(tr("&String Table Editor..."), {}, none, [] {
        auto* w = new StringTableWindow;
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->show();
    });
    aModelViewer = mk(tr("&Model Viewer..."), {}, none, [this] {
        QString f = QFileDialog::getOpenFileName(this, tr("Open Model"), {}, tr("LithTech models (*.abc *.ltb *.ABC *.LTB)"));
        if (f.isEmpty()) return;
        QFile file(f);
        if (!file.open(QIODevice::ReadOnly)) return;
        auto* v = new ModelView;
        v->setAttribute(Qt::WA_DeleteOnClose);
        connect(v, &ModelView::skinActivated, v, [](const QString& path) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        });
        v->setWindowTitle(QFileInfo(f).fileName() + " - LithRez Studio");
        QString dir = QFileInfo(f).absolutePath();
        QString stem = QFileInfo(f).completeBaseName();
        QString err;
        bool ok = v->loadAbc(file.readAll(), QFileInfo(f).fileName(), [dir, stem](int idx, int, QString& path) -> QByteArray {
            // SKINS beside MODELS, as NOLF and AvP2 lay them out.
            QDir d(dir);
            QStringList where = {dir};
            if (d.cdUp()) {
                for (const QString& n : d.entryList(QDir::Dirs))
                    if (n.compare("skins", Qt::CaseInsensitive) == 0) where << d.filePath(n);
            }
            QStringList cands;
            for (const QString& w : where)
                for (const QString& n : QDir(w).entryList({"*.dtx", "*.DTX"}, QDir::Files, QDir::Name | QDir::IgnoreCase))
                    if (n.startsWith(stem, Qt::CaseInsensitive)) cands << QDir(w).filePath(n);
            std::stable_sort(cands.begin(), cands.end(), [&](const QString& a, const QString& b) {
                bool ea = QFileInfo(a).completeBaseName().compare(stem, Qt::CaseInsensitive) == 0;
                bool eb = QFileInfo(b).completeBaseName().compare(stem, Qt::CaseInsensitive) == 0;
                return ea && !eb;
            });
            if (idx < cands.size()) {
                QFile s(cands[idx]);
                if (s.open(QIODevice::ReadOnly)) {
                    path = cands[idx];
                    return s.readAll();
                }
            }
            return {};
        }, &err);
        if (!ok) {
            QMessageBox::warning(this, tr("Model Viewer"), err);
            delete v;
            return;
        }
        v->resize(900, 700);
        v->show();
    });

    aSoundPlayer = mk(tr("S&ound Player..."), {}, none, [this] {
        QString f = QFileDialog::getOpenFileName(this, tr("Open Sound"), {}, tr("Sounds and PS2 sound banks (*.wav *.WAV *.lps *.LPS)"));
        if (f.isEmpty()) return;
        QFile file(f);
        if (!file.open(QIODevice::ReadOnly)) return;
        openSound(file.readAll(), QFileInfo(f).fileName());
    });

    aCut = mk(tr("Cu&t"), QKeySequence::Cut, none, [this] { doCut(); });
    aCopy = mk(tr("&Copy"), QKeySequence::Copy, none, [this] { doCut(); });
    aPaste = mk(tr("&Paste"), QKeySequence::Paste, none, [this] { doPaste(); });
    aSelectAll = mk(tr("Select &All"), QKeySequence::SelectAll, none, [this] { doSelectAll(); });
    aDelete = mk(tr("&Delete"), QKeySequence::Delete, QStyle::SP_TrashIcon, [this] { doDelete(); });
    aRename = mk(tr("&Rename"), QKeySequence(Qt::Key_F2), none, [this] { doRename(); });

    aExtract = mk(tr("&Extract..."), QKeySequence(Qt::CTRL | Qt::Key_E), QStyle::SP_ArrowDown, [this] { doExtract(false); });
    aExtractAll = mk(tr("Extract &All..."), {}, none, [this] { doExtract(true); });
    aInsert = mk(tr("&Insert Files..."), QKeySequence(Qt::Key_Insert), QStyle::SP_ArrowUp, [this] { doInsertFiles(); });
    aReplace = mk(tr("&Replace..."), {}, none, [this] { doReplace(); });
    aInsertFolder = mk(tr("Insert F&older..."), {}, none, [this] { doInsertFolder(); });
    aInsertEmpty = mk(tr("Insert Empty &Folder..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N), QStyle::SP_FileDialogNewFolder,
                      [this] { doInsertEmptyFolder(); });

    aToolbar = new QAction(tr("&Toolbar"), this);
    aStatus = new QAction(tr("&Status Bar"), this);
    aLocation = new QAction(tr("&Location Bar"), this);
    aPreview = new QAction(tr("&Preview Pane"), this);
    for (QAction* a : {aToolbar, aStatus, aLocation, aPreview}) {
        a->setCheckable(true);
        a->setChecked(true);
    }
    connect(aStatus, &QAction::toggled, this, [this](bool on) { statusBar()->setVisible(on); });

    auto* modes = new QActionGroup(this);
    aLarge = mk(tr("Lar&ge Icons"), {}, none, [this] { setViewMode(ViewMode::LargeIcons); });
    aSmall = mk(tr("S&mall Icons"), {}, none, [this] { setViewMode(ViewMode::SmallIcons); });
    aList = mk(tr("&List"), {}, QStyle::SP_FileDialogListView, [this] { setViewMode(ViewMode::List); });
    aDetails = mk(tr("&Details"), {}, QStyle::SP_FileDialogDetailedView, [this] { setViewMode(ViewMode::Details); });
    for (QAction* a : {aLarge, aSmall, aList, aDetails}) {
        a->setCheckable(true);
        modes->addAction(a);
    }
    aBack = mk(tr("&Back"), QKeySequence::Back, QStyle::SP_ArrowBack, [this] { doBack(); });
    aForward = mk(tr("&Forward"), QKeySequence::Forward, QStyle::SP_ArrowForward, [this] { doForward(); });
    aUp = mk(tr("&Up"), QKeySequence(Qt::ALT | Qt::Key_Up), QStyle::SP_FileDialogToParent, [this] { doUp(); });
    aRefresh = mk(tr("&Refresh"), QKeySequence::Refresh, QStyle::SP_BrowserReload, [this] { rebuildTree(); });

    aSettings = mk(tr("&Settings..."), {}, none, [this] { doSettings(); });
    aHelp = mk(tr("&Help"), QKeySequence::HelpContents, QStyle::SP_MessageBoxQuestion, [this] {
        QMessageBox::information(this, tr("LithRez Studio Help"),
            tr("<p><b>LithRez Studio</b> reads, edits and creates LithTech .rez files.</p>"
               "<ul><li>Browse with the folder tree and the contents view; type a path in the location bar.</li>"
               "<li><b>Insert</b> adds files (or drag them in from your file manager); <b>Extract</b> writes the "
               "selection out (or drag it out).</li>"
               "<li>Cut and Copy both <i>move</i> on Paste, as in WinRez: a rez file never holds the same data twice.</li>"
               "<li>Double-click a texture, model or CRes.dll to open it in the viewer or string editor.</li>"
               "<li><b>New Rez Project</b> builds a rez from a folder exactly as LithRez does.</li></ul>"
               "<p>The command line tool <tt>lithrez</tt> does the same for scripts and CI.</p>"));
    });
    aAbout = mk(tr("&About LithRez Studio"), {}, none, [this] { doAbout(); });

    recentMenu_ = new QMenu(tr("Recent &Files"), this);
    for (int i = 0; i < kRecentMax; ++i) {
        auto* a = new QAction(this);
        a->setVisible(false);
        connect(a, &QAction::triggered, this, [this, a] {
            if (maybeSave()) openRez(a->data().toString());
        });
        recentActions_ << a;
        recentMenu_->addAction(a);
    }
}

void MainWindow::buildMenus() {
    QMenu* f = menuBar()->addMenu(tr("&File"));
    QMenu* fnew = f->addMenu(tr("&New"));
    fnew->addAction(aNew);
    fnew->addAction(aProject);
    fnew->addAction(aWizard);
    f->addAction(aOpen);
    f->addAction(aClose);
    f->addSeparator();
    f->addAction(aSave);
    f->addAction(aSaveAs);
    f->addSeparator();
    f->addAction(aImport);
    f->addAction(aMerge);
    f->addAction(aInfo);
    f->addSeparator();
    f->addMenu(recentMenu_);
    f->addSeparator();
    f->addAction(aExit);

    QMenu* e = menuBar()->addMenu(tr("&Edit"));
    e->addAction(aCut);
    e->addAction(aCopy);
    e->addAction(aPaste);
    e->addSeparator();
    e->addAction(aSelectAll);
    e->addAction(aDelete);
    e->addAction(aRename);

    QMenu* o = menuBar()->addMenu(tr("&Object"));
    o->addAction(aExtract);
    o->addAction(aExtractAll);
    o->addSeparator();
    o->addAction(aInsert);
    o->addAction(aReplace);
    o->addAction(aInsertFolder);
    o->addAction(aInsertEmpty);

    QMenu* v = menuBar()->addMenu(tr("&View"));
    v->addAction(aToolbar);
    v->addAction(aStatus);
    v->addAction(aLocation);
    v->addAction(aPreview);
    v->addSeparator();
    v->addAction(aLarge);
    v->addAction(aSmall);
    v->addAction(aList);
    v->addAction(aDetails);
    v->addSeparator();
    v->addAction(aBack);
    v->addAction(aForward);
    v->addAction(aUp);
    v->addSeparator();
    v->addAction(aRefresh);

    QMenu* t = menuBar()->addMenu(tr("&Tools"));
    t->addAction(aTexViewer);
    t->addAction(aModelViewer);
    t->addAction(aSoundPlayer);
    t->addAction(aStrings);
    t->addSeparator();
    t->addAction(aSettings);

    QMenu* h = menuBar()->addMenu(tr("&Help"));
    h->addAction(aHelp);
    h->addAction(aAbout);
}

void MainWindow::buildToolbar() {
    toolbar_ = addToolBar(tr("Toolbar"));
    toolbar_->setObjectName("toolbar");
    for (QAction* a : {aNew, aWizard, aOpen, aSave}) toolbar_->addAction(a);
    toolbar_->addSeparator();
    for (QAction* a : {aBack, aForward, aUp}) toolbar_->addAction(a);
    toolbar_->addSeparator();
    for (QAction* a : {aExtract, aInsert, aInsertEmpty, aDelete}) toolbar_->addAction(a);
    toolbar_->addSeparator();
    for (QAction* a : {aList, aDetails, aHelp}) toolbar_->addAction(a);
    connect(aToolbar, &QAction::toggled, toolbar_, &QToolBar::setVisible);

    addToolBarBreak(Qt::TopToolBarArea);
    auto* lb = new QToolBar(tr("Location"), this);
    lb->setObjectName("location");
    addToolBar(Qt::TopToolBarArea, lb);
    lb->addWidget(new QLabel(tr(" Location: "), lb));
    location_ = new QComboBox(lb);
    location_->setEditable(true);
    location_->setInsertPolicy(QComboBox::NoInsert);
    location_->setMinimumWidth(400);
    location_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    lb->addWidget(location_);
    auto* go = new QPushButton(tr("Go"), lb);
    lb->addWidget(go);
    connect(go, &QPushButton::clicked, this, [this] { doGo(); });
    connect(location_->lineEdit(), &QLineEdit::returnPressed, this, [this] { doGo(); });
    connect(location_, QOverload<int>::of(&QComboBox::activated), this, [this](int) { doGo(); });
    connect(aLocation, &QAction::toggled, lb, &QToolBar::setVisible);
    locationBar_ = lb;
}

void MainWindow::buildViews() {
    auto* split = new QSplitter(this);
    tree_ = new QTreeWidget(split);
    tree_->setHeaderHidden(true);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->setDragDropMode(QAbstractItemView::DropOnly);
    tree_->viewport()->installEventFilter(this);
    stack_ = new QStackedWidget(split);

    details_ = new QTreeWidget(stack_);
    details_->setRootIsDecorated(false);
    details_->setSortingEnabled(true);
    details_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    details_->setHeaderLabels({tr("Name"), tr("Type"), tr("Size"), tr("ID"), tr("Modified")});
    details_->header()->setStretchLastSection(false);
    details_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 5; ++c) details_->header()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    details_->setContextMenuPolicy(Qt::CustomContextMenu);
    details_->setDragEnabled(true);
    details_->viewport()->installEventFilter(this);
    details_->installEventFilter(this);
    icons_ = new QListWidget(stack_);
    icons_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    icons_->setContextMenuPolicy(Qt::CustomContextMenu);
    icons_->setResizeMode(QListView::Adjust);
    icons_->setDragEnabled(true);
    icons_->viewport()->installEventFilter(this);
    icons_->installEventFilter(this);
    stack_->addWidget(details_);
    stack_->addWidget(icons_);
    split->setStretchFactor(1, 3);
    setCentralWidget(split);

    connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* it) {
        if (!it) return;
        auto* d = reinterpret_cast<Directory*>(it->data(0, PtrRole).value<quintptr>());
        if (d && d != current_) showDir(d);
    });

    auto activated = [this](QVariant kind, QVariant ptr) {
        if (kind.toInt() == KindDir) onActivated(reinterpret_cast<Directory*>(ptr.value<quintptr>()), nullptr);
        else onActivated(nullptr, reinterpret_cast<Resource*>(ptr.value<quintptr>()));
    };
    connect(details_, &QTreeWidget::itemActivated, this,
            [activated](QTreeWidgetItem* it) { activated(it->data(0, KindRole), it->data(0, PtrRole)); });
    connect(icons_, &QListWidget::itemActivated, this,
            [activated](QListWidgetItem* it) { activated(it->data(KindRole), it->data(PtrRole)); });
    auto selChanged = [this] {
        Sel s = selection();
        if (s.res.size() == 1 && s.dirs.empty()) preview(s.res[0]);
        updateActions();
        quint64 bytes = 0;
        for (Resource* r : s.res) bytes += r->size;
        if (!s.empty())
            status_->setText(tr("%1 selected (%2)").arg(s.res.size() + s.dirs.size()).arg(util::humanSize(bytes)));
    };
    connect(details_, &QTreeWidget::itemSelectionChanged, this, selChanged);
    connect(icons_, &QListWidget::itemSelectionChanged, this, selChanged);

    auto ctx = [this](const QPoint&) {
        QMenu m(this);
        m.addAction(aExtract);
        if (sender() != tree_) m.addAction(aReplace);
        m.addAction(aInsert);
        m.addAction(aInsertFolder);
        m.addSeparator();
        m.addAction(aCut);
        m.addAction(aCopy);
        m.addAction(aPaste);
        m.addSeparator();
        m.addAction(aDelete);
        m.addAction(aRename);
        m.addSeparator();
        m.addAction(aInsertEmpty);
        m.exec(QCursor::pos());
    };
    connect(details_, &QWidget::customContextMenuRequested, this, ctx);
    connect(icons_, &QWidget::customContextMenuRequested, this, ctx);
    connect(tree_, &QWidget::customContextMenuRequested, this, ctx);

    // Drags out: the selection, extracted to a temporary folder.
    for (QAbstractItemView* v : {static_cast<QAbstractItemView*>(details_), static_cast<QAbstractItemView*>(icons_)}) {
        v->setDragDropMode(QAbstractItemView::DragDrop);
        v->setDefaultDropAction(Qt::CopyAction);
    }

    previewDock_ = new QDockWidget(tr("Preview"), this);
    previewDock_->setObjectName("preview");
    previewStack_ = new QStackedWidget(previewDock_);
    infoPreview_ = new QLabel(tr("Select a file to preview it."), previewStack_);
    infoPreview_->setAlignment(Qt::AlignCenter);
    infoPreview_->setWordWrap(true);
    texPreview_ = new TextureView(previewStack_);
    modelPreview_ = new ModelView(previewStack_);
    connect(modelPreview_, &ModelView::skinActivated, this, &MainWindow::showResource);
    soundPreview_ = new SoundView(previewStack_);
    bankPreview_ = new BankView(previewStack_);
    spritePreview_ = new SpriteView(previewStack_);
    connect(spritePreview_, &SpriteView::frameActivated, this, &MainWindow::showResource);
    textPreview_ = new QPlainTextEdit(previewStack_);
    textPreview_->setReadOnly(true);
    textPreview_->setLineWrapMode(QPlainTextEdit::NoWrap);
    textPreview_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    previewStack_->addWidget(infoPreview_);
    previewStack_->addWidget(texPreview_);
    previewStack_->addWidget(modelPreview_);
    previewStack_->addWidget(soundPreview_);
    previewStack_->addWidget(bankPreview_);
    previewStack_->addWidget(spritePreview_);
    previewStack_->addWidget(textPreview_);
    previewDock_->setWidget(previewStack_);
    addDockWidget(Qt::RightDockWidgetArea, previewDock_);
    resizeDocks({previewDock_}, {420}, Qt::Horizontal);
    split->setSizes({260, 600});
    connect(aPreview, &QAction::toggled, previewDock_, &QDockWidget::setVisible);
    connect(previewDock_, &QDockWidget::visibilityChanged, aPreview, &QAction::setChecked);
    // a sound stops when something else is previewed or the pane closes
    connect(previewStack_, &QStackedWidget::currentChanged, this, [this] {
        if (previewStack_->currentWidget() != soundPreview_) soundPreview_->stop();
        if (previewStack_->currentWidget() != bankPreview_) bankPreview_->stop();
        if (previewStack_->currentWidget() != spritePreview_) spritePreview_->pause();
    });
    connect(previewDock_, &QDockWidget::visibilityChanged, this, [this](bool on) {
        if (!on) soundPreview_->stop(), bankPreview_->stop(), spritePreview_->pause();
    });
    resize(1280, 800);
}

// ---------------------------------------------------------------------------
// State

void MainWindow::updateTitle() {
    QString name = path_.isEmpty() ? tr("Untitled") : QFileInfo(path_).fileName();
    setWindowTitle(QStringLiteral("%1%2 - LithRez Studio").arg(name, modified_ ? "*" : ""));
    setWindowModified(modified_);
}

void MainWindow::setModified(bool m) {
    modified_ = m;
    if (m && archive_) archive_->header.time = rez::current_time(std::nullopt);
    updateTitle();
    updateActions();
}

void MainWindow::updateActions() {
    Sel s = selection();
    bool any = !s.empty();
    aExtract->setEnabled(any || current_);
    aDelete->setEnabled(any);
    aRename->setEnabled(s.res.size() + s.dirs.size() == 1);
    aReplace->setEnabled(s.res.size() == 1 && s.dirs.empty());
    aCut->setEnabled(any);
    aCopy->setEnabled(any);
    aPaste->setEnabled(!clipboard_.empty());
    aBack->setEnabled(!back_.empty());
    aForward->setEnabled(!forward_.empty());
    aUp->setEnabled(current_ && current_->parent());
    aSave->setEnabled(modified_ || path_.isEmpty());
    if (archive_) {
        status_->setText(tr("%1 folders, %2 files").arg(archive_->root().count_dirs()).arg(archive_->root().count_resources()));
    }
}

bool MainWindow::maybeSave() {
    if (!modified_) return true;
    auto r = QMessageBox::question(this, tr("LithRez Studio"), tr("Save the changes to %1?").arg(path_.isEmpty() ? tr("Untitled") : QFileInfo(path_).fileName()),
                                   QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (r == QMessageBox::Cancel) return false;
    if (r == QMessageBox::Save) return doSave();
    return true;
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (!maybeSave()) {
        e->ignore();
        return;
    }
    QSettings s;
    s.setValue("geometry", saveGeometry());
    s.setValue("state", saveState());
    s.setValue("viewMode", int(mode_));
    e->accept();
}

void MainWindow::newRez() {
    QSignalBlocker b1(details_), b2(icons_);
    clearPreview();
    butesFrom_ = nullptr;
    butes_.clear();
    archive_ = std::make_unique<Archive>();
    if (ps2Note_) ps2Note_->hide();
    archive_->header.user_title = std::string(QSettings().value("userTitle", qs(std::string(rez::kLithTechUserTitle))).toString().toUtf8());
    path_.clear();
    modified_ = false;
    back_.clear();
    forward_.clear();
    clipboard_ = {};
    current_ = &archive_->root();
    rebuildTree();
    updateTitle();
}

void MainWindow::runBusy(const QString& title, bool canCancel, const std::function<void(Busy&)>& job,
                         const std::function<void()>& finish, const QString& finishText) {
    // The job runs on a thread of its own so the window keeps painting; the
    // dialog shows straight away, every time, even if a fast machine only
    // flashes it, so a slow one is never left looking hung.
    Busy busy;
    std::atomic<bool> finished{false};
    std::exception_ptr error;
    std::thread worker([&] {
        try {
            job(busy);
        } catch (...) {
            error = std::current_exception();
        }
        finished = true;
    });

    QProgressDialog prog(title, canCancel ? tr("Cancel") : QString(), 0, 0, this);
    prog.setWindowTitle(QApplication::applicationName());
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(0);
    prog.setAutoReset(false);
    prog.setAutoClose(false);
    if (canCancel) connect(&prog, &QProgressDialog::canceled, this, [&] { busy.cancel = true; });
    auto update = [&] {
        uint64_t done, total;
        QString detail;
        {
            std::lock_guard<std::mutex> lock(busy.m);
            done = busy.done;
            total = busy.total;
            detail = busy.detail;
        }
        if (total) {
            // QProgressDialog's range is an int: work in thousandths
            prog.setMaximum(1000);
            prog.setValue(int(std::min(done, total) * 1000 / total));
        } else {
            prog.setMaximum(0);
            prog.setValue(0);
        }
        prog.setLabelText(detail.isEmpty() ? title : title + "\n" + detail);
    };
    busy_ = true;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    prog.show();
    update();
    QApplication::processEvents();
    QEventLoop loop;
    QTimer poll;
    connect(&poll, &QTimer::timeout, this, [&] {
        if (canCancel && prog.wasCanceled()) busy.cancel = true;
        if (finished) {
            loop.quit();
            return;
        }
        update();
    });
    poll.start(50);
    if (!finished) loop.exec();
    poll.stop();
    worker.join();
    if (!error) {   // the bar full before it goes, however quick the job was
        prog.setMaximum(1);
        prog.setValue(1);
        QApplication::processEvents();
    }
    if (!error && finish) {
        prog.setCancelButton(nullptr);
        prog.setMaximum(0);
        prog.setValue(0);
        prog.setLabelText(finishText.isEmpty() ? title : title + "\n" + finishText);
        QApplication::processEvents();
        finish();
    }
    QApplication::restoreOverrideCursor();
    busy_ = false;
    prog.close();
    if (error) std::rethrow_exception(error);
}

std::unique_ptr<Archive> MainWindow::openArchive(const QString& path,
                                                 const std::function<void(std::unique_ptr<Archive>&)>& finish) {
    // Progress: directory bytes or PS2 entries (a busy bar while the total
    // isn't known), and the folders and files read so far.
    std::unique_ptr<Archive> a;
    rez::OpenOptions opts;
    runBusy(tr("Opening %1...").arg(QFileInfo(path).fileName()), true,
            [&](Busy& busy) {
                opts.progress = [&](const rez::OpenProgress& p) {
                    busy.set(p.done, p.total,
                             tr("%1 files in %2 folders")
                                 .arg(QLocale().toString(qulonglong(p.resources)), QLocale().toString(qulonglong(p.dirs))));
                    return !busy.cancel.load();
                };
                a = Archive::open(ss(path), opts);
            },
            finish ? std::function<void()>([&] { finish(a); }) : std::function<void()>(),
            tr("Building the folder tree..."));
    return a;
}

bool MainWindow::openRez(const QString& path) {
    try {
        openArchive(path, [&](std::unique_ptr<Archive>& a) {
            QSignalBlocker b1(details_), b2(icons_);
            clearPreview();
            butesFrom_ = nullptr;
            butes_.clear();
            archive_ = std::move(a);
            path_ = path;
            modified_ = false;
            ps2Note_->setVisible(archive_->is_ps2());
            back_.clear();
            forward_.clear();
            clipboard_ = {};
            current_ = &archive_->root();
            rebuildTree();
            updateTitle();
        });
    } catch (const rez::Cancelled&) {
        return false;
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Open"), tr("Can't open %1:\n%2").arg(path, qs(e.what())));
        return false;
    }
    addRecent(path);
    return true;
}

// ---------------------------------------------------------------------------
// Tree and contents

void MainWindow::rebuildTree() {
    tree_->clear();
    if (!archive_) return;
    auto* root = new QTreeWidgetItem(tree_);
    root->setText(0, path_.isEmpty() ? tr("Untitled") : QFileInfo(path_).fileName());
    root->setIcon(0, style()->standardIcon(QStyle::SP_DriveHDIcon));
    root->setData(0, PtrRole, QVariant::fromValue(quintptr(&archive_->root())));
    for (auto& d : archive_->root().dirs()) addTreeDir(root, d.get());
    root->setExpanded(true);
    if (!current_) current_ = &archive_->root();
    showDir(current_, false);
}

void MainWindow::addTreeDir(QTreeWidgetItem* parent, Directory* d) {
    auto* it = new QTreeWidgetItem(parent);
    it->setText(0, qs(d->name));
    it->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    it->setData(0, PtrRole, QVariant::fromValue(quintptr(d)));
    std::vector<Directory*> subs;
    for (auto& s : d->dirs()) subs.push_back(s.get());
    std::sort(subs.begin(), subs.end(), [](Directory* a, Directory* b) { return QString::compare(qs(a->name), qs(b->name), Qt::CaseInsensitive) < 0; });
    for (Directory* s : subs) addTreeDir(it, s);
    parent->sortChildren(0, Qt::AscendingOrder);
}

void MainWindow::showDir(Directory* d, bool addHistory) {
    if (!d) return;
    if (addHistory && current_ && current_ != d) {
        back_.push_back(current_);
        forward_.clear();
    }
    current_ = d;
    // select it in the tree
    QTreeWidgetItemIterator it(tree_);
    while (*it) {
        if ((*it)->data(0, PtrRole).value<quintptr>() == quintptr(d)) {
            QSignalBlocker b(tree_);
            tree_->setCurrentItem(*it);
            for (QTreeWidgetItem* p = (*it)->parent(); p; p = p->parent()) p->setExpanded(true);
            break;
        }
        ++it;
    }
    QString loc = "\\" + qs(d->path('\\'));
    location_->setEditText(loc);
    if (location_->findText(loc) < 0) location_->insertItem(0, loc);
    while (location_->count() > 16) location_->removeItem(location_->count() - 1);
    refreshContents();
}

void MainWindow::refreshContents() {
    details_->clear();
    icons_->clear();
    if (!current_) return;
    QIcon dirIcon = style()->standardIcon(QStyle::SP_DirIcon);
    QIcon fileIcon = style()->standardIcon(QStyle::SP_FileIcon);
    details_->setSortingEnabled(false);
    for (auto& d : current_->dirs()) {
        auto* it = new DetailsItem(details_);
        it->setText(0, qs(d->name));
        it->setIcon(0, dirIcon);
        it->setText(1, tr("Folder"));
        it->setText(4, d->time ? QDateTime::fromSecsSinceEpoch(d->time).toString("yyyy-MM-dd hh:mm") : QString());
        it->setData(0, KindRole, KindDir);
        it->setData(0, PtrRole, QVariant::fromValue(quintptr(d.get())));
        auto* li = new QListWidgetItem(dirIcon, qs(d->name), icons_);
        li->setData(KindRole, KindDir);
        li->setData(PtrRole, QVariant::fromValue(quintptr(d.get())));
    }
    for (auto& r : current_->resources()) {
        auto* it = new DetailsItem(details_);
        it->setText(0, qs(r->file_name()));
        it->setIcon(0, fileIcon);
        it->setText(1, typeLabel(qs(r->type_string())));
        it->setText(2, QLocale().toString(qulonglong(r->size)));
        it->setData(2, Qt::UserRole, qulonglong(r->size));
        it->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        it->setText(3, QString::number(r->id));
        it->setText(4, r->time ? QDateTime::fromSecsSinceEpoch(r->time).toString("yyyy-MM-dd hh:mm") : QString());
        it->setData(0, KindRole, KindRes);
        it->setData(0, PtrRole, QVariant::fromValue(quintptr(r.get())));
        auto* li = new QListWidgetItem(fileIcon, qs(r->file_name()), icons_);
        li->setData(KindRole, KindRes);
        li->setData(PtrRole, QVariant::fromValue(quintptr(r.get())));
        li->setToolTip(tr("%1\n%2").arg(typeLabel(qs(r->type_string())), util::humanSize(r->size)));
    }
    details_->setSortingEnabled(true);
    details_->sortByColumn(0, Qt::AscendingOrder);
    icons_->sortItems();
    updateActions();
    startThumbnails();
}

void MainWindow::setViewMode(ViewMode m) {
    mode_ = m;
    switch (m) {
        case ViewMode::LargeIcons:
            icons_->setViewMode(QListView::IconMode);
            icons_->setIconSize(QSize(96, 96));
            icons_->setGridSize(QSize(128, 128));
            icons_->setWordWrap(true);
            stack_->setCurrentWidget(icons_);
            aLarge->setChecked(true);
            break;
        case ViewMode::SmallIcons:
            icons_->setViewMode(QListView::IconMode);
            icons_->setIconSize(QSize(32, 32));
            icons_->setGridSize(QSize(110, 64));
            icons_->setWordWrap(true);
            stack_->setCurrentWidget(icons_);
            aSmall->setChecked(true);
            break;
        case ViewMode::List:
            icons_->setViewMode(QListView::ListMode);
            icons_->setFlow(QListView::TopToBottom);
            icons_->setWrapping(true);
            icons_->setIconSize(QSize(16, 16));
            icons_->setGridSize(QSize());
            stack_->setCurrentWidget(icons_);
            aList->setChecked(true);
            break;
        case ViewMode::Details:
            stack_->setCurrentWidget(details_);
            aDetails->setChecked(true);
            break;
    }
    startThumbnails();
}

// DTXShell's job: thumbnails for textures in the icon views, made a few at a
// time so a big folder stays responsive.
void MainWindow::startThumbnails() {
    if (mode_ != ViewMode::LargeIcons && mode_ != ViewMode::SmallIcons) return;
    thumbRow_ = 0;
    int size = mode_ == ViewMode::LargeIcons ? 96 : 32;
    Directory* dir = current_;
    auto step = std::make_shared<std::function<void()>>();
    *step = [this, size, dir, step] {
        if (dir != current_ || (mode_ != ViewMode::LargeIcons && mode_ != ViewMode::SmallIcons)) return;
        if (busy_) {   // a save may be reading the archive on its own thread: wait for it
            QTimer::singleShot(100, this, *step);
            return;
        }
        int done = 0;
        while (thumbRow_ < icons_->count() && done < 8) {
            QListWidgetItem* li = icons_->item(thumbRow_++);
            if (li->data(KindRole).toInt() != KindRes) continue;
            auto* r = reinterpret_cast<Resource*>(li->data(PtrRole).value<quintptr>());
            QImage img;
            if (r->type_string() == "DTX")
                img = dtxThumbnail(readData(r), size, archive_->is_ps2());   // PS2 palettes: alpha as the PS2 draws it
            else if (r->type_string() == "PCX" || r->type_string() == "TGA" || r->type_string() == "BMP")
                img = imageThumbnail(readData(r), qs(r->type_string()), size);
            else if (r->type_string() == "SPR") {
                // its first frame
                QByteArray b = readData(r);
                try {
                    auto s = spr::read(reinterpret_cast<const uint8_t*>(b.constData()), size_t(b.size()));
                    QString found;
                    if (!s.frames.empty()) img = dtxThumbnail(findFrame(QString::fromLatin1(s.frames[0].c_str()), found), size);
                } catch (const std::exception&) {
                }
            }
            else
                continue;
            if (!img.isNull()) li->setIcon(QIcon(QPixmap::fromImage(img)));
            ++done;
        }
        if (thumbRow_ < icons_->count()) QTimer::singleShot(0, this, *step);
    };
    QTimer::singleShot(0, this, *step);
}

MainWindow::Sel MainWindow::selection() const {
    Sel s;
    if (stack_->currentWidget() == details_) {
        for (QTreeWidgetItem* it : details_->selectedItems()) {
            quintptr p = it->data(0, PtrRole).value<quintptr>();
            if (it->data(0, KindRole).toInt() == KindDir) s.dirs.push_back(reinterpret_cast<Directory*>(p));
            else s.res.push_back(reinterpret_cast<Resource*>(p));
        }
    } else {
        for (QListWidgetItem* it : icons_->selectedItems()) {
            quintptr p = it->data(PtrRole).value<quintptr>();
            if (it->data(KindRole).toInt() == KindDir) s.dirs.push_back(reinterpret_cast<Directory*>(p));
            else s.res.push_back(reinterpret_cast<Resource*>(p));
        }
    }
    return s;
}

Directory* MainWindow::targetDir() const { return current_ ? current_ : (archive_ ? &archive_->root() : nullptr); }

// ---------------------------------------------------------------------------
// Opening and previewing resources

QByteArray MainWindow::readData(const Resource* r) const {
    try {
        return util::qb(r->read());
    } catch (const std::exception&) {
        return {};
    }
}

QStringList MainWindow::butesSkins(const Resource* model) const {
    if (!archive_) return {};
    Resource* bf = archive_->root().find_path("ATTRIBUTES\\MODELBUTES.TXT");
    if (!bf) return {};
    if (bf != butesFrom_) {
        // [Model0] / ModelFile = "chars\models\player_action.ltb" / Skin0 = "..."
        butes_.clear();
        butesFrom_ = bf;
        QString text = QString::fromLatin1(readData(bf));
        QString modelFile;
        QStringList skins;
        auto flush = [&] {
            if (!modelFile.isEmpty() && !butes_.count(modelFile)) butes_[modelFile] = skins;
            modelFile.clear();
            skins.clear();
        };
        static const QRegularExpression key(R"re(^\s*([A-Za-z0-9_]+)\s*=\s*(?:"([^"]*)"|(\S+)))re");
        for (QString line : text.split('\n')) {
            int c = line.indexOf("//");
            if (c >= 0) line.truncate(c);
            QString t = line.trimmed();
            if (t.startsWith('[')) {
                flush();
                continue;
            }
            auto m = key.match(line);
            if (!m.hasMatch()) continue;
            QString k = m.captured(1), v = m.captured(2).isNull() ? m.captured(3) : m.captured(2);
            if ((k.compare("ModelFile", Qt::CaseInsensitive) == 0 || k.compare("Filename", Qt::CaseInsensitive) == 0) &&
                modelFile.isEmpty())
                modelFile = v.replace('/', '\\').toLower();
            else if (k.startsWith("Skin", Qt::CaseInsensitive)) {
                bool ok;
                int i = k.mid(4).toInt(&ok);
                if (ok && i >= 0 && i < 16) {
                    while (skins.size() <= i) skins << QString();
                    if (skins[i].isEmpty()) skins[i] = v;
                }
            }
        }
        flush();
    }
    auto it = butes_.find(qs(model->path('\\')).toLower());
    return it == butes_.end() ? QStringList() : it->second;
}

QByteArray MainWindow::findFrame(const QString& path, QString& found) const {
    if (!archive_) return {};
    Resource* r = archive_->root().find_path(ss(path));
    if (!r) return {};
    found = qs(r->path());
    return readData(r);
}

QByteArray MainWindow::findSkin(const Resource* model, int index, int count, QString& path) const {
    // The game's own list first, where the archive carries it.
    QStringList listed = butesSkins(model);
    if (index < listed.size() && !listed[index].isEmpty())
        if (Resource* r = archive_->root().find_path(ss(listed[index]))) {
            path = qs(r->path());
            return readData(r);
        }
    // Skins are chosen by the game's attribute files; guess as WinRez users
    // would: SKINS beside MODELS, <model>.dtx then <model>_*.dtx.
    if (!model->parent()) return {};
    QString stem = qs(model->name);
    std::vector<Directory*> where = {model->parent()};
    for (Directory* p = model->parent(); p; p = p->parent()) {
        if (QString::compare(qs(p->name), "MODELS", Qt::CaseInsensitive) == 0 ||
            QString::compare(qs(p->name), "MODEL", Qt::CaseInsensitive) == 0 || qs(p->name).startsWith("MODELS_", Qt::CaseInsensitive)) {
            if (Directory* gp = p->parent()) {
                QString skins = qs(p->name).toUpper().replace("MODELS", "SKINS").replace("MODEL", "SKINS");
                if (Directory* s = gp->find_dir(ss(skins))) where.push_back(s);
                if (Directory* s = gp->find_dir("SKINS")) where.push_back(s);
            }
        }
        // AvP2: Models/Characters <-> Skins/Characters
        if (p->parent() && QString::compare(qs(p->parent()->name), "MODELS", Qt::CaseInsensitive) == 0) {
            if (Directory* gp = p->parent()->parent())
                if (Directory* s = gp->find_dir("SKINS"))
                    if (Directory* t = s->find_dir(p->name)) where.push_back(t);
        }
    }
    std::vector<Resource*> exact, others;
    uint32_t dtx = rez::type_from_string("DTX");
    auto collect = [&](const QString& st) {
        for (Directory* d : where)
            for (auto& r : d->resources()) {
                if (r->type != dtx) continue;
                QString n = qs(r->name);
                if (n.compare(st, Qt::CaseInsensitive) == 0) {
                    if (std::find(exact.begin(), exact.end(), r.get()) == exact.end()) exact.push_back(r.get());
                } else if (n.startsWith(st, Qt::CaseInsensitive) && (n.size() > st.size()) &&
                           (n[st.size()] == '_' || n[st.size()].isDigit())) {
                    if (std::find(others.begin(), others.end(), r.get()) == others.end()) others.push_back(r.get());
                }
            }
    };
    collect(stem);
    if (exact.empty() && others.empty() && model->type == rez::type_from_string("ABC")) {
        // NOLF adds cinematic extensions to models, independently of skins.
        static const QRegularExpression cinematic("[MT][0-9]+$", QRegularExpression::CaseInsensitiveOption);
        auto m = cinematic.match(stem);
        if (m.hasMatch() && m.capturedStart() > 0) collect(stem.left(m.capturedStart()));
    }
    std::sort(others.begin(), others.end(), [](Resource* a, Resource* b) { return a->name < b->name; });
    exact.insert(exact.end(), others.begin(), others.end());
    (void)count;
    if (index < int(exact.size())) {
        Resource* r = exact[size_t(index)];
        path = qs(r->path());
        return readData(r);
    }
    return {};
}

void MainWindow::showResource(const QString& path) {
    if (!archive_) return;
    Resource* r = archive_->root().find_path(ss(path));
    if (!r) return;
    {
        // Finish navigation before selection handlers update the preview.
        QSignalBlocker b1(details_), b2(icons_);
        showDir(r->parent());
        for (int i = 0; i < details_->topLevelItemCount(); ++i) {
            auto* item = details_->topLevelItem(i);
            if (item->data(0, PtrRole).value<quintptr>() == quintptr(r)) {
                details_->setCurrentItem(item, 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                details_->scrollToItem(item);
            }
        }
        for (int i = 0; i < icons_->count(); ++i) {
            auto* item = icons_->item(i);
            if (item->data(PtrRole).value<quintptr>() == quintptr(r)) {
                icons_->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
                icons_->scrollToItem(item);
            }
        }
    }
    preview(r);
    stack_->currentWidget()->setFocus(Qt::OtherFocusReason);
    updateActions();
    status_->setText(tr("%1 selected (%2)").arg(1).arg(util::humanSize(r->size)));
}

void MainWindow::clearPreview() {
    previewStack_->setCurrentWidget(infoPreview_);
    infoPreview_->clear();
    texPreview_->clear();
    modelPreview_->clear();
    soundPreview_->clear();
    bankPreview_->clear();
    spritePreview_->clear();
    textPreview_->clear();
}

void MainWindow::preview(Resource* r) {
    if (!aPreview->isChecked() || !r) return;
    QString t = qs(r->type_string()).toUpper();
    QString err;
    if (t == "DTX") {
        texPreview_->setPs2Alpha(archive_ && archive_->is_ps2());
        if (texPreview_->loadDtx(readData(r), qs(r->file_name()), &err)) {
            previewStack_->setCurrentWidget(texPreview_);
            return;
        }
    } else if (t == "PCX" || t == "TGA" || t == "BMP") {
        if (texPreview_->loadImage(readData(r), qs(r->file_name()), &err)) {
            previewStack_->setCurrentWidget(texPreview_);
            return;
        }
    } else if (t == "ABC" || t == "LTB") {
        if (modelPreview_->loadAbc(readData(r), qs(r->file_name()),
                                   [this, r](int i, int n, QString& path) { return findSkin(r, i, n, path); }, &err)) {
            previewStack_->setCurrentWidget(modelPreview_);
            return;
        }
    } else if (t == "SPR") {
        if (spritePreview_->loadSprite(readData(r), qs(r->file_name()),
                                       [this](const QString& p, QString& found) { return findFrame(p, found); }, &err)) {
            previewStack_->setCurrentWidget(spritePreview_);
            return;
        }
    } else if (t == "WAV") {
        previewStack_->setCurrentWidget(soundPreview_);   // first, so the last sound stops
        if (soundPreview_->loadWav(readData(r), qs(r->file_name()), &err)) return;
    } else if (t == "LPS") {
        previewStack_->setCurrentWidget(bankPreview_);
        if (bankPreview_->loadBank(readData(r), qs(r->file_name()), &err)) return;
    } else if (t == "SGT" || t == "STY" || t == "DLS") {
        err = tr("DirectMusic %1: LithRez Studio can't play DirectMusic yet.")
                  .arg(t == "SGT" ? tr("segment") : t == "STY" ? tr("style") : tr("instrument collection"));
    } else if (t == "BUT" && [&] {
                   // the PS2's compiled attribute files, as ButeMgr text
                   QByteArray b = readData(r);
                   auto* p = reinterpret_cast<const uint8_t*>(b.constData());
                   if (!lithtech::butes::looks_like(p, size_t(b.size()))) return false;
                   textPreview_->setPlainText(qs(lithtech::butes::to_text(lithtech::butes::read(p, size_t(b.size())))));
                   previewStack_->setCurrentWidget(textPreview_);
                   return true;
               }()) {
        return;
    } else if (isTextType(t) || r->size < 64 * 1024) {
        QByteArray b = readData(r).left(256 * 1024);
        bool text = !b.isEmpty() && std::count_if(b.begin(), b.end(), [](char c) {
                        unsigned char u = uchar(c);
                        return u < 9 || (u > 13 && u < 32);
                    }) < b.size() / 50 + 1;
        if (text || isTextType(t)) {
            textPreview_->setPlainText(QString::fromLatin1(b));
            previewStack_->setCurrentWidget(textPreview_);
            return;
        }
        // hex
        QString hex;
        for (int i = 0; i < std::min<int>(b.size(), 4096); i += 16) {
            QString line = QStringLiteral("%1  ").arg(i, 8, 16, QChar('0'));
            QString asc;
            for (int j = 0; j < 16; ++j) {
                if (i + j < b.size()) {
                    uchar c = uchar(b[i + j]);
                    line += QStringLiteral("%1 ").arg(c, 2, 16, QChar('0'));
                    asc += (c >= 32 && c < 127) ? QChar(c) : QChar('.');
                } else {
                    line += "   ";
                }
            }
            hex += line + " " + asc + "\n";
        }
        textPreview_->setPlainText(hex);
        previewStack_->setCurrentWidget(textPreview_);
        return;
    }
    infoPreview_->setText(err.isEmpty() ? tr("%1\n%2\n%3").arg(qs(r->file_name()), typeLabel(t), util::humanSize(r->size))
                                        : tr("%1\n\n%2").arg(qs(r->file_name()), err));
    previewStack_->setCurrentWidget(infoPreview_);
}

QString MainWindow::tempExtract(const Resource* r) const {
    if (!tempDir().isValid()) return {};
    QString dir = tempDir().path() + "/" + QString::number(quintptr(r), 16);
    QDir().mkpath(dir);
    rez::Report rep;
    if (!rez::extract_resource(*r, ss(dir), rep)) return {};
    return dir + "/" + qs(r->file_name());
}

void MainWindow::openSound(const QByteArray& data, const QString& name) {
    if (lithtech::sound::is_bank(reinterpret_cast<const uint8_t*>(data.constData()), size_t(data.size()))) {
        auto* b = new BankView;
        b->setAttribute(Qt::WA_DeleteOnClose);
        b->setWindowTitle(name + " - LithRez Studio");
        QString err;
        if (!b->loadBank(data, name, &err)) {
            QMessageBox::warning(this, tr("Sound Player"), err);
            delete b;
            return;
        }
        b->resize(760, 680);
        b->show();
        return;
    }
    auto* v = new SoundView;
    v->setAttribute(Qt::WA_DeleteOnClose);
    v->setWindowTitle(name + " - LithRez Studio");
    QString err;
    if (!v->loadWav(data, name, &err)) {
        QMessageBox::warning(this, tr("Sound Player"), err);
        delete v;
        return;
    }
    v->resize(700, 420);
    v->show();
}

void MainWindow::openResource(Resource* r) {
    QString t = qs(r->type_string()).toUpper();
    if (t == "DTX") {
        auto* w = new TextureWindow;
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->view()->setPs2Alpha(archive_ && archive_->is_ps2());
        w->openData(readData(r), qs(r->file_name()));
        w->show();
        return;
    }
    if (t == "ABC" || t == "LTB") {
        auto* v = new ModelView;
        v->setAttribute(Qt::WA_DeleteOnClose);
        connect(v, &ModelView::skinActivated, this, &MainWindow::showResource);
        v->setWindowTitle(qs(r->file_name()) + " - LithRez Studio");
        QString err;
        if (!v->loadAbc(readData(r), qs(r->file_name()), [this, r](int i, int n, QString& path) { return findSkin(r, i, n, path); }, &err)) {
            QMessageBox::warning(this, tr("Model"), err);
            delete v;
            return;
        }
        v->resize(900, 700);
        v->show();
        return;
    }
    if (t == "SPR") {
        auto* v = new SpriteView;
        v->setAttribute(Qt::WA_DeleteOnClose);
        connect(v, &SpriteView::frameActivated, this, &MainWindow::showResource);
        v->setWindowTitle(qs(r->file_name()) + " - LithRez Studio");
        QString err;
        if (!v->loadSprite(readData(r), qs(r->file_name()), [this](const QString& p, QString& found) { return findFrame(p, found); },
                           &err)) {
            QMessageBox::warning(this, tr("Sprite"), err);
            delete v;
            return;
        }
        v->resize(700, 700);
        v->show();
        return;
    }
    if (t == "WAV" || t == "LPS") {
        openSound(readData(r), qs(r->file_name()));
        return;
    }
    if (t == "DLL") {
        auto* w = new StringTableWindow;
        w->setAttribute(Qt::WA_DeleteOnClose);
        QPointer<MainWindow> self(this);
        if (w->openData(readData(r), qs(r->file_name()), [self, r](const QByteArray& b) {
                if (!self) return false;
                r->source = rez::DataSource{};
                r->source.kind = rez::DataSource::Kind::Memory;
                r->source.bytes = std::make_shared<const std::vector<uint8_t>>(util::vb(b));
                r->size = uint32_t(b.size());
                r->time = rez::current_time(std::nullopt);
                r->data_order = self->archive_->next_data_order();
                self->setModified();
                self->refreshContents();
                return true;
            })) {
            w->show();
            return;
        }
        delete w;
    }
    // anything else: hand it to the system
    QString f = tempExtract(r);
    if (!f.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(f));
}

void MainWindow::onActivated(Directory* d, Resource* r) {
    if (d) showDir(d);
    else if (r) openResource(r);
}

// ---------------------------------------------------------------------------
// File menu

void MainWindow::doOpen() {
    if (!maybeSave()) return;
    QString f = QFileDialog::getOpenFileName(this, tr("Open Rez File"), QSettings().value("lastDir").toString(),
                                             tr("Rez files and PS2 discs (*.rez *.REZ *.iso *.ISO);;Rez files (*.rez *.REZ);;"
                                                "PS2 disc images (*.iso *.ISO);;All files (*)"));
    if (f.isEmpty()) return;
    QSettings().setValue("lastDir", QFileInfo(f).absolutePath());
    openRez(f);
}

void MainWindow::doClose() {
    if (maybeSave()) newRez();
}

bool MainWindow::saveTo(const QString& path) {
    if (archive_->is_ps2()) {
        bool inPlace = QFileInfo(path) == QFileInfo(path_);
        auto r = QMessageBox::warning(
            this, tr("Save PS2 archive"),
            tr("Saving changes to a PS2 archive is experimental: it follows what the PS2's reader requires, but it "
               "hasn't been tried on a PS2 or an emulator.%1\n\nSave anyway?")
                .arg(inPlace ? tr("\n\n%1 will be changed in place; keep a copy of the original.").arg(QFileInfo(path).fileName())
                             : QString()),
            QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel);
        if (r != QMessageBox::Save) return false;
    }
    try {
        // Progress: the bytes of resource data written; the directory (or a
        // PS2 archive's patching) comes last, as a busy bar, and can't stop.
        rez::SaveOptions so;
        runBusy(tr("Saving %1...").arg(QFileInfo(path).fileName()), true, [&](Busy& busy) {
            so.progress = [&](uint64_t done, uint64_t total) {
                busy.set(done, total, total ? tr("%1 of %2").arg(util::humanSize(done), util::humanSize(total))
                                            : tr("Writing the directory..."));
                return !busy.cancel.load();
            };
            archive_->save(ss(path), so);
        });
    } catch (const rez::Cancelled&) {
        return false;
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Save"), tr("Can't save %1:\n%2").arg(path, qs(e.what())));
        return false;
    }
    path_ = path;
    modified_ = false;
    updateTitle();
    updateActions();
    addRecent(path);
    if (QTreeWidgetItem* root = tree_->topLevelItem(0)) root->setText(0, QFileInfo(path).fileName());
    return true;
}

bool MainWindow::doSave() {
    if (path_.isEmpty()) return doSaveAs();
    return saveTo(path_);
}

bool MainWindow::doSaveAs() {
    // a PS2 archive is saved as what it is: a disc image, or the archive alone
    bool iso = archive_->is_ps2() && archive_->ps2->where.in_iso;
    QString f = QFileDialog::getSaveFileName(this, tr("Save Rez File"),
                                             path_.isEmpty() ? QSettings().value("lastDir").toString() : path_,
                                             iso ? tr("PS2 disc images (*.iso)") : tr("Rez files (*.rez)"));
    if (f.isEmpty()) return false;
    if (QFileInfo(f).suffix().isEmpty()) f += iso ? ".iso" : ".rez";
    QSettings().setValue("lastDir", QFileInfo(f).absolutePath());
    return saveTo(f);
}

rez::CreateOptions MainWindow::createOptions() const {
    QSettings s;
    rez::CreateOptions co;
    co.lower_case_ok = s.value("lowerCaseOk", false).toBool();
    co.zero_ids = s.value("compat", "1.00").toString() != "1.00";
    co.user_title = ss(s.value("userTitle", qs(std::string(rez::kLithTechUserTitle))).toString());
    return co;
}

void MainWindow::doNewProject() {
    if (!maybeSave()) return;
    QString dir = QFileDialog::getExistingDirectory(this, tr("Rez Project: choose the project's root folder"),
                                                    QSettings().value("lastDir").toString());
    if (dir.isEmpty()) return;
    rez::Report rep;
    QStringList log;
    rep.out = [&log](const std::string& s) { log << qs(s).trimmed(); };
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto a = rez::create_from_directory(ss(dir), createOptions(), rep);
    QApplication::restoreOverrideCursor();
    QSignalBlocker b1(details_), b2(icons_);
    clearPreview();
    butesFrom_ = nullptr;
    butes_.clear();
    archive_ = std::move(a);
    path_.clear();
    current_ = &archive_->root();
    back_.clear();
    forward_.clear();
    rebuildTree();
    setModified();
    if (rep.errors || rep.warnings)
        QMessageBox::warning(this, tr("Rez Project"), tr("%1 error(s), %2 warning(s):\n\n%3").arg(rep.errors).arg(rep.warnings).arg(log.filter(QRegularExpression("^(ERROR|WARNING)")).join("\n")));
    // the same as "lithrez c": save straight away under a name
    doSaveAs();
}

void MainWindow::doWizard() {
    RezWizard w(this);
    if (w.exec() != QDialog::Accepted) return;
    const RezWizard::Result& res = w.result();
    switch (res.action) {
        case RezWizard::AddFiles:
        case RezWizard::AddFolders:
            if (!maybeSave()) return;
            newRez();
            insertPaths(res.paths, &archive_->root());
            setModified();
            doSaveAs();
            break;
        case RezWizard::ProjectFolder:
            if (!maybeSave()) return;
            newRez();
            {
                rez::Report rep;
                archive_ = rez::create_from_directory(ss(res.paths.value(0)), createOptions(), rep);
                current_ = &archive_->root();
                rebuildTree();
                setModified();
            }
            doSaveAs();
            break;
        case RezWizard::GameLayout:
            if (!maybeSave()) return;
            newRez();
            for (const auto& [folder, files] : res.placed) {
                Directory* d = &archive_->root();
                for (const QString& part : folder.split('\\', Qt::SkipEmptyParts)) {
                    std::string p = ss(part.toUpper());
                    Directory* n = d->find_dir(p);
                    d = n ? n : d->add_dir(p, rez::current_time(std::nullopt));
                }
                insertPaths(files, d);
            }
            rebuildTree();
            setModified();
            doSaveAs();
            break;
        case RezWizard::ExtractOpen:
            doExtract(true);
            break;
    }
}

void MainWindow::importArchive(Archive& from, Directory& src, Directory& dst, int& count) {
    for (auto& s : src.dirs()) {
        Directory* d = dst.find_dir(s->name);
        if (!d) d = dst.add_dir(s->name, s->time);
        importArchive(from, *s, *d, count);
    }
    for (auto& r : src.resources()) {
        if (Resource* old = dst.find(r->name, r->type)) dst.take(old);
        auto c = Resource::copy_of(*r);
        c->data_order = archive_->next_data_order();
        dst.add(std::move(c));
        ++count;
    }
}

void MainWindow::doImport() {
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Import Rez Files"), QSettings().value("lastDir").toString(),
                                                      tr("Rez files (*.rez *.REZ)"));
    if (files.isEmpty()) return;
    int count = 0;
    std::vector<std::unique_ptr<Archive>> keep;
    for (const QString& f : files) {
        try {
            auto other = openArchive(f);
            importArchive(*other, other->root(), *targetDir(), count);
        } catch (const rez::Cancelled&) {
            break;
        } catch (const std::exception& e) {
            QMessageBox::warning(this, tr("Import"), tr("Can't read %1:\n%2").arg(f, qs(e.what())));
        }
    }
    rebuildTree();
    setModified();
    status_->setText(tr("Imported %1 files").arg(count));
}

void MainWindow::doMerge() {
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Merge: choose the rez files, in load order"),
                                                      QSettings().value("lastDir").toString(), tr("Rez files (*.rez *.REZ)"));
    if (files.size() < 2) return;
    if (!maybeSave()) return;
    newRez();
    int count = 0;
    for (const QString& f : files) {
        try {
            auto other = openArchive(f);
            importArchive(*other, other->root(), archive_->root(), count);
        } catch (const rez::Cancelled&) {
            return;
        } catch (const std::exception& e) {
            QMessageBox::warning(this, tr("Merge"), tr("Can't read %1:\n%2").arg(f, qs(e.what())));
            return;
        }
    }
    rebuildTree();
    setModified();
    QMessageBox::information(this, tr("Merge"),
                             tr("Merged %1 files from %2 rez files; later files replace earlier ones. Save the result.").arg(count).arg(files.size()));
    doSaveAs();
}

void MainWindow::doInfo() {
    if (!archive_) return;
    if (archive_->is_ps2()) {
        const auto& p = *archive_->ps2;
        std::size_t named = 0;
        for (auto& e : p.tables[0])
            if (lithtech::ps2rez::Names::global().find(e.hash, p.header.weight)) ++named;
        QString text = tr("<b>%1</b><br>PS2 LITH_PS2.REZ%2<br><br>"
                          "Version %3<br>%4 files in each of %5 tables (room for %6)<br>"
                          "Data from sector %7; hash weight %8<br>%9 of the files named<br><br>"
                          "Editing is experimental: see docs/PS2REZ.md.")
                           .arg(path_.toHtmlEscaped(),
                                p.where.in_iso ? tr(", in a disc image at sector %1").arg(p.where.lsn) : QString())
                           .arg(p.header.version)
                           .arg(p.header.entries)
                           .arg(p.header.tables)
                           .arg(p.capacity())
                           .arg(p.header.data_sector)
                           .arg(p.header.weight)
                           .arg(named);
        QMessageBox::information(this, tr("Rez File Information"), text);
        return;
    }
    const auto& h = archive_->header;
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Rez File Information"));
    auto* form = new QFormLayout(&dlg);
    auto row = [&](const QString& k, const QString& v) {
        auto* l = new QLabel(v, &dlg);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(k, l);
    };
    row(tr("File"), path_.isEmpty() ? tr("(not saved)") : path_);
    row(tr("File type"), qs(h.file_type));
    auto* title = new QLineEdit(qs(h.user_title), &dlg);
    title->setMaxLength(int(rez::kTitleSize));
    form->addRow(tr("User title"), title);
    row(tr("Format version"), QString::number(h.version));
    row(tr("Sorted"), h.sorted ? tr("yes") : tr("no"));
    row(tr("Last modified"), h.time ? QDateTime::fromSecsSinceEpoch(h.time).toString() : tr("-"));
    row(tr("Root directory"), tr("at %1, %2 bytes").arg(h.root_dir_pos).arg(h.root_dir_size));
    row(tr("Largest names"), tr("directory %1, resource %2, comment %3, keys %4").arg(h.largest_dir_name).arg(h.largest_rez_name).arg(h.largest_comment).arg(h.largest_key_array));
    row(tr("Contents"), tr("%1 folders, %2 files").arg(archive_->root().count_dirs()).arg(archive_->root().count_resources()));
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() == QDialog::Accepted && ss(title->text()) != h.user_title) {
        archive_->header.user_title = ss(title->text());
        setModified();
    }
}

void MainWindow::doSettings() {
    QSettings s;
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Settings"));
    auto* form = new QFormLayout(&dlg);
    auto* lower = new QCheckBox(tr("Keep the case of names (LithRez option l)"), &dlg);
    lower->setChecked(s.value("lowerCaseOk", false).toBool());
    form->addRow(lower);
    auto* compat = new QComboBox(&dlg);
    compat->addItem(tr("LithRez 1.00 (NOLF, AvP2): number resources"), "1.00");
    compat->addItem(tr("LithRez 1.02 / 1.10 (NOLF2, Tron, Contract J.A.C.K.): IDs 0"), "1.10");
    compat->setCurrentIndex(s.value("compat", "1.00").toString() == "1.00" ? 0 : 1);
    form->addRow(tr("Compatibility"), compat);
    auto* title = new QLineEdit(s.value("userTitle", qs(std::string(rez::kLithTechUserTitle))).toString(), &dlg);
    title->setMaxLength(int(rez::kTitleSize));
    form->addRow(tr("Title for new files"), title);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;
    s.setValue("lowerCaseOk", lower->isChecked());
    s.setValue("compat", compat->currentData());
    s.setValue("userTitle", title->text());
}

void MainWindow::doAbout() {
    QMessageBox::about(this, tr("About LithRez Studio"),
                       tr("<h3>LithRez Studio %1</h3><p>Reads, edits and creates LithTech resource files (.rez), "
                          "with a texture viewer, a model preview and a string table editor.</p>"
                          "<p>Part of lithtools. Cross-platform successor to WinRez LT Studio's tools; "
                          "its files match LithRez's output byte for byte.</p>")
                           .arg(QApplication::applicationVersion()));
}

void MainWindow::addRecent(const QString& path) {
    QSettings s;
    QStringList r = s.value("recent").toStringList();
    r.removeAll(path);
    r.prepend(path);
    while (r.size() > kRecentMax) r.removeLast();
    s.setValue("recent", r);
    updateRecent();
}

void MainWindow::updateRecent() {
    QStringList r = QSettings().value("recent").toStringList();
    for (int i = 0; i < kRecentMax; ++i) {
        if (i < r.size()) {
            recentActions_[i]->setText(QStringLiteral("&%1 %2").arg(i + 1).arg(QFileInfo(r[i]).fileName()));
            recentActions_[i]->setData(r[i]);
            recentActions_[i]->setToolTip(r[i]);
            recentActions_[i]->setVisible(true);
        } else {
            recentActions_[i]->setVisible(false);
        }
    }
    recentMenu_->setEnabled(!r.isEmpty());
}

// ---------------------------------------------------------------------------
// Object menu

bool MainWindow::extractAllTo(const QString& dir) {
    rez::Report rep;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    rez::extract(archive_->root(), ss(dir), rep, false);
    QApplication::restoreOverrideCursor();
    return rep.errors == 0;
}

void MainWindow::doExtract(bool all) {
    if (!archive_) return;
    Sel s = selection();
    if (all || s.empty()) {
        s = {};
        if (all) s.dirs.push_back(&archive_->root());
        else if (current_) s.dirs.push_back(current_);
    }
    QString dir = QFileDialog::getExistingDirectory(this, tr("Extract to"), QSettings().value("extractDir").toString());
    if (dir.isEmpty()) return;
    QSettings().setValue("extractDir", dir);
    std::size_t total = s.res.size();
    for (Directory* d : s.dirs) total += d->count_resources();
    QProgressDialog prog(tr("Extracting..."), tr("Cancel"), 0, int(total), this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(300);
    rez::Report rep;
    QStringList errors;
    rep.out = [&errors](const std::string& m) { errors << qs(m).trimmed(); };
    int done = 0;
    std::function<void(Directory&, const QString&)> walk = [&](Directory& d, const QString& to) {
        QDir().mkpath(to);
        for (auto& r : d.resources()) {
            if (prog.wasCanceled()) return;
            rez::extract_resource(*r, ss(to), rep);
            prog.setValue(++done);
        }
        for (auto& sd : d.dirs()) walk(*sd, to + "/" + qs(sd->name));
    };
    for (Resource* r : s.res) {
        rez::extract_resource(*r, ss(dir), rep);
        prog.setValue(++done);
    }
    for (Directory* d : s.dirs) walk(*d, d->parent() ? dir + "/" + qs(d->name) : dir);
    prog.setValue(int(total));
    if (rep.errors) QMessageBox::warning(this, tr("Extract"), tr("%1 error(s):\n%2").arg(rep.errors).arg(errors.mid(0, 20).join("\n")));
    else status_->setText(tr("Extracted %1 files to %2").arg(done).arg(dir));
}

int MainWindow::insertPaths(const QStringList& paths, Directory* into) {
    rez::Report rep;
    QStringList log;
    rep.out = [&log](const std::string& m) { log << qs(m).trimmed(); };
    auto co = createOptions();
    int n = 0;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    for (const QString& p : paths) {
        QFileInfo fi(p);
        if (fi.isDir()) {
            std::string name = ss(co.lower_case_ok ? fi.fileName() : fi.fileName().toUpper());
            Directory* d = into->find_dir(name);
            if (!d) d = into->add_dir(name, rez::current_time(std::nullopt));
            n += rez::add_from_directory(*archive_, *d, ss(p), co, rep);
        } else if (rez::add_file(*archive_, *into, ss(p), co, rep)) {
            ++n;
        }
    }
    QApplication::restoreOverrideCursor();
    if (rep.errors || rep.warnings)
        QMessageBox::warning(this, tr("Insert"), tr("%1 error(s), %2 warning(s):\n%3").arg(rep.errors).arg(rep.warnings).arg(log.mid(0, 20).join("\n")));
    return n;
}

void MainWindow::addPaths(const QStringList& paths) {
    int n = insertPaths(paths, &archive_->root());
    rebuildTree();
    setModified();
    status_->setText(tr("Added %1 files").arg(n));
}

void MainWindow::doInsertFiles() {
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Insert Files into %1").arg("\\" + qs(targetDir()->path())),
                                                      QSettings().value("insertDir").toString());
    if (files.isEmpty()) return;
    QSettings().setValue("insertDir", QFileInfo(files[0]).absolutePath());
    int n = insertPaths(files, targetDir());
    refreshContents();
    setModified();
    status_->setText(tr("Inserted %1 files").arg(n));
}

void MainWindow::doReplace() {
    Sel s = selection();
    if (!archive_ || s.res.size() != 1 || !s.dirs.empty()) return;
    Resource* r = s.res[0];
    QString path = QFileDialog::getOpenFileName(this, tr("Replace %1 with").arg(qs(r->file_name())),
                                              QSettings().value("insertDir").toString());
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Replace"), tr("Couldn't read %1:\n%2").arg(path, file.errorString()));
        return;
    }
    if (file.size() > std::numeric_limits<uint32_t>::max()) {
        QMessageBox::warning(this, tr("Replace"), tr("This file is too large for a rez resource."));
        return;
    }
    QByteArray bytes = file.readAll();
    if (file.error() != QFile::NoError) {
        QMessageBox::warning(this, tr("Replace"), tr("Couldn't read %1:\n%2").arg(path, file.errorString()));
        return;
    }
    r->source = rez::DataSource{};
    r->source.kind = rez::DataSource::Kind::Memory;
    r->source.bytes = std::make_shared<const std::vector<uint8_t>>(util::vb(bytes));
    r->size = uint32_t(bytes.size());
    r->time = rez::current_time(std::nullopt);
    r->data_order = archive_->next_data_order();
    if (r == butesFrom_) butesFrom_ = nullptr;
    QSettings().setValue("insertDir", QFileInfo(path).absolutePath());
    setModified();
    showResource(qs(r->path()));
    status_->setText(tr("Replaced %1; save the rez file to keep the change.").arg(qs(r->file_name())));
}

void MainWindow::doInsertFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Insert Folder into %1").arg("\\" + qs(targetDir()->path())),
                                                    QSettings().value("insertDir").toString());
    if (dir.isEmpty()) return;
    QSettings().setValue("insertDir", QFileInfo(dir).absolutePath());
    int n = insertPaths({dir}, targetDir());
    rebuildTree();
    setModified();
    status_->setText(tr("Inserted %1 files").arg(n));
}

void MainWindow::doInsertEmptyFolder() {
    bool ok;
    QString name = QInputDialog::getText(this, tr("Insert Empty Folder"), tr("Folder name:"), QLineEdit::Normal, tr("NEWFOLDER"), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    if (name.contains('\\') || name.contains('/')) {
        QMessageBox::warning(this, tr("Insert Empty Folder"), tr("A folder name can't contain slashes."));
        return;
    }
    std::string n = ss(createOptions().lower_case_ok ? name.trimmed() : name.trimmed().toUpper());
    if (targetDir()->find_dir(n)) {
        QMessageBox::warning(this, tr("Insert Empty Folder"), tr("%1 already exists.").arg(qs(n)));
        return;
    }
    targetDir()->add_dir(n, rez::current_time(std::nullopt));
    rebuildTree();
    setModified();
}

void MainWindow::doDelete() {
    Sel s = selection();
    if (s.empty()) return;
    if (QMessageBox::question(this, tr("Delete"), tr("Delete %1 item(s) from the rez file?").arg(s.dirs.size() + s.res.size())) != QMessageBox::Yes)
        return;
    for (Resource* r : s.res) r->parent()->take(r);
    for (Directory* d : s.dirs) {
        if (d == current_) current_ = d->parent();
        // drop it from the history too
        auto inside = [d](Directory* x) {
            for (; x; x = x->parent())
                if (x == d) return true;
            return false;
        };
        back_.erase(std::remove_if(back_.begin(), back_.end(), inside), back_.end());
        forward_.erase(std::remove_if(forward_.begin(), forward_.end(), inside), forward_.end());
        if (d->parent()) d->parent()->take_dir(d);
    }
    clipboard_ = {};
    rebuildTree();
    setModified();
}

void MainWindow::doRename() {
    Sel s = selection();
    if (s.res.size() + s.dirs.size() != 1) return;
    bool ok;
    if (!s.dirs.empty()) {
        Directory* d = s.dirs[0];
        QString name = QInputDialog::getText(this, tr("Rename Folder"), tr("New name:"), QLineEdit::Normal, qs(d->name), &ok);
        if (!ok || name.isEmpty() || name.contains('\\') || name.contains('/')) return;
        if (Directory* other = d->parent()->find_dir(ss(name)); other && other != d) {
            QMessageBox::warning(this, tr("Rename"), tr("%1 already exists.").arg(name));
            return;
        }
        d->name = ss(name);
    } else {
        Resource* r = s.res[0];
        QString name = QInputDialog::getText(this, tr("Rename File"), tr("New name (NAME.EXT):"), QLineEdit::Normal, qs(r->file_name()), &ok);
        if (!ok || name.isEmpty() || name.contains('\\') || name.contains('/')) return;
        int dot = name.lastIndexOf('.');
        QString base = dot >= 0 && name.size() - dot - 1 <= 4 ? name.left(dot) : name;
        QString ext = dot >= 0 && name.size() - dot - 1 <= 4 ? name.mid(dot + 1).toUpper() : QString();
        uint32_t type = rez::type_from_string(ss(ext));
        if (Resource* other = r->parent()->find(ss(base), type); other && other != r) {
            QMessageBox::warning(this, tr("Rename"), tr("%1 already exists.").arg(name));
            return;
        }
        // re-add so RezMgr's per-type bookkeeping sees the new type
        Directory* p = r->parent();
        auto owned = p->take(r);
        owned->name = ss(base);
        owned->type = type;
        p->add(std::move(owned));
    }
    rebuildTree();
    setModified();
}

void MainWindow::doCut() {
    clipboard_ = selection();
    status_->setText(tr("%1 item(s) ready to move; choose a folder and Paste").arg(clipboard_.dirs.size() + clipboard_.res.size()));
    updateActions();
}

void MainWindow::doPaste() {
    if (clipboard_.empty()) return;
    Directory* to = targetDir();
    // A folder can't move into itself.
    for (Directory* d : clipboard_.dirs)
        for (Directory* x = to; x; x = x->parent())
            if (x == d) {
                QMessageBox::warning(this, tr("Paste"), tr("Can't move a folder into itself."));
                return;
            }
    for (Resource* r : clipboard_.res) {
        if (r->parent() == to) continue;
        if (Resource* old = to->find(r->name, r->type)) {
            if (QMessageBox::question(this, tr("Paste"), tr("Replace %1?").arg(qs(old->file_name()))) != QMessageBox::Yes) continue;
            to->take(old);
        }
        auto owned = r->parent()->take(r);
        to->add(std::move(owned));
    }
    for (Directory* d : clipboard_.dirs) {
        if (d->parent() == to) continue;
        if (to->find_dir(d->name)) {
            QMessageBox::warning(this, tr("Paste"), tr("%1 already exists here.").arg(qs(d->name)));
            continue;
        }
        auto owned = d->parent()->take_dir(d);
        to->adopt(std::move(owned));
    }
    clipboard_ = {};
    rebuildTree();
    setModified();
}

void MainWindow::doSelectAll() {
    if (stack_->currentWidget() == details_) details_->selectAll();
    else icons_->selectAll();
}

void MainWindow::doBack() {
    if (back_.empty()) return;
    forward_.push_back(current_);
    Directory* d = back_.back();
    back_.pop_back();
    showDir(d, false);
}

void MainWindow::doForward() {
    if (forward_.empty()) return;
    back_.push_back(current_);
    Directory* d = forward_.back();
    forward_.pop_back();
    showDir(d, false);
}

void MainWindow::doUp() {
    if (current_ && current_->parent()) showDir(current_->parent());
}

void MainWindow::doGo() {
    if (!archive_) return;
    QString p = location_->currentText().trimmed();
    if (Directory* d = archive_->root().find_dir_path(ss(p))) {
        showDir(d);
    } else if (Resource* r = archive_->root().find_path(ss(p))) {
        showDir(r->parent());
        preview(r);
    } else {
        QMessageBox::information(this, tr("Location"), tr("%1 isn't in this rez file.").arg(p));
    }
}

// ---------------------------------------------------------------------------
// Drag and drop

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    QStringList paths;
    for (const QUrl& u : e->mimeData()->urls())
        if (u.isLocalFile()) paths << u.toLocalFile();
    if (paths.isEmpty()) return;
    // A single .rez dropped on the window opens it; anything else is inserted.
    if (paths.size() == 1 && (paths[0].endsWith(".rez", Qt::CaseInsensitive) || paths[0].endsWith(".iso", Qt::CaseInsensitive)) &&
        Archive::probe(ss(paths[0]))) {
        if (maybeSave()) openRez(paths[0]);
        return;
    }
    int n = insertPaths(paths, targetDir());
    rebuildTree();
    setModified();
    status_->setText(tr("Inserted %1 files").arg(n));
    e->acceptProposedAction();
}

bool MainWindow::eventFilter(QObject* o, QEvent* e) {
    // Drops from the file manager onto the views insert; drags out of them
    // extract to a temporary folder and hand the files over.
    if (e->type() == QEvent::DragEnter || e->type() == QEvent::DragMove) {
        auto* de = static_cast<QDragMoveEvent*>(e);
        if (de->mimeData()->hasUrls() && de->source() == nullptr) {
            de->acceptProposedAction();
            return true;
        }
    }
    if (e->type() == QEvent::Drop) {
        auto* de = static_cast<QDropEvent*>(e);
        if (de->mimeData()->hasUrls() && de->source() == nullptr) {
            Directory* into = targetDir();
            if (o == tree_->viewport()) {
                if (QTreeWidgetItem* it = tree_->itemAt(de->position().toPoint()))
                    into = reinterpret_cast<Directory*>(it->data(0, PtrRole).value<quintptr>());
            }
            QStringList paths;
            for (const QUrl& u : de->mimeData()->urls())
                if (u.isLocalFile()) paths << u.toLocalFile();
            int n = insertPaths(paths, into);
            rebuildTree();
            setModified();
            status_->setText(tr("Inserted %1 files").arg(n));
            de->acceptProposedAction();
            return true;
        }
    }
    if (e->type() == QEvent::MouseButtonPress && (o == details_->viewport() || o == icons_->viewport())) {
        auto* me = static_cast<QMouseEvent*>(e);
        dragStart_ = me->position().toPoint();
        dragArmed_ = me->button() == Qt::LeftButton &&
                     (o == details_->viewport() ? details_->itemAt(dragStart_) != nullptr
                                                : icons_->itemAt(dragStart_) != nullptr);
    }
    if (e->type() == QEvent::MouseMove && dragArmed_ && (o == details_->viewport() || o == icons_->viewport())) {
        auto* me = static_cast<QMouseEvent*>(e);
        if (!(me->buttons() & Qt::LeftButton)) return false;
        if ((me->position().toPoint() - dragStart_).manhattanLength() < QApplication::startDragDistance()) return false;
        dragArmed_ = false;
        Sel s = selection();
        if (s.empty()) return false;
        // start a drag of temp-extracted files
        QList<QUrl> urls;
        QString base = tempDir().path() + "/drag-" + QString::number(QDateTime::currentMSecsSinceEpoch());
        QDir().mkpath(base);
        rez::Report rep;
        for (Resource* r : s.res)
            if (rez::extract_resource(*r, ss(base), rep)) urls << QUrl::fromLocalFile(base + "/" + qs(r->file_name()));
        for (Directory* d : s.dirs) {
            rez::extract(*d, ss(base + "/" + qs(d->name)), rep, false);
            urls << QUrl::fromLocalFile(base + "/" + qs(d->name));
        }
        auto* mime = new QMimeData;
        mime->setUrls(urls);
        auto* drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->exec(Qt::CopyAction);
        return true;
    }
    if (e->type() == QEvent::KeyPress && (o == details_ || o == icons_)) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Backspace) {
            doUp();
            return true;
        }
    }
    return QMainWindow::eventFilter(o, e);
}

void MainWindow::testNavigate(const QString& path, const QString& view) {
    if (view == "large") setViewMode(ViewMode::LargeIcons);
    else if (view == "small") setViewMode(ViewMode::SmallIcons);
    else if (view == "list") setViewMode(ViewMode::List);
    else if (view == "details") setViewMode(ViewMode::Details);
    if (path.isEmpty() || !archive_) return;
    location_->setEditText(path);
    doGo();
    if (Resource* r = archive_->root().find_path(ss(path))) {
        // select it
        for (int i = 0; i < details_->topLevelItemCount(); ++i)
            if (details_->topLevelItem(i)->data(0, PtrRole).value<quintptr>() == quintptr(r))
                details_->setCurrentItem(details_->topLevelItem(i));
        for (int i = 0; i < icons_->count(); ++i)
            if (icons_->item(i)->data(PtrRole).value<quintptr>() == quintptr(r)) icons_->setCurrentItem(icons_->item(i));
    }
}
