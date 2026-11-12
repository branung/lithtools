#pragma once
// Render style preview: an .ltb of file type 5 as a property tree: the
// header, then each style's lighting material and passes, each pass's
// states, texture stages and Direct3D options.

#include <QByteArray>
#include <QString>
#include <QWidget>

class QTreeWidget;
class QLabel;

class RenderStyleView : public QWidget {
    Q_OBJECT
public:
    explicit RenderStyleView(QWidget* parent = nullptr);
    bool loadRenderStyle(const QByteArray& data, const QString& name, QString* error = nullptr);
    void clear();

private:
    QLabel* title_ = nullptr;
    QTreeWidget* tree_ = nullptr;
};
