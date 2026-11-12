#pragma once
// Texture effect preview: a compiled texture script (.tfs) as its
// parameters, its source decompiled and its bytecode disassembled, and run
// over time as the engine runs it, showing the texture matrix it makes
// (and, for a script on UV coordinates, a pattern moved by it); a texture
// effect group (.tfg) as its stages, their scripts (linked) and their
// parameter values, the evaluated stages run the same way.

#include <QByteArray>
#include <QString>
#include <QWidget>
#include <functional>

class TexFxView : public QWidget {
    Q_OBJECT
public:
    // Returns a script's bytes and sets the path it was found at, or
    // returns an empty array.
    using ScriptLookup = std::function<QByteArray(const QString& path, QString& found)>;

    explicit TexFxView(QWidget* parent = nullptr);
    ~TexFxView() override;
    bool loadScript(const QByteArray& data, const QString& name, QString* error = nullptr);
    bool loadGroup(const QByteArray& data, const QString& name, ScriptLookup scripts, QString* error = nullptr);
    void clear();

    void play();
    void pause();
    bool isPlaying() const;
    // For tests: the time the script runs at, in seconds, and the matrix
    // it made there (row-major).
    void setTime(double seconds);
    QList<float> matrix() const;

signals:
    // A script clicked in a group: its path in the archive.
    void scriptActivated(const QString& path);

private:
    struct Impl;
    Impl* d;
    void showStage(int index);
    void evaluate(double seconds, double elapsed);
};
