// LithRez Studio. Command line (WinRez LT's switches, plus file opening):
//   lithrez-studio [file.rez | file.dtx | file.dll | file.abc]
//   lithrez-studio <name.rez> /C <file or folder>...   create name.rez from them
//   lithrez-studio /A <file or folder>...              add them, then ask for a name
//   lithrez-studio /X <name.rez> <target folder>       extract everything
//   lithrez-studio /E <name.rez>                       extract, asking where

#include "mainwindow.h"
#include "modelview.h"
#include "stringtable.h"
#include "textureview.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTimer>

#include <lithtech/rez.hpp>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("LithRez Studio");
    QApplication::setOrganizationName("lithtools");
    QApplication::setApplicationVersion(QStringLiteral(LITHREZSTUDIO_VERSION));
    QApplication::setWindowIcon(QIcon(":/lithrez-studio.png"));

    QStringList args = app.arguments().mid(1);
    auto sw = [&](const QString& s) { return args.indexOf(QRegularExpression("^[/-]" + s + "$", QRegularExpression::CaseInsensitiveOption)); };

    if (int i = sw("X"); i >= 0 && args.size() >= i + 3) {
        MainWindow w;
        if (!w.openRez(args[i + 1])) return 1;
        return w.extractAllTo(args[i + 2]) ? 0 : 1;
    }
    if (int i = sw("E"); i >= 0 && args.size() >= i + 2) {
        MainWindow w;
        if (!w.openRez(args[i + 1])) return 1;
        QString dir = QFileDialog::getExistingDirectory(nullptr, QObject::tr("Extract %1 to").arg(QFileInfo(args[i + 1]).fileName()));
        if (dir.isEmpty()) return 1;
        return w.extractAllTo(dir) ? 0 : 1;
    }
    if (int i = sw("C"); i >= 1) {
        MainWindow w;
        w.addPaths(args.mid(i + 1));
        return w.saveTo(args[i - 1]) ? 0 : 1;
    }

    auto* w = new MainWindow;
    w->setAttribute(Qt::WA_DeleteOnClose);
    if (int i = sw("A"); i >= 0) {
        w->show();
        w->addPaths(args.mid(i + 1));
        return app.exec();
    }
    QStringList rezFiles;
    for (const QString& a : args) {
        QString ext = QFileInfo(a).suffix().toLower();
        if (ext == "dtx") {
            auto* t = new TextureWindow;
            t->setAttribute(Qt::WA_DeleteOnClose);
            t->openFile(a);
            t->show();
            delete w;
            return app.exec();
        }
        if (ext == "dll") {
            auto* s = new StringTableWindow;
            s->setAttribute(Qt::WA_DeleteOnClose);
            s->openFile(a);
            s->show();
            delete w;
            return app.exec();
        }
        if (lithtech::rez::Archive::probe(a.toUtf8().toStdString())) rezFiles << a;
    }
    // The window first, then the file, so its progress bar shows over the
    // window rather than before there is one.
    w->show();
    QTimer::singleShot(0, w, [w, rezFiles] {
        for (const QString& a : rezFiles) w->openRez(a);
        // For tests and documentation: LITHREZSTUDIO_SHOT=<png> [LITHREZSTUDIO_GOTO=<path>]
        // [LITHREZSTUDIO_VIEW=large|small|list|details] grabs the window and quits.
        if (qEnvironmentVariableIsSet("LITHREZSTUDIO_SHOT")) {
            QTimer::singleShot(400, w, [w] {
                w->testNavigate(qEnvironmentVariable("LITHREZSTUDIO_GOTO"), qEnvironmentVariable("LITHREZSTUDIO_VIEW"));
            });
            QTimer::singleShot(2500, w, [w] {
                w->grab().save(qEnvironmentVariable("LITHREZSTUDIO_SHOT"));
                QApplication::quit();
            });
        }
    });
    return app.exec();
}
