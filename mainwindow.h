#pragma once

#include <QMainWindow>
#include <QPoint>
#include <QRect>

class QStackedWidget;
class Sidebar;
class BookInfoPage;
class ReaderWindow;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);

protected:
    // Перетаскивание окна за верхнюю панель в оконном режиме.
    bool eventFilter(QObject* obj, QEvent* event) override;

private:

    QStackedWidget* stack;
    QWidget* topBarWidget = nullptr;
    Sidebar* sidebar;
    BookInfoPage* bookInfoPage = nullptr;
    ReaderWindow* readerWindow = nullptr;
    QPushButton* addBtn = nullptr;
    int lastNonBookPageIndex = 0;

    bool dragging = false;
    QPoint dragPosition;
    const int dragHeight = 40;

    bool isWindowedMode = false;

    bool windowMaximized = true; // текущее состояние окна (развёрнуто/оконный режим)
    QRect normalGeom;            // геометрия оконного режима
};
