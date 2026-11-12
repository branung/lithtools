#pragma once
// LithRez Studio's main window: an Explorer-like view of a .rez file (WinRez
// LT's layout: folder tree on the left, contents on the right, location bar
// with history), with a preview pane for textures, models, sounds and text.

#include <QMainWindow>
#include <QPointer>
#include <QStringList>

#include <lithtech/rez.hpp>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

class QAction;
class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QPlainTextEdit;
class QDockWidget;
class TextureView;
class ModelView;
class SoundView;
class BankView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool openRez(const QString& path);
    void newRez();
    // WinRez's command line: add files/folders to a new archive.
    void addPaths(const QStringList& paths);
    bool saveTo(const QString& path);
    bool extractAllTo(const QString& dir);
    // Test hook: go to a folder or file, choose a view mode.
    void testNavigate(const QString& path, const QString& view);

protected:
    void closeEvent(QCloseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    enum class ViewMode { LargeIcons, SmallIcons, List, Details };
    struct Sel {
        std::vector<lithtech::rez::Directory*> dirs;
        std::vector<lithtech::rez::Resource*> res;
        bool empty() const { return dirs.empty() && res.empty(); }
    };

    void buildActions();
    void buildMenus();
    void buildToolbar();
    void buildViews();
    void updateActions();
    void updateTitle();
    void setModified(bool m = true);
    bool maybeSave();

    void rebuildTree();
    void addTreeDir(QTreeWidgetItem* parent, lithtech::rez::Directory* d);
    void showDir(lithtech::rez::Directory* d, bool addHistory = true);
    void refreshContents();
    void setViewMode(ViewMode m);
    void startThumbnails();
    Sel selection() const;
    lithtech::rez::Directory* targetDir() const;

    void onActivated(lithtech::rez::Directory* d, lithtech::rez::Resource* r);
    void preview(lithtech::rez::Resource* r);
    void clearPreview();
    void showResource(const QString& path);
    void openResource(lithtech::rez::Resource* r);
    void openSound(const QByteArray& data, const QString& name);   // in a window of its own (a .wav or a PS2 bank)
    QByteArray readData(const lithtech::rez::Resource* r) const;
    QByteArray findSkin(const lithtech::rez::Resource* model, int index, int count, QString& path) const;
    // The skins ATTRIBUTES/MODELBUTES.TXT gives a model (NOLF2, Contract
    // J.A.C.K., Tron 2.0), by slot; empty if it doesn't list it.
    QStringList butesSkins(const lithtech::rez::Resource* model) const;
    QString tempExtract(const lithtech::rez::Resource* r) const;

    // actions
    void doOpen();
    void doClose();
    bool doSave();
    bool doSaveAs();
    void doNewProject();
    void doWizard();
    void doImport();
    void doMerge();
    void doExtract(bool all);
    void doInsertFiles();
    void doReplace();
    void doInsertFolder();
    void doInsertEmptyFolder();
    void doDelete();
    void doRename();
    void doCut();
    void doPaste();
    void doSelectAll();
    void doBack();
    void doForward();
    void doUp();
    void doGo();
    void doInfo();
    void doSettings();
    void doAbout();
    void addRecent(const QString& path);
    void updateRecent();

    lithtech::rez::CreateOptions createOptions() const;
    // A long job's progress, filled in from its thread.
    struct Busy {
        std::mutex m;
        uint64_t done = 0, total = 0;   // total 0: a busy bar
        QString detail;
        std::atomic<bool> cancel{false};
        void set(uint64_t d, uint64_t t, const QString& text = QString()) {
            std::lock_guard<std::mutex> lock(m);
            done = d;
            total = t;
            if (!text.isNull()) detail = text;
        }
    };
    // Runs job on a thread of its own behind a progress dialog, which always
    // shows, if only for a moment; then finish, if given, on this thread with
    // the dialog still up (its text finishText). Rethrows what job throws.
    void runBusy(const QString& title, bool canCancel, const std::function<void(Busy&)>& job,
                 const std::function<void()>& finish = {}, const QString& finishText = QString());
    // Archive::open through runBusy (finish then runs before the dialog
    // closes); throws what Archive::open throws (rez::Cancelled if cancelled).
    std::unique_ptr<lithtech::rez::Archive> openArchive(const QString& path,
                                                        const std::function<void(std::unique_ptr<lithtech::rez::Archive>&)>& finish = {});
    bool busy_ = false;   // a runBusy job is running: thumbnails wait
    void importArchive(lithtech::rez::Archive& from, lithtech::rez::Directory& src,
                       lithtech::rez::Directory& dst, int& count);
    int insertPaths(const QStringList& paths, lithtech::rez::Directory* into);

    std::unique_ptr<lithtech::rez::Archive> archive_;
    QString path_;          // empty for an untitled archive
    bool modified_ = false;
    lithtech::rez::Directory* current_ = nullptr;
    std::vector<lithtech::rez::Directory*> back_, forward_;
    Sel clipboard_;
    ViewMode mode_ = ViewMode::Details;
    int thumbRow_ = 0;
    // MODELBUTES.TXT as parsed: ModelFile (lower case, backslashes) -> skins.
    mutable const lithtech::rez::Resource* butesFrom_ = nullptr;
    mutable std::map<QString, QStringList> butes_;
    QPoint dragStart_;
    bool dragArmed_ = false;

    QTreeWidget* tree_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QTreeWidget* details_ = nullptr;
    QListWidget* icons_ = nullptr;
    QComboBox* location_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* ps2Note_ = nullptr;   // shown while a PS2 archive is open
    QDockWidget* previewDock_ = nullptr;
    QStackedWidget* previewStack_ = nullptr;
    TextureView* texPreview_ = nullptr;
    ModelView* modelPreview_ = nullptr;
    SoundView* soundPreview_ = nullptr;
    BankView* bankPreview_ = nullptr;
    QPlainTextEdit* textPreview_ = nullptr;
    QLabel* infoPreview_ = nullptr;
    QToolBar* toolbar_ = nullptr;
    QWidget* locationBar_ = nullptr;

    QAction *aNew, *aProject, *aWizard, *aOpen, *aClose, *aSave, *aSaveAs, *aImport, *aMerge, *aInfo,
        *aExit, *aTexViewer, *aStrings, *aModelViewer, *aSoundPlayer;
    QAction *aCut, *aCopy, *aPaste, *aSelectAll, *aDelete, *aRename;
    QAction *aExtract, *aExtractAll, *aInsert, *aInsertFolder, *aInsertEmpty, *aReplace;
    QAction *aToolbar, *aStatus, *aLocation, *aPreview, *aLarge, *aSmall, *aList, *aDetails, *aBack,
        *aForward, *aUp, *aRefresh;
    QAction *aSettings, *aHelp, *aAbout;
    QList<QAction*> recentActions_;
    QMenu* recentMenu_ = nullptr;
};
