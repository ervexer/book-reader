#pragma once

#include <QWidget>
#include <QPushButton>
#include <QPoint>


class TopBar : public QWidget
{
    Q_OBJECT

public:
    explicit TopBar(QWidget* parent = nullptr);

    void setBackVisible(bool visible); // показывать кнопку "назад" (на bookinfopage)
    void setBackOffset(int sidebarWidth); // выровнять стрелку по левому краю обложки

signals:
    void closeClicked();
    void minimizeClicked();
    void maximizeClicked();
    void toggleSidebarClicked();
    void backClicked();

private:
    bool isDragging = false;
    QPoint dragPosition;

    QPushButton* menuBtn;
    QWidget* backSpacer;
    QPushButton* backBtn;
    QPushButton* minBtn;
    QPushButton* maxBtn;
    QPushButton* closeBtn;
};
