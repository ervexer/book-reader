#pragma once

#include <QMainWindow>
#include <QPoint>

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
    /*
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    */
private:

    QStackedWidget* stack;
    Sidebar* sidebar;
    BookInfoPage* bookInfoPage = nullptr;
    ReaderWindow* readerWindow = nullptr;
    QPushButton* addBtn = nullptr;
    int lastNonBookPageIndex = 0;

    bool dragging = false;
    QPoint dragPosition;
    const int dragHeight = 40;

    bool isWindowedMode = false;
};
