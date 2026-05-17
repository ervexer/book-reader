#include "topbar.h"
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
    menuBtn->setIcon(QIcon("image icon/menu.png"));
    menuBtn->setFixedSize(40, 40);
    menuBtn->setStyleSheet("background:transparent;");
    layout->addWidget(menuBtn);

    connect(menuBtn, &QPushButton::clicked,
        this, &TopBar::toggleSidebarClicked);

    layout->addStretch();

    minBtn = new QPushButton;
    maxBtn = new QPushButton;
    closeBtn = new QPushButton;

    minBtn->setIcon(QIcon("image icon/minus.png"));
    maxBtn->setIcon(QIcon("image icon/copy.png"));
    closeBtn->setIcon(QIcon("image icon/cross.png"));

    for (auto b : { minBtn, maxBtn, closeBtn })
    {
        b->setFixedSize(30, 30);
        b->setStyleSheet("background:transparent;");
        layout->addWidget(b);
    }

    connect(closeBtn, &QPushButton::clicked, this, &TopBar::closeClicked);
    connect(minBtn, &QPushButton::clicked, this, &TopBar::minimizeClicked);
    connect(maxBtn, &QPushButton::clicked, this, &TopBar::maximizeClicked);
}
