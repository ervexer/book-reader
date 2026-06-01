#include "fonts.h"
#include <QFontDatabase>
#include <QStringList>

// Шрифты из комплекта удалены — используем системный.
// "Segoe UI" присутствует во всех современных Windows и хорошо читается.
static const QString kSystemFont = "Segoe UI";

void Fonts::load()
{
    // Ничего не загружаем — системного шрифта достаточно.
}

QString Fonts::manrope() { return kSystemFont; }
QString Fonts::noto() { return kSystemFont; }
