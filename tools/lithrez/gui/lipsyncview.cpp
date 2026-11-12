#include "lipsyncview.h"

#include "soundview.h"

#include <lithtech/lip.hpp>

#include <QComboBox>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QShortcut>
#include <QSplitter>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace {

QString timeText(double s) {
    int ms = int(std::lround(std::max(0.0, s) * 1000));
    return QStringLiteral("%1:%2.%3").arg(ms / 60000).arg(ms / 1000 % 60, 2, 10, QLatin1Char('0')).arg(ms % 1000, 3, 10, QLatin1Char('0'));
}

QColor nodeColour(int i) {
    static const QColor c[] = {QColor(0x1f, 0x77, 0xb4), QColor(0xd6, 0x27, 0x28), QColor(0x2c, 0xa0, 0x2c),
                               QColor(0xff, 0x7f, 0x0e), QColor(0x94, 0x67, 0xbd), QColor(0x8c, 0x56, 0x4b),
                               QColor(0xe3, 0x77, 0xc2), QColor(0x7f, 0x7f, 0x7f), QColor(0xbc, 0xbd, 0x22),
                               QColor(0x17, 0xbe, 0xcf)};
    return c[i % 10];
}

// The curves: one channel of every shown node, keyframe by keyframe, with
// the playhead. Clicking or dragging seeks.
class Plot : public QWidget {
public:
    explicit Plot(QWidget* parent) : QWidget(parent) {
        setMinimumHeight(120);
        setBackgroundRole(QPalette::Base);
        setAutoFillBackground(true);
    }
    std::vector<std::vector<float>> curves;   // per node: values per keyframe
    std::vector<bool> shown;
    double length = 0;      // seconds the x axis covers
    double lipLength = 0;   // where the keyframes end
    double playhead = 0;
    QString unit;
    std::function<void(double)> seek;

protected:
    QRect area() const { return rect().adjusted(52, 8, -10, -22); }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QRect a = area();
        if (a.width() < 10 || a.height() < 10 || length <= 0) return;
        float lo = 0, hi = 0;
        bool any = false;
        for (std::size_t n = 0; n < curves.size(); ++n) {
            if (!shown[n]) continue;
            for (float v : curves[n]) {
                if (!any) lo = hi = v, any = true;
                lo = std::min(lo, v), hi = std::max(hi, v);
            }
        }
        if (!any || hi - lo < 1e-6f) lo -= 1, hi += 1;
        float pad = (hi - lo) * 0.05f;
        lo -= pad, hi += pad;
        auto x = [&](double s) { return a.left() + s / length * a.width(); };
        auto y = [&](float v) { return a.bottom() - double(v - lo) / double(hi - lo) * a.height(); };
        QColor grid = palette().color(QPalette::Mid);
        QColor text = palette().color(QPalette::Text);
        p.setPen(grid);
        p.drawRect(a);
        // y: lo, 0 (if in range), hi
        p.setPen(text);
        QFontMetrics fm(font());
        for (float v : {lo, hi}) {
            QString t = QString::number(double(v), 'g', 3);
            p.drawText(QRectF(0, y(v) - fm.height() / 2.0, a.left() - 4, fm.height()), Qt::AlignRight | Qt::AlignVCenter, t);
        }
        if (lo < 0 && hi > 0) {
            p.setPen(QPen(grid, 1, Qt::DashLine));
            p.drawLine(QPointF(a.left(), y(0)), QPointF(a.right(), y(0)));
            p.setPen(text);
            p.drawText(QRectF(0, y(0) - fm.height() / 2.0, a.left() - 4, fm.height()), Qt::AlignRight | Qt::AlignVCenter, "0");
        }
        p.drawText(QRectF(2, a.top(), a.left(), fm.height()), Qt::AlignLeft, unit);
        // x: a tick a second (or a half)
        double step = length > 8 ? 1.0 : 0.5;
        for (double s = 0; s <= length + 1e-9; s += step) {
            p.setPen(grid);
            p.drawLine(QPointF(x(s), a.bottom()), QPointF(x(s), a.bottom() + 4));
            p.setPen(text);
            p.drawText(QRectF(x(s) - 30, a.bottom() + 4, 60, fm.height()), Qt::AlignHCenter, QString::number(s, 'g', 3) + "s");
        }
        if (lipLength < length) {   // the voice line goes on past the keyframes
            p.fillRect(QRectF(QPointF(x(lipLength), a.top()), QPointF(a.right(), a.bottom())), QColor(128, 128, 128, 40));
        }
        p.save();
        p.setClipRect(a);
        for (std::size_t n = 0; n < curves.size(); ++n) {
            if (!shown[n] || curves[n].empty()) continue;
            QPainterPath path;
            for (std::size_t k = 0; k < curves[n].size(); ++k) {
                QPointF pt(x(double(k) / lithtech::lip::kKeyframesPerSecond), y(curves[n][k]));
                if (k == 0) path.moveTo(pt);
                else path.lineTo(pt);
            }
            p.setPen(QPen(nodeColour(int(n)), 1.5));
            p.drawPath(path);
        }
        p.restore();
        p.setPen(QPen(palette().color(QPalette::Highlight), 1.5));
        p.drawLine(QPointF(x(playhead), a.top()), QPointF(x(playhead), a.bottom()));
    }
    void mousePressEvent(QMouseEvent* e) override { seekTo(e->position().x()); }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (e->buttons() & Qt::LeftButton) seekTo(e->position().x());
    }
    void seekTo(double px) {
        QRect a = area();
        if (seek && a.width() > 0) seek(std::clamp((px - a.left()) / a.width(), 0.0, 1.0) * length);
    }
};

}  // namespace

struct LipSyncView::Impl {
    lithtech::lip::LipSync lip;
    bool loaded = false;
    // per node, per keyframe: rotation X, Y, Z (degrees), translation X, Y, Z
    std::vector<std::vector<std::array<float, 6>>> values;
    int channel = 0;
    bool voice = false;
    // without a voice line: a clock of our own
    bool playing = false;
    QElapsedTimer clock;
    double base = 0;
    QTimer timer;

    QToolButton *play = nullptr, *stop = nullptr;
    QLabel* time = nullptr;
    QComboBox* channelBox = nullptr;
    Plot* plot = nullptr;
    QTreeWidget* nodes = nullptr;
    QTreeWidget* header = nullptr;
    SoundView* sound = nullptr;
    QWidget* lower = nullptr;

    double length() const { return lip.length_seconds(); }
    double soundSeconds() const { return sound->sampleRate() ? double(sound->frames()) / sound->sampleRate() : 0; }
};

LipSyncView::LipSyncView(QWidget* parent) : QWidget(parent), d(new Impl) {
    auto* top = new QVBoxLayout(this);
    top->setContentsMargins(0, 0, 0, 0);
    auto* bar = new QHBoxLayout();
    bar->setContentsMargins(4, 4, 4, 0);
    d->play = new QToolButton(this);
    d->play->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    d->play->setToolTip(tr("Play / pause (Space), with the voice line if there is one"));
    d->stop = new QToolButton(this);
    d->stop->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    d->stop->setToolTip(tr("Stop"));
    d->time = new QLabel(this);
    d->time->setMinimumWidth(130);
    d->channelBox = new QComboBox(this);
    d->channelBox->addItems({tr("Rotation X (pitch)"), tr("Rotation Y (yaw)"), tr("Rotation Z (roll)"), tr("Translation X"),
                             tr("Translation Y"), tr("Translation Z")});
    d->channelBox->setToolTip(tr("The curve shown: rotations as the angles LipCompiler built them from (about Y, then X, "
                                 "then Z, in degrees), or the translation"));
    bar->addWidget(d->play);
    bar->addWidget(d->stop);
    bar->addWidget(d->time);
    bar->addStretch(1);
    bar->addWidget(d->channelBox);
    top->addLayout(bar);

    auto* split = new QSplitter(Qt::Vertical, this);
    auto* upper = new QSplitter(Qt::Horizontal, split);
    d->plot = new Plot(upper);
    auto* side = new QSplitter(Qt::Vertical, upper);
    d->nodes = new QTreeWidget(side);
    d->nodes->setRootIsDecorated(false);
    d->nodes->setHeaderLabels({tr("Node")});
    d->nodes->setToolTip(tr("Untick a node to hide its curve"));
    d->header = new QTreeWidget(side);
    d->header->setRootIsDecorated(false);
    d->header->setHeaderLabels({tr("Lip-sync"), tr("Value")});
    d->header->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    upper->addWidget(d->plot);
    upper->addWidget(side);
    upper->setStretchFactor(0, 3);
    upper->setStretchFactor(1, 1);
    d->lower = new QWidget(split);
    auto* ll = new QVBoxLayout(d->lower);
    ll->setContentsMargins(0, 0, 0, 0);
    d->sound = new SoundView(d->lower);
    ll->addWidget(d->sound, 1);
    split->addWidget(upper);
    split->addWidget(d->lower);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    top->addWidget(split, 1);

    d->plot->seek = [this](double s) { seek(s); };
    d->timer.setInterval(15);
    connect(&d->timer, &QTimer::timeout, this, [this] {
        if (position() >= d->length()) {
            pause();
            d->base = d->length();
        }
        updatePosition();
    });
    connect(d->sound, &SoundView::positionChanged, this, [this] { updatePosition(); });
    auto toggle = [this] { isPlaying() ? pause() : play(); };
    connect(d->play, &QToolButton::clicked, this, toggle);
    connect(d->stop, &QToolButton::clicked, this, &LipSyncView::stop);
    auto* space = new QShortcut(QKeySequence(Qt::Key_Space), this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(space, &QShortcut::activated, this, toggle);
    connect(d->channelBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LipSyncView::setChannel);
    connect(d->nodes, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* it) {
        int i = d->nodes->indexOfTopLevelItem(it);
        if (i < 0 || i >= int(d->plot->shown.size())) return;
        d->plot->shown[size_t(i)] = it->checkState(0) == Qt::Checked;
        d->plot->update();
    });
    clear();
}

LipSyncView::~LipSyncView() {
    // the sound player outlives d (a child widget) and signals as it stops
    disconnect(d->sound, nullptr, this, nullptr);
    delete d;
}

bool LipSyncView::hasVoice() const { return d->voice; }

void LipSyncView::clear() {
    stop();
    d->sound->clear();
    d->loaded = false;
    d->voice = false;
    d->lip = {};
    d->values.clear();
    d->plot->curves.clear();
    d->plot->shown.clear();
    d->plot->length = d->plot->lipLength = d->plot->playhead = 0;
    d->plot->update();
    d->nodes->clear();
    d->header->clear();
    d->lower->hide();
    d->play->setEnabled(false);
    d->stop->setEnabled(false);
    d->time->clear();
}

bool LipSyncView::loadLip(const QByteArray& data, const QString& name, SoundLookup voice, QString* error) {
    clear();
    try {
        d->lip = lithtech::lip::read(reinterpret_cast<const uint8_t*>(data.constData()), size_t(data.size()));
    } catch (const std::exception& e) {
        if (error) *error = tr("Can't read %1: %2").arg(name, QString::fromUtf8(e.what()));
        return false;
    }
    d->loaded = true;
    const auto& l = d->lip;
    QSignalBlocker b(d->nodes);
    for (std::size_t n = 0; n < l.nodes.size(); ++n) {
        std::vector<std::array<float, 6>> v;
        v.reserve(l.nodes[n].keys.size());
        for (const auto& k : l.nodes[n].keys) {
            auto e = lithtech::lip::euler_degrees(k.rotation);
            v.push_back({e[0], e[1], e[2], k.translation[0], k.translation[1], k.translation[2]});
        }
        d->values.push_back(std::move(v));
        auto* it = new QTreeWidgetItem(d->nodes, {QString::fromLatin1(l.nodes[n].name.c_str())});
        QPixmap swatch(12, 12);
        swatch.fill(nodeColour(int(n)));
        it->setIcon(0, QIcon(swatch));
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(0, Qt::Checked);
        d->plot->shown.push_back(true);
    }

    QString voicePath;
    QByteArray wav = voice ? voice(voicePath) : QByteArray();
    QString soundErr;
    if (!wav.isEmpty() && d->sound->loadWav(wav, voicePath, &soundErr)) {
        d->voice = true;
        d->lower->show();
    }

    auto add = [this](const QString& k, const QString& v) { new QTreeWidgetItem(d->header, {k, v}); };
    add(tr("Version"), QString::number(l.version));
    add(tr("Nodes"), QString::number(l.nodes.size()));
    add(tr("Keyframes"), tr("%1 (30 a second)").arg(l.keyframe_count));
    add(tr("Length"), timeText(l.length_seconds()));
    if (d->voice) {
        add(tr("Voice line"), voicePath);
        add(tr("Its length"), timeText(d->soundSeconds()));
    } else if (!wav.isEmpty()) {
        add(tr("Voice line"), tr("%1 doesn't play: %2").arg(voicePath, soundErr));
    } else {
        add(tr("Voice line"), tr("none: no %1.WAV beside it").arg(QFileInfo(name).completeBaseName()));
    }
    if (l.trailing) add(tr("Trailing bytes"), QString::number(l.trailing));

    // show the rotation axis that moves the most; a translation axis if
    // no rotation moves by a tenth of a degree
    std::array<float, 6> range{};
    for (int c = 0; c < 6; ++c) {
        float lo = 0, hi = 0;
        for (const auto& node : d->values)
            for (const auto& v : node) lo = std::min(lo, v[size_t(c)]), hi = std::max(hi, v[size_t(c)]);
        range[size_t(c)] = hi - lo;
    }
    int best = int(std::max_element(range.begin(), range.begin() + 3) - range.begin());
    if (range[size_t(best)] < 0.1f) best = 3 + int(std::max_element(range.begin() + 3, range.end()) - (range.begin() + 3));
    d->plot->lipLength = l.length_seconds();
    d->plot->length = std::max(l.length_seconds(), d->voice ? d->soundSeconds() : 0.0);
    d->play->setEnabled(d->plot->length > 0);
    d->stop->setEnabled(d->plot->length > 0);
    setChannel(best);
    updatePosition();
    return true;
}

void LipSyncView::setChannel(int channel) {
    d->channel = std::clamp(channel, 0, 5);
    if (d->channelBox->currentIndex() != d->channel) d->channelBox->setCurrentIndex(d->channel);
    d->plot->curves.clear();
    for (const auto& node : d->values) {
        std::vector<float> c;
        c.reserve(node.size());
        for (const auto& v : node) c.push_back(v[size_t(d->channel)]);
        d->plot->curves.push_back(std::move(c));
    }
    d->plot->unit = d->channel < 3 ? tr("degrees") : tr("units");
    d->plot->update();
}

void LipSyncView::play() {
    if (!d->loaded) return;
    if (d->voice) {
        d->sound->play();
    } else if (!d->playing) {
        if (d->base >= d->length()) d->base = 0;
        d->playing = true;
        d->clock.start();
        d->timer.start();
    }
    d->play->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
}

void LipSyncView::pause() {
    if (d->voice) d->sound->pause();
    if (d->playing) {
        d->base += d->clock.elapsed() / 1000.0;
        d->playing = false;
    }
    d->timer.stop();
    d->play->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    updatePosition();
}

void LipSyncView::stop() {
    if (d->voice) d->sound->stop();
    d->playing = false;
    d->timer.stop();
    d->base = 0;
    d->play->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    updatePosition();
}

bool LipSyncView::isPlaying() const { return d->voice ? d->sound->isPlaying() : d->playing; }

double LipSyncView::position() const {
    if (d->voice) return d->sound->sampleRate() ? double(d->sound->position()) / d->sound->sampleRate() : 0;
    return d->playing ? d->base + d->clock.elapsed() / 1000.0 : d->base;
}

void LipSyncView::seek(double seconds) {
    seconds = std::clamp(seconds, 0.0, d->plot->length);
    if (d->voice) {
        d->sound->seek(qint64(std::llround(seconds * d->sound->sampleRate())));
    } else {
        d->base = seconds;
        if (d->playing) d->clock.start();
    }
    updatePosition();
}

void LipSyncView::updatePosition() {
    if (!d->loaded) return;
    double s = position();
    d->play->setIcon(style()->standardIcon(isPlaying() ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    int key = std::min<int>(int(s * lithtech::lip::kKeyframesPerSecond), int(d->lip.keyframe_count) - 1);
    d->time->setText(tr("%1 / %2   keyframe %3").arg(timeText(s), timeText(d->plot->length)).arg(std::max(key, 0)));
    d->plot->playhead = s;
    d->plot->update();
}
