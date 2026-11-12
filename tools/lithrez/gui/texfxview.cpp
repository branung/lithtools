#include "texfxview.h"

#include <lithtech/texscript.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QSplitter>
#include <QStyle>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <array>
#include <cmath>
#include <vector>

namespace ts = lithtech::texscript;

namespace {

QString tr_(const char* s) { return QCoreApplication::translate("TexFxView", s); }

QString inputText(uint32_t i) {
    switch (i) {
    case ts::IN_CSNORMAL: return tr_("CSNormal: camera-space normals");
    case ts::IN_CSPOS: return tr_("CSPos: camera-space positions");
    case ts::IN_CSREFLECTION: return tr_("CSReflection: camera-space reflection vectors");
    case ts::IN_WSNORMAL: return tr_("WSNormal: world-space normals");
    case ts::IN_WSPOS: return tr_("WSPos: world-space positions");
    case ts::IN_WSREFLECTION: return tr_("WSReflection: world-space reflection vectors");
    case ts::IN_UV: return tr_("UV: the texture coordinates");
    default: return tr_("unknown (%1)").arg(i);
    }
}

QString outputText(uint32_t o) {
    const char* n = ts::output_name(o);
    return n ? QString::fromLatin1(n) : tr_("unknown (%1)").arg(o);
}

QString dirtyText(uint32_t f) {
    QStringList s;
    if (f & ts::DIRTY_EVERYUPDATE) s << tr_("every frame (uses Time or Elapsed)");
    if (f & ts::DIRTY_USERVARCHANGED) s << tr_("when its parameters change");
    if (f & ~3u) s << QStringLiteral("0x%1").arg(f & ~3u, 0, 16);
    return s.isEmpty() ? tr_("never: evaluated once") : s.join(QStringLiteral("; "));
}

QString num(float f) { return QString::fromStdString(ts::number(f)); }

}  // namespace

struct TexFxView::Impl {
    bool loaded = false, group = false;
    ts::Group grp;
    // the scripts the view can run: a .tfs alone, or a group's evaluated stages
    struct Run {
        QString label;
        bool found = false;
        ts::Script script;
        std::vector<float> values;   // the starting user variables
    };
    std::vector<Run> runs;
    QStringList links;   // the scripts the group's links go to (link:<index>)
    int current = -1;
    float user[ts::NUM_USER] = {};
    std::array<float, 16> matrix{};
    bool playing = false;
    QElapsedTimer clock;
    double base = 0, last = 0;
    QTimer timer;

    QTextBrowser* info = nullptr;
    QWidget* evalBox = nullptr;
    QToolButton *play = nullptr, *restart = nullptr;
    QLabel* time = nullptr;
    QComboBox* stageBox = nullptr;
    QFormLayout* params = nullptr;
    QWidget* paramBox = nullptr;
    std::vector<QDoubleSpinBox*> spins;
    QTableWidget* table = nullptr;
    QLabel* pattern = nullptr;
    QLabel* note = nullptr;
    QPlainTextEdit* code = nullptr;

    double now() const { return playing ? base + clock.elapsed() / 1000.0 : base; }
};

TexFxView::TexFxView(QWidget* parent) : QWidget(parent), d(new Impl) {
    auto* top = new QVBoxLayout(this);
    top->setContentsMargins(0, 0, 0, 0);
    auto* split = new QSplitter(Qt::Vertical, this);
    d->info = new QTextBrowser(split);
    d->info->setOpenLinks(false);

    d->evalBox = new QWidget(split);
    auto* ev = new QVBoxLayout(d->evalBox);
    ev->setContentsMargins(4, 4, 4, 4);
    auto* bar = new QHBoxLayout();
    d->play = new QToolButton(d->evalBox);
    d->play->setToolTip(tr("Run / pause the script over time (Space)"));
    d->restart = new QToolButton(d->evalBox);
    d->restart->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
    d->restart->setToolTip(tr("Back to time 0, the parameters to their starting values"));
    d->time = new QLabel(d->evalBox);
    d->time->setMinimumWidth(110);
    d->stageBox = new QComboBox(d->evalBox);
    d->stageBox->setToolTip(tr("The stage whose script runs"));
    bar->addWidget(d->play);
    bar->addWidget(d->restart);
    bar->addWidget(d->time);
    bar->addWidget(d->stageBox);
    bar->addStretch(1);
    ev->addLayout(bar);
    auto* row = new QHBoxLayout();
    d->paramBox = new QWidget(d->evalBox);
    d->params = new QFormLayout(d->paramBox);
    d->params->setContentsMargins(0, 0, 0, 0);
    d->table = new QTableWidget(4, 4, d->evalBox);
    d->table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    d->table->setSelectionMode(QAbstractItemView::NoSelection);
    d->table->setToolTip(tr("The texture matrix (MatRC: row R, column C), as the script leaves it"));
    for (int i = 0; i < 4; ++i) {
        d->table->setHorizontalHeaderItem(i, new QTableWidgetItem(QString::number(i)));
        d->table->setVerticalHeaderItem(i, new QTableWidgetItem(QString::number(i)));
        for (int j = 0; j < 4; ++j) {
            auto* it = new QTableWidgetItem;
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            d->table->setItem(i, j, it);
        }
    }
    d->table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    d->table->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    d->table->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    d->table->setMinimumSize(260, 130);
    d->pattern = new QLabel(d->evalBox);
    d->pattern->setFixedSize(132, 132);
    d->pattern->setToolTip(tr("A test pattern moved by the matrix: each point (u, v) shows the pattern at "
                              "(u, v, 1, 0) times the matrix, as Direct3D transforms two texture coordinates"));
    row->addWidget(d->paramBox);
    row->addWidget(d->table, 1);
    row->addWidget(d->pattern);
    ev->addLayout(row, 1);
    d->note = new QLabel(d->evalBox);
    d->note->setWordWrap(true);
    ev->addWidget(d->note);

    d->code = new QPlainTextEdit(split);
    d->code->setReadOnly(true);
    d->code->setLineWrapMode(QPlainTextEdit::NoWrap);
    d->code->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    split->addWidget(d->info);
    split->addWidget(d->evalBox);
    split->addWidget(d->code);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 2);
    top->addWidget(split, 1);

    d->timer.setInterval(30);
    connect(&d->timer, &QTimer::timeout, this, [this] {
        double t = d->now();
        evaluate(t, t - d->last);
        d->last = t;
    });
    auto toggle = [this] { isPlaying() ? pause() : play(); };
    connect(d->play, &QToolButton::clicked, this, toggle);
    auto* space = new QShortcut(QKeySequence(Qt::Key_Space), this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(space, &QShortcut::activated, this, toggle);
    connect(d->restart, &QToolButton::clicked, this, [this] { showStage(d->current); });
    connect(d->stageBox, QOverload<int>::of(&QComboBox::activated), this, [this](int i) { showStage(i); });
    connect(d->info, &QTextBrowser::anchorClicked, this, [this](const QUrl& u) {
        int i = u.path().toInt();
        if (u.scheme() == QLatin1String("link") && i >= 0 && i < d->links.size()) emit scriptActivated(d->links[i]);
    });
    clear();
}

TexFxView::~TexFxView() { delete d; }

void TexFxView::clear() {
    pause();
    d->loaded = d->group = false;
    d->grp = {};
    d->runs.clear();
    d->links.clear();
    d->current = -1;
    d->base = d->last = 0;
    d->info->clear();
    d->code->clear();
    d->stageBox->clear();
    d->stageBox->hide();
    d->evalBox->hide();
    d->note->clear();
    for (auto* s : d->spins) {
        d->params->removeRow(s);
    }
    d->spins.clear();
}

bool TexFxView::loadScript(const QByteArray& data, const QString& name, QString* error) {
    clear();
    Impl::Run run;
    try {
        run.script = ts::read(reinterpret_cast<const uint8_t*>(data.constData()), size_t(data.size()));
    } catch (const std::exception& e) {
        if (error) *error = tr("Can't read %1: %2").arg(name, QString::fromUtf8(e.what()));
        return false;
    }
    d->loaded = true;
    run.found = true;
    run.label = name;
    run.values.assign(run.script.params.size(), 0.0f);
    const ts::Script& s = run.script;
    auto row = [](const QString& k, const QString& v) {
        return QStringLiteral("<tr><td style='color:gray;padding-right:8px'>%1</td><td>%2</td></tr>").arg(k, v);
    };
    QString h = QStringLiteral("<b>%1</b> &nbsp;%2<table>").arg(name.toHtmlEscaped(), tr("texture script"));
    QStringList ps;
    for (auto& p : s.params) ps << QString::fromLatin1(p.c_str()).toHtmlEscaped();
    h += row(tr("Parameters"), ps.isEmpty() ? tr("none") : ps.join(QStringLiteral(", ")));
    h += row(tr("Input"), inputText(s.input).toHtmlEscaped());
    h += row(tr("Output"), outputText(s.output).toHtmlEscaped());
    h += row(tr("Re-evaluated"), dirtyText(s.dirty).toHtmlEscaped());
    QStringList cs;
    for (float c : s.constants) cs << num(c);
    h += row(tr("Constants"), cs.isEmpty() ? tr("none") : cs.join(QStringLiteral(", ")));
    h += row(tr("Code"), tr("%1 instructions (%2 bytes)").arg(s.code.size() / 4).arg(s.code.size()));
    if (s.trailing) h += row(tr("Trailing bytes"), QString::number(s.trailing));
    for (auto& p : ts::check(s)) h += row(tr("Problem"), QString::fromStdString(p).toHtmlEscaped());
    h += QStringLiteral("</table>");
    d->info->setHtml(h);
    d->runs.push_back(std::move(run));
    showStage(0);
    return true;
}

bool TexFxView::loadGroup(const QByteArray& data, const QString& name, ScriptLookup lookup, QString* error) {
    clear();
    try {
        d->grp = ts::read_group(reinterpret_cast<const uint8_t*>(data.constData()), size_t(data.size()));
    } catch (const std::exception& e) {
        if (error) *error = tr("Can't read %1: %2").arg(name, QString::fromUtf8(e.what()));
        return false;
    }
    d->loaded = d->group = true;
    auto row = [](const QString& k, const QString& v) {
        return QStringLiteral("<tr><td style='color:gray;padding-right:8px'>%1</td><td>%2</td></tr>").arg(k, v);
    };
    QString h = QStringLiteral("<b>%1</b> &nbsp;%2<table>").arg(name.toHtmlEscaped(), tr("texture effect group"));
    h += row(tr("Version"), QString::number(d->grp.version));
    h += row(tr("Stages"), d->grp.stage_count == ts::kMaxStages ? QString::number(d->grp.stage_count)
                                                               : tr("%1 stored (the engine reads at most 2)").arg(d->grp.stage_count));
    if (d->grp.trailing) h += row(tr("Trailing bytes"), QString::number(d->grp.trailing));
    h += QStringLiteral("</table>");
    for (std::size_t i = 0; i < d->grp.stages.size(); ++i) {
        const ts::Stage& st = d->grp.stages[i];
        const char* type = ts::stage_type_name(st.type);
        const char* ch = ts::channel_name(st.channel);
        h += QStringLiteral("<p><b>%1</b> &nbsp;%2").arg(tr("Stage %1").arg(i), type ? QString::fromLatin1(type) : tr("unknown type %1").arg(st.type));
        if (st.type == ts::STAGE_DISABLED) {
            h += QStringLiteral("</p>");
            continue;
        }
        h += QStringLiteral(", %1</p><table>").arg(ch ? QString::fromLatin1(ch) : tr("channel %1").arg(st.channel));
        if (st.type == ts::STAGE_OVERRIDDEN) {
            h += row(tr("Uses"), tr("stage %1's transform").arg(st.override_stage)) + QStringLiteral("</table>");
            continue;
        }
        if (st.type != ts::STAGE_EVALUATED) {
            h += QStringLiteral("</table>");
            continue;
        }
        Impl::Run run;
        QString path = QString::fromLatin1(st.script.c_str());
        QString found;
        QByteArray b = lookup ? lookup(path, found) : QByteArray();
        QString err;
        if (!b.isEmpty()) {
            try {
                run.script = ts::read(reinterpret_cast<const uint8_t*>(b.constData()), size_t(b.size()));
                run.found = true;
            } catch (const std::exception& e) {
                err = QString::fromUtf8(e.what());
            }
        }
        if (run.found)
        {
            h += row(tr("Script"), QStringLiteral("<a href='link:%1'>%2</a>").arg(d->links.size()).arg(path.toHtmlEscaped()));
            d->links << found;
        }
        else if (!b.isEmpty())
            h += row(tr("Script"), tr("%1 (can't read it: %2)").arg(path.toHtmlEscaped(), err.toHtmlEscaped()));
        else
            h += row(tr("Script"), tr("%1 (not in this archive)").arg(path.toHtmlEscaped()));
        for (std::size_t v = 0; v < st.defaults.size(); ++v) {
            QString pn = run.found && v < run.script.params.size() ? QString::fromLatin1(run.script.params[v].c_str())
                                                                   : tr("value %1").arg(v);
            if (run.found && v >= run.script.params.size() && st.defaults[v] == 0) continue;   // DEdit's unused slots
            h += row(pn.toHtmlEscaped(), num(st.defaults[v]));
        }
        h += QStringLiteral("</table>");
        run.label = tr("Stage %1: %2").arg(i).arg(QString::fromLatin1(st.script.c_str()));
        run.values = st.defaults;
        if (run.found) d->runs.push_back(std::move(run));
    }
    d->info->setHtml(h);
    for (auto& r : d->runs) d->stageBox->addItem(r.label);
    d->stageBox->setVisible(d->runs.size() > 1);
    showStage(d->runs.empty() ? -1 : 0);
    return true;
}

void TexFxView::showStage(int index) {
    pause();
    if (index < 0 || index >= int(d->runs.size())) {
        d->evalBox->hide();
        d->code->hide();
        return;
    }
    d->current = index;
    if (d->stageBox->currentIndex() != index) d->stageBox->setCurrentIndex(index);
    const Impl::Run& r = d->runs[size_t(index)];
    const ts::Script& s = r.script;
    QString text = QString::fromStdString(ts::decompile(s));
    if (d->group) text = QStringLiteral("# %1\n").arg(r.label) + text;
    text += QStringLiteral("\n# The bytecode: offset, bytes, op, what the interpreter does\n");
    for (auto& l : ts::disassemble(s)) text += QStringLiteral("# ") + QString::fromStdString(l) + QLatin1Char('\n');
    d->code->setPlainText(text);

    for (auto* sp : d->spins) d->params->removeRow(sp);
    d->spins.clear();
    std::fill(std::begin(d->user), std::end(d->user), 0.0f);
    for (std::size_t i = 0; i < ts::NUM_USER && i < r.values.size(); ++i) d->user[i] = r.values[i];
    for (std::size_t i = 0; i < s.params.size() && i < ts::NUM_USER; ++i) {
        auto* sp = new QDoubleSpinBox(d->paramBox);
        sp->setRange(-1e6, 1e6);
        sp->setDecimals(4);
        sp->setSingleStep(0.1);
        sp->setValue(double(d->user[i]));
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, i](double v) {
            d->user[i] = float(v);
            if (!isPlaying()) evaluate(d->base, 0);
        });
        d->params->addRow(QString::fromLatin1(s.params[i].c_str()), sp);
        d->spins.push_back(sp);
    }
    d->paramBox->setVisible(!d->spins.empty());
    bool uv = s.input == ts::IN_UV && s.output == ts::OUT_2;
    d->pattern->setVisible(uv);
    d->note->setText(uv ? tr("The script runs as the engine runs it (its Sin is the cosine and its Cos the sine). "
                             "Time starts at 0; Level offset is 0.")
                        : tr("The script runs as the engine runs it (its Sin is the cosine and its Cos the sine). "
                             "Its input is %1, so the renderer turns the matrix into world or camera space first "
                             "and there is no flat preview; this is the matrix as the script leaves it.")
                              .arg(QString::fromLatin1(ts::input_name(s.input) ? ts::input_name(s.input) : "?")));
    d->evalBox->show();
    d->code->show();
    d->base = d->last = 0;
    evaluate(0, 0);
    if (s.dirty & ts::DIRTY_EVERYUPDATE) play();
}

void TexFxView::evaluate(double seconds, double elapsed) {
    if (d->current < 0) return;
    const ts::Script& s = d->runs[size_t(d->current)].script;
    d->matrix = ts::evaluate(s, float(seconds), float(elapsed), d->user);
    // the user variables as the script left them (the engine writes them back)
    for (std::size_t i = 0; i < d->spins.size(); ++i)
        if (!d->spins[i]->hasFocus() && d->spins[i]->value() != double(d->user[i])) {
            QSignalBlocker b(d->spins[i]);
            d->spins[i]->setValue(double(d->user[i]));
        }
    d->time->setText(tr("Time %1 s").arg(seconds, 0, 'f', 2));
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) d->table->item(i, j)->setText(QString::number(double(d->matrix[size_t(4 * i + j)]), 'f', 4));
    if (!d->pattern->isVisible()) return;
    // a checkerboard of 4 x 4 cells, coloured by cell, at (u, v, 1, 0) * M
    const int n = 128;
    QImage img(n, n, QImage::Format_RGB32);
    const auto& m = d->matrix;
    for (int y = 0; y < n; ++y) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        float v = (y + 0.5f) / n;
        for (int x = 0; x < n; ++x) {
            float u = (x + 0.5f) / n;
            float tu = m[0] * u + m[1] * v + m[2], tv = m[4] * u + m[5] * v + m[6];
            float fu = tu - std::floor(tu), fv = tv - std::floor(tv);
            int cu = int(fu * 4), cv = int(fv * 4);
            bool dark = (cu + cv) & 1;
            int r = 80 + cu * 40, g = 80 + cv * 40;
            line[x] = dark ? qRgb(r / 2, g / 2, 90) : qRgb(r, g, 200);
        }
    }
    d->pattern->setPixmap(QPixmap::fromImage(img.scaled(132, 132)));
}

void TexFxView::play() {
    if (d->current < 0) return;
    if (!d->playing) {
        d->playing = true;
        d->clock.start();
        d->timer.start();
    }
    d->play->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
}

void TexFxView::pause() {
    if (d->playing) {
        d->base += d->clock.elapsed() / 1000.0;
        d->playing = false;
    }
    d->timer.stop();
    d->play->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
}

bool TexFxView::isPlaying() const { return d->playing; }

void TexFxView::setTime(double seconds) {
    pause();
    d->base = seconds;
    evaluate(seconds, seconds - d->last);
    d->last = seconds;
}

QList<float> TexFxView::matrix() const { return QList<float>(d->matrix.begin(), d->matrix.end()); }
