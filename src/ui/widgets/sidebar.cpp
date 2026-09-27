#include "ui/widgets/sidebar.h"
#include "data/database.h"
#include "parsers/fb2parser.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QFrame>
#include <QFontDatabase>
#include <QIcon>
#include <QFileDialog>
#include <QDebug>
#include <QMessageBox>
#include <QSize>
#include <QStringList>

Sidebar::Sidebar(QWidget* parent) : QWidget(parent)
{
    setFixedWidth(220);
    setStyleSheet("background:#17130E;");

    // ===== FONT =====
    QFont uiFont("Segoe UI");
    uiFont.setPointSize(14);
    uiFont.setWeight(QFont::Medium);
    setFont(uiFont);

    // ===== LAYOUT =====
    layoutPtr = new QVBoxLayout(this);
    layoutPtr->setContentsMargins(10, 10, 10, 10);
    layoutPtr->setSpacing(5);

    // ===== ACTIVE LINE =====
    activeLine = new QFrame(this);
    activeLine->setFixedWidth(4);
    activeLine->setStyleSheet("background:#ff8c00;");
    activeLine->hide();

    // ===== MENU =====
    addMenuItem(" Главная", ":/icons/home.png", ":/icons/home_orange.png", 0);
    addMenuItem(" Библиотека", ":/icons/library.png", ":/icons/library_orange.png", 1);
    addMenuItem(" Избранное", ":/icons/collection.png", ":/icons/collection_orange.png", 2);

    layoutPtr->addSpacing(20);

    addBtn = new QPushButton("Добавить книгу");
    addBtn->setFocusPolicy(Qt::NoFocus);

    addBtn->setStyleSheet(
        "QPushButton{background:#ff8c00;color:white;border-radius:8px;"
        "padding:10px;font-size:18px;}"
        "QPushButton:hover{background:#ffa733;}"
    );

    connect(addBtn, &QPushButton::clicked, this, [=]()
    {
        QString filePath = QFileDialog::getOpenFileName(
            this, "Выберите файл книги", "", "Книги (*.fb2)"
        );

        if (!filePath.isEmpty())
        {
            ParsedBook book;
            QString err;
            if (!Fb2Parser::parseFile(filePath, &book, &err))
            {
                qWarning() << "Ошибка парсинга книги:" << err;
                QMessageBox::warning(this, "Не удалось добавить книгу", err);
                return;
            }

            const int bookId = Database::instance().addBook(book, &err);
            if (!bookId)
            {
                qWarning() << "Ошибка добавления в БД:" << err;
                QMessageBox::warning(this, "Не удалось добавить книгу", err);
                return;
            }

            emit bookAdded(bookId);
        }
    });

    layoutPtr->addWidget(addBtn);
    layoutPtr->addStretch();

    addMenuItem(" Аккаунт", ":/icons/profile.png", ":/icons/profile_orange.png", 3);
    addMenuItem(" Справка", ":/icons/interrogation.png", ":/icons/interrogation-orange.png", 4);
    setActive(0);
}

void Sidebar::addMenuItem(const QString& text, const QString& iconPath, const QString& activeIconPath, int index)
{
    QPushButton* btn = new QPushButton(text);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(22, 22));
    btn->setFixedHeight(45);
    btn->setFont(font());

    setStyleSheet("background-color:#1a1a1a;");
    btn->setStyleSheet(
        "QPushButton {"
        "color:#aaa;"
        "text-align:left;"
        "padding-left:10px;"
        "border:none;"
        "background:transparent;"
        "}"
        "QPushButton:hover {"
        "background:#2a2a2a;"
        "}"
    );

    layoutPtr->addWidget(btn);

    MenuItem item;
    item.button = btn;
    item.icon = iconPath;
    item.activeIcon = activeIconPath;
    item.index = index;

    items.push_back(item);

    connect(btn, &QPushButton::clicked, this, [=]() {
        setActive(index);
        emit pageSelected(index);
        });
}

void Sidebar::setActive(int index)
{
    for (auto& item : items)
    {
        bool active = (item.index == index);

        item.button->setIcon(QIcon(active ? item.activeIcon : item.icon));

        item.button->setStyleSheet(
            QString(
                "QPushButton{color:%1;text-align:left;padding-left:10px;border:none;}"
                "QPushButton:hover{background:#2a2a2a;}"
            ).arg(active ? "#ff8c00" : "#aaaaaa"));

        if (active)
        {
            activeLine->setParent(item.button);
            activeLine->move(0, 0);
            activeLine->resize(4, item.button->height());
            activeLine->show();
        }
    }
}

void Sidebar::toggleSidebar()
{
    collapsed = !collapsed;

    if (collapsed)
    {
        setFixedWidth(65);

        for (auto& item : items)
        {
            item.button->setText("");
            item.button->setIconSize(QSize(22, 22));
        }
        if (addBtn)
        {
            addBtn->setText("");
            addBtn->setIcon(QIcon(":/icons/plus.png"));
            addBtn->setIconSize(QSize(20, 20));
        }
    }
    else
    {
        setFixedWidth(220);
        QStringList texts = {" Главная"," Библиотека"," Избранное"," Аккаунт"," Справка"};

        for (int i = 0; i < items.size(); i++)
        {
            items[i].button->setText(texts[i]);
            items[i].button->setIconSize(QSize(22, 22));
            items[i].button->setFont(font());
        }
        if (addBtn)
        {
            addBtn->setText("Добавить книгу");
            addBtn->setIcon(QIcon());
            
        }
    }
}
