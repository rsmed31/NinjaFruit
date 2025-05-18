#include <QApplication>
#include <QIcon>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Use the logo from our resource as the app/window icon:
    app.setWindowIcon(QIcon(":/image/logo.png"));

    MainWindow w;
    w.show();
    return app.exec();
}
