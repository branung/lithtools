#pragma once
// Sprite preview: a .spr's frames animated at its rate (the engine's frame
// choice, lithtech::spr::Sprite::frame_at) in a TextureView, with
// play/pause and frame stepping, and the header and the frame list beside
// it. Frames are found by path through a callback, as model skins are.

#include <QByteArray>
#include <QString>
#include <QWidget>
#include <functional>

class SpriteView : public QWidget {
    Q_OBJECT
public:
    // Returns a frame's .dtx bytes and sets the path it was found at, or
    // returns an empty array (not found).
    using FrameLookup = std::function<QByteArray(const QString& path, QString& found)>;

    explicit SpriteView(QWidget* parent = nullptr);
    ~SpriteView() override;
    bool loadSprite(const QByteArray& data, const QString& name, FrameLookup frames, QString* error = nullptr);
    void clear();

    void play();
    void pause();
    bool isPlaying() const;
    // The frame shown (stepping pauses); -1 if none.
    void setFrame(int index);
    int frame() const;
    int frameCount() const;

signals:
    // A frame double-clicked in the list: its path in the archive.
    void frameActivated(const QString& path);

private:
    struct Impl;
    Impl* d;
    void showFrame(int index);
};
