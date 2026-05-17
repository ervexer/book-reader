#include "mainwindow.h"
#include "sidebar.h"
#include "topbar.h"
#include "homepage.h"
#include "librarypage.h"
#include "bookinfopage.h"
#include "readerwindow.h"

#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QApplication>
#include <QFileDialog>

// ==== ПРОСТЫЕ СТРАНИЦЫ ====
QWidget* createPage(const QString& text)
{
    QWidget* page = new QWidget;

    QVBoxLayout* layout = new QVBoxLayout(page);

    QLabel* label = new QLabel(text);
    label->setStyleSheet("color:white; font-size:24px;");

    layout->addStretch();
    layout->addWidget(label, 0, Qt::AlignCenter);
    layout->addStretch();

    page->setStyleSheet("background:#110E0A;"); // 👈 фон страниц

    return page;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    showMaximized();

    // УБИРАЕМ СИСТЕМНОЕ ОКНО
    setWindowFlags(Qt::FramelessWindowHint);

    QWidget* central = new QWidget;
    setCentralWidget(central);

    // ===== ГЛАВНЫЙ ВЕРТИКАЛЬНЫЙ LAYOUT =====
    QVBoxLayout* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ===== TOP BAR =====
    TopBar* topBar = new TopBar;

    // ===== ОСНОВНОЙ КОНТЕНТ =====
    QHBoxLayout* mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ===== Sidebar =====
    sidebar = new Sidebar;

    // ===== Страницы =====
    stack = new QStackedWidget;

    auto* homePage = new HomePage;
    auto* libraryPage = new LibraryPage;
    stack->addWidget(homePage);
    stack->addWidget(libraryPage);
    stack->addWidget(createPage("Коллекции"));
    stack->addWidget(createPage("Профиль"));
    stack->addWidget(createPage("Настройки"));

    bookInfoPage = new BookInfoPage;
    stack->addWidget(bookInfoPage); // index 5

    // ===== ДОБАВЛЯЕМ В layout =====
    mainLayout->addWidget(sidebar);
    //mainLayout->addWidget(scroll);
    mainLayout->addWidget(stack);

    root->addWidget(topBar);
    root->addLayout(mainLayout);

    // ===== ПЕРЕКЛЮЧЕНИЕ СТРАНИЦ =====
    connect(sidebar, &Sidebar::pageSelected,
        this, [=](int index) {
            lastNonBookPageIndex = index;
            stack->setCurrentIndex(index);
        });

    connect(bookInfoPage, &BookInfoPage::backRequested, this, [this]() {
        stack->setCurrentIndex(lastNonBookPageIndex);
    });

    readerWindow = new ReaderWindow;
    connect(bookInfoPage, &BookInfoPage::openReaderRequested, this,
        [this](int bookId, int chapterIndex) {
            readerWindow->openBook(bookId, chapterIndex);
        });
    connect(readerWindow, &ReaderWindow::readingFinished,
        bookInfoPage, &BookInfoPage::updateProgress);


    auto openBookInfo = [this](int bookId) {
        bookInfoPage->setBookId(bookId);
        stack->setCurrentWidget(bookInfoPage);
    };

    connect(homePage, &HomePage::bookSelected, this, openBookInfo);
    connect(libraryPage, &LibraryPage::bookSelected, this, openBookInfo);
    connect(readerWindow, &ReaderWindow::readingProgressChanged, libraryPage, &LibraryPage::reloadBooks);

    connect(sidebar, &Sidebar::bookAdded, this, [=](int bookId) {
        homePage->reloadBooks();
        libraryPage->reloadBooks();
        lastNonBookPageIndex = 1;
        sidebar->setActive(1);
        openBookInfo(bookId);
    });

    // ===== КНОПКИ ОКНА =====
    connect(topBar, &TopBar::closeClicked, this, &QWidget::close);

    connect(topBar, &TopBar::minimizeClicked,
        this, &QWidget::showMinimized);

    connect(topBar, &TopBar::maximizeClicked, this, [=]() mutable {

        if (isMaximized())
        {
            //resize(100, 700);
            showNormal();
        }
        else
        {
            showMaximized();
        }

        });

    connect(topBar, &TopBar::toggleSidebarClicked,
        sidebar, &Sidebar::toggleSidebar);

    // ===== СТИЛЬ =====
    setStyleSheet("background:#110E0A;"); //общий фон
}
/*
void MainWindow::mousePressEvent(QMouseEvent* event)
{
    if (isMaximized() || !isWindowedMode)
        return;

    if (event->pos().y() > dragHeight)
        return;

    dragging = true;
    dragPosition = event->globalPosition().toPoint()
        - frameGeometry().topLeft();
}

void MainWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (!dragging || isMaximized() || !isWindowedMode)
        return;

    move(event->globalPosition().toPoint() - dragPosition);
}

void MainWindow::mouseReleaseEvent(QMouseEvent*)
{
    dragging = false;
}
*/
