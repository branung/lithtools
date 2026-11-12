#include "spriteview.h"

#include "textureview.h"

#include <lithtech/spr.hpp>

#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <vector>

struct SpriteView::Impl {
    lithtech::spr::Sprite sprite;
    bool loaded = false;
    QString name;
    struct Frame {
        QString path;    // as stored
        QString found;   // where it was found, empty if not
        QByteArray data;
    };
    std::vector<Frame> frames;
    int current = -1;
    // Playing: the engine's tracker time, elapsed since `clock` from `base`.
    bool playing = false;
    QElapsedTimer clock;
    qint64 base = 0;
    QTimer timer;

    QToolButton *play = nullptr, *prev = nullptr, *next = nullptr;
    QLabel* position = nullptr;
    QStackedWidget* stack = nullptr;
    TextureView* texture = nullptr;
    QLabel* missing = nullptr;
    QTreeWidget* header = nullptr;
    QTreeWidget* list = nullptr;

    qint64 now() const { return playing ? base + clock.elapsed() : base; }
};

SpriteView::SpriteView(QWidget* parent) : QWidget(parent), d(new Impl) {
    auto* top = new QVBoxLayout(this);
    top->setContentsMargins(0, 0, 0, 0);

    auto* bar = new QHBoxLayout();
    bar->setContentsMargins(4, 4, 4, 0);
    d->play = new QToolButton(this);
    d->play->setToolTip(tr("Play / pause (Space)"));
    d->prev = new QToolButton(this);
    d->prev->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
    d->prev->setToolTip(tr("Previous frame"));
    d->next = new QToolButton(this);
    d->next->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
    d->next->setToolTip(tr("Next frame"));
    d->position = new QLabel(this);
    bar->addWidget(d->play);
    bar->addWidget(d->prev);
    bar->addWidget(d->next);
    bar->addWidget(d->position, 1);
    top->addLayout(bar);

    auto* split = new QSplitter(Qt::Vertical, this);
    d->stack = new QStackedWidget(split);
    d->texture = new TextureView(d->stack);
    d->texture->showInfo(false);
    d->missing = new QLabel(d->stack);
    d->missing->setAlignment(Qt::AlignCenter);
    d->missing->setWordWrap(true);
    d->stack->addWidget(d->texture);
    d->stack->addWidget(d->missing);

    auto* lists = new QSplitter(Qt::Horizontal, split);
    d->header = new QTreeWidget(lists);
    d->header->setRootIsDecorated(false);
    d->header->setHeaderLabels({tr("Sprite"), tr("Value")});
    d->header->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    d->list = new QTreeWidget(lists);
    d->list->setRootIsDecorated(false);
    d->list->setHeaderLabels({tr("#"), tr("Frame")});
    d->list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    d->list->setToolTip(tr("Double-click a frame to go to its texture"));
    lists->addWidget(d->header);
    lists->addWidget(d->list);
    lists->setStretchFactor(0, 1);
    lists->setStretchFactor(1, 2);
    split->addWidget(d->stack);
    split->addWidget(lists);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    top->addWidget(split, 1);

    d->timer.setInterval(15);
    connect(&d->timer, &QTimer::timeout, this, [this] {
        if (d->sprite.length_ms() == 0) return;
        showFrame(d->sprite.frame_at(uint32_t(d->now() % d->sprite.length_ms())));
    });
    auto toggle = [this] { isPlaying() ? pause() : play(); };
    connect(d->play, &QToolButton::clicked, this, toggle);
    auto* space = new QShortcut(QKeySequence(Qt::Key_Space), this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(space, &QShortcut::activated, this, toggle);
    auto step = [this](int by) {
        int n = frameCount();
        if (n) setFrame(((d->current < 0 ? 0 : d->current) + by + n) % n);
    };
    connect(d->prev, &QToolButton::clicked, this, [step] { step(-1); });
    connect(d->next, &QToolButton::clicked, this, [step] { step(1); });
    connect(d->list, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* it) {
        if (it && d->list->indexOfTopLevelItem(it) != d->current) setFrame(d->list->indexOfTopLevelItem(it));
    });
    connect(d->list, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* it) {
        int i = d->list->indexOfTopLevelItem(it);
        if (i >= 0 && i < frameCount() && !d->frames[size_t(i)].found.isEmpty()) emit frameActivated(d->frames[size_t(i)].found);
    });
    clear();
}

SpriteView::~SpriteView() { delete d; }

void SpriteView::clear() {
    pause();
    d->loaded = false;
    d->sprite = {};
    d->frames.clear();
    d->current = -1;
    d->base = 0;
    d->texture->clear();
    d->missing->clear();
    d->stack->setCurrentWidget(d->texture);
    d->header->clear();
    d->list->clear();
    d->position->clear();
    d->play->setEnabled(false);
    d->prev->setEnabled(false);
    d->next->setEnabled(false);
}

bool SpriteView::loadSprite(const QByteArray& data, const QString& name, FrameLookup lookup, QString* error) {
    clear();
    d->name = name;
    try {
        d->sprite = lithtech::spr::read(reinterpret_cast<const uint8_t*>(data.constData()), size_t(data.size()));
    } catch (const std::exception& e) {
        QString msg = tr("Can't read %1: %2").arg(name, QString::fromUtf8(e.what()));
        if (data.isEmpty())
            msg = tr("%1 is empty (0 bytes), so not a sprite. The engine's reads of it come back zeroed: no frames at "
                     "a rate of 0, which spr_Create divides by.").arg(name);
        if (error) *error = msg;
        d->missing->setText(msg);
        d->stack->setCurrentWidget(d->missing);
        return false;
    }
    d->loaded = true;
    const auto& s = d->sprite;
    int found = 0;
    for (const std::string& p : s.frames) {
        Impl::Frame f;
        f.path = QString::fromLatin1(p.c_str());
        if (lookup) f.data = lookup(f.path, f.found);
        if (f.data.isEmpty()) f.found.clear();
        found += !f.found.isEmpty();
        auto* it = new QTreeWidgetItem(d->list, {QString::number(d->frames.size() + 1), f.path});
        if (f.found.isEmpty()) {
            it->setForeground(1, palette().brush(QPalette::Disabled, QPalette::Text));
            it->setToolTip(1, tr("Not in this archive"));
        } else {
            it->setToolTip(1, f.found);
        }
        d->frames.push_back(std::move(f));
    }

    auto add = [this](const QString& k, const QString& v) { new QTreeWidgetItem(d->header, {k, v}); };
    add(tr("Frames"), QString::number(s.frames.size()));
    add(tr("Frame rate"), tr("%1 per second").arg(s.frame_rate));
    if (s.frame_rate == 0) add(tr("Length"), tr("— (a frame rate of 0: the engine would divide by it)"));
    else add(tr("Length"), tr("%1 ms (%2 ms a frame)").arg(s.length_ms()).arg(1000 / s.frame_rate));
    add(tr("Keyed"), QString::number(s.keyed));
    add(tr("Translucent"), QString::number(s.translucent));
    add(tr("Colour key"), QStringLiteral("0x%1").arg(s.colour_key, 8, 16, QLatin1Char('0')));
    add(tr("Frames found"), tr("%1 of %2").arg(found).arg(s.frames.size()));
    if (s.trailing) add(tr("Trailing bytes"), QString::number(s.trailing));

    bool animates = s.length_ms() > 0 && s.frames.size() > 1;
    d->play->setEnabled(animates);
    d->prev->setEnabled(!s.frames.empty());
    d->next->setEnabled(!s.frames.empty());
    if (s.frames.empty()) {
        d->missing->setText(tr("%1 has no frames.").arg(name));
        d->stack->setCurrentWidget(d->missing);
        d->position->setText(tr("No frames"));
    } else {
        showFrame(0);
    }
    if (animates) play();
    else pause();
    return true;
}

void SpriteView::play() {
    if (!d->loaded || d->sprite.length_ms() == 0) return;
    if (!d->playing) {
        d->playing = true;
        d->clock.start();
        d->timer.start();
    }
    d->play->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
}

void SpriteView::pause() {
    if (d->playing) {
        d->base += d->clock.elapsed();
        d->playing = false;
    }
    d->timer.stop();
    d->play->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
}

bool SpriteView::isPlaying() const { return d->playing; }
int SpriteView::frame() const { return d->current; }
int SpriteView::frameCount() const { return int(d->frames.size()); }

void SpriteView::setFrame(int index) {
    if (index < 0 || index >= frameCount()) return;
    pause();
    // the tracker's time at the start of that frame, so play goes on from it
    if (d->sprite.frame_rate) d->base = qint64(index) * (1000 / d->sprite.frame_rate);
    showFrame(index);
}

void SpriteView::showFrame(int index) {
    if (index < 0 || index >= frameCount()) return;
    QString pos = tr("Frame %1 of %2").arg(index + 1).arg(frameCount());
    if (d->sprite.frame_rate) pos += tr("  (%1 ms)").arg(d->now() % std::max<qint64>(1, d->sprite.length_ms()));
    d->position->setText(pos);
    if (index == d->current) return;
    d->current = index;
    const Impl::Frame& f = d->frames[size_t(index)];
    QString err;
    if (f.found.isEmpty()) {
        d->missing->setText(tr("Frame %1: %2\nis not in this archive.").arg(index + 1).arg(f.path));
        d->stack->setCurrentWidget(d->missing);
    } else if (!d->texture->loadDtx(f.data, f.found, &err)) {
        d->missing->setText(tr("Frame %1: %2").arg(index + 1).arg(err));
        d->stack->setCurrentWidget(d->missing);
    } else {
        d->stack->setCurrentWidget(d->texture);
    }
    QSignalBlocker b(d->list);
    d->list->setCurrentItem(d->list->topLevelItem(index));
}
