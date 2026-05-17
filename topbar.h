#pragma once

#include <QWidget>
#include <QPushButton>
#include <QPoint>


class TopBar : public QWidget
{
    Q_OBJECT

public:
    explicit TopBar(QWidget* parent = nullptr);

signals:
    void closeClicked();
    void minimizeClicked();
    void maximizeClicked();
    void toggleSidebarClicked();

private:
    bool isDragging = false;
    QPoint dragPosition;

    QPushButton* menuBtn;
    QPushButton* minBtn;
    QPushButton* maxBtn;
    QPushButton* closeBtn;
};
