#include <QApplication>
#include "mainwindow.h"
#include "fonts.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    MainWindow window;
    window.show();
    Fonts::load();

    return app.exec();
}
