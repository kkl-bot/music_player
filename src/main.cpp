#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Music Player");
    app.setApplicationVersion("1.0");
    MainWindow window;
    window.show();
    
    return app.exec();
}