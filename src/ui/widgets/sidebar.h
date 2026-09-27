#pragma once

#include <QWidget>
#include <QVector>


class QPushButton;
class QFrame;
class QVBoxLayout;

class Sidebar : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(QWidget* parent = nullptr);

public slots:
    void toggleSidebar();
    void setActive(int index);

signals:
    void pageSelected(int index);
    void bookAdded(int bookId);

private:
    struct MenuItem {
        QPushButton* button = nullptr;
        QString icon;
        QString activeIcon;
        int index = 0;
    };

    QVector<MenuItem> items;

    QFrame* activeLine = nullptr;
    QVBoxLayout* layoutPtr = nullptr;
    QPushButton* addBtn = nullptr;

    bool collapsed = false;

    void addMenuItem(const QString& text,
        const QString& iconPath,
        const QString& activeIconPath,
        int index);
};
