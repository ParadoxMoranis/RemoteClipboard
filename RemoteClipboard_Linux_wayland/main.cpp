#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyle(QStringLiteral("Fusion"));
    const QStringList arguments = a.arguments();
    const bool autoStartMode = arguments.contains(QStringLiteral("--autostart"));

    MainWindow w(autoStartMode);
    if (!autoStartMode) {
        w.show();
    }
    return a.exec();
}
