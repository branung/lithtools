#pragma once
// Lip-sync preview: a .lip's face nodes and their curves over time (one
// channel at a time: the rotation as LipCompiler's angles, or the
// translation), played with the voice line of the same name through the
// sound player when the archive has it, else on a clock of its own.

#include <QByteArray>
#include <QString>
#include <QWidget>
#include <functional>

class LipSyncView : public QWidget {
    Q_OBJECT
public:
    // Returns the voice line's bytes and sets its path in the archive, or
    // returns an empty array.
    using SoundLookup = std::function<QByteArray(QString& path)>;

    explicit LipSyncView(QWidget* parent = nullptr);
    ~LipSyncView() override;
    bool loadLip(const QByteArray& data, const QString& name, SoundLookup voice, QString* error = nullptr);
    void clear();

    void play();
    void pause();
    void stop();   // and back to the start
    bool isPlaying() const;
    double position() const;   // seconds
    void seek(double seconds);
    // 0-2: rotation X, Y, Z (degrees); 3-5: translation X, Y, Z.
    void setChannel(int channel);
    bool hasVoice() const;

private:
    struct Impl;
    Impl* d;
    void updatePosition();
};
