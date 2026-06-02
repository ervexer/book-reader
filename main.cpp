#include <QApplication>
#include <QProxyStyle>
#include "mainwindow.h"

// Стиль, убирающий пунктирную рамку фокуса со всех виджетов приложения.
class NoFocusRectStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
        QPainter* painter, const QWidget* widget) const override
    {
        if (element == QStyle::PE_FrameFocusRect)
            return; // не рисуем рамку фокуса
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setStyle(new NoFocusRectStyle);

    MainWindow window;
    window.show();

    return app.exec();
}
