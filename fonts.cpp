#include "fonts.h"
#include <QFontDatabase>
#include <QStringList>

static QString font_manrope;
static QString font_noto;

void Fonts::load()
{
    int id1 = QFontDatabase::addApplicationFont("fonts/Manrope-VariableFont_wght.ttf");
    font_manrope = QFontDatabase::applicationFontFamilies(id1).at(0);

    int id2 = QFontDatabase::addApplicationFont("fonts/NotoSerif-VariableFont_wdth,wght.ttf");
    font_noto = QFontDatabase::applicationFontFamilies(id2).at(0);
}

QString Fonts::manrope() { return font_manrope; }
QString Fonts::noto() { return font_noto; }