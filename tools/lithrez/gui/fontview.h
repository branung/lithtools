#pragma once
// Font preview: a .ttf (NOLF2, Contract J.A.C.K. and Tron 2.0 ship their
// interface fonts) loaded into Qt's font database for as long as it is
// shown, with its family and styles, a sample text at several sizes and
// its printable ASCII.

#include <QByteArray>
#include <QString>
#include <QWidget>

class QLabel;
class QTextEdit;

class FontView : public QWidget {
    Q_OBJECT
public:
    explicit FontView(QWidget* parent = nullptr);
    ~FontView() override;
    bool loadFont(const QByteArray& data, const QString& name, QString* error = nullptr);
    void clear();

private:
    int id_ = -1;
    QLabel* title_ = nullptr;
    QTextEdit* sample_ = nullptr;
};
