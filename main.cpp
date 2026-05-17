#include <QApplication>
#include <QDir>
#include "mainwindow.h"
#include "fonts.h"
#include "fb2textparser.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    Fb2TextParser::clearCache(QDir::current().filePath("caches"));

    MainWindow window;
    window.show();
    Fonts::load();

    return app.exec();
}
