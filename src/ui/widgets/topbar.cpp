#include "ui/widgets/topbar.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QMouseEvent>

TopBar::TopBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(40);
    setStyleSheet("background:#121212;");

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 10, 0);
    layout->setSpacing(5);

    menuBtn = new QPushButton;
    menuBtn->setIcon(QIcon(":/icons/menu.png"));
    menuBtn->setFixedSize(40, 40);
    menuBtn->setStyleSheet(
        "QPushButton{background:transparent;border:none;border-radius:6px;}"
        "QPushButton:hover{background:#1d1914;}");
    layout->addWidget(menuBtn);

    connect(menuBtn, &QPushButton::clicked,
        this, &TopBar::toggleSidebarClicked);

    // Распорка выравнивает стрелку "назад" по левому краю обложки книги
    // (ширина боковой панели + отступ страницы). Обновляется при сворачивании.
    backSpacer = new QWidget;
    backSpacer->setFixedWidth(171); // под раскрытую панель (220px)
    backSpacer->setStyleSheet("background:transparent;");
    layout->addWidget(backSpacer);

    // Кнопка "назад" — показывается только на странице информации о книге
    backBtn = new QPushButton;
    backBtn->setIcon(QIcon(":/icons/back.png"));
    backBtn->setIconSize(QSize(18, 18));
    backBtn->setFixedSize(36, 36);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton{background:transparent;border:none;}"
        "QPushButton:hover{background:#1d1914;border-radius:8px;}"
    );
    backBtn->hide();
    layout->addWidget(backBtn);

    connect(backBtn, &QPushButton::clicked,
        this, &TopBar::backClicked);

    layout->addStretch();

    minBtn = new QPushButton;
    maxBtn = new QPushButton;
    closeBtn = new QPushButton;

    minBtn->setIcon(QIcon(":/icons/minus.png"));
    maxBtn->setIcon(QIcon(":/icons/copy.png"));
    closeBtn->setIcon(QIcon(":/icons/cross.png"));

    for (auto b : { minBtn, maxBtn, closeBtn })
    {
        b->setFixedSize(30, 30);
        b->setStyleSheet(
            "QPushButton{background:transparent;border:none;border-radius:6px;}"
            "QPushButton:hover{background:#1d1914;}");
        layout->addWidget(b);
    }

    connect(closeBtn, &QPushButton::clicked, this, &TopBar::closeClicked);
    connect(minBtn, &QPushButton::clicked, this, &TopBar::minimizeClicked);
    connect(maxBtn, &QPushButton::clicked, this, &TopBar::maximizeClicked);
}

void TopBar::setBackVisible(bool visible)
{
    backBtn->setVisible(visible);
}

void TopBar::setBackOffset(int sidebarWidth)
{
    // Левый край обложки = sidebarWidth + 22 (отступ страницы). Учитываем
    // фиксированную часть слева (margin 12 + меню 40 + 2×spacing 5) и иконку
    // (18px по центру 36px кнопки → 9px слева).
    backSpacer->setFixedWidth(qMax(0, sidebarWidth + 22 - 9 - (12 + 40 + 5 + 5)));
}
