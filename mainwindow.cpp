#include "mainwindow.h"
#include "sidebar.h"
#include "topbar.h"
#include "homepage.h"
#include "librarypage.h"
#include "bookinfopage.h"
#include "readerwindow.h"
#include "accountpage.h"

#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QFrame>
#include <QApplication>
#include <QScreen>
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

// ==== СТРАНИЦА СПРАВКИ ====
QWidget* createHelpPage()
{
    auto* page = new QWidget;
    page->setStyleSheet("background:#110E0A;");

    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(R"(
        QScrollArea { background: transparent; border: none; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 0px; }
        QScrollBar::handle:vertical { background: #4a4038; border-radius: 5px; min-height: 28px; }
        QScrollBar::handle:vertical:hover { background: #5e5249; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
    )");

    auto* content = new QWidget;
    content->setStyleSheet("background:transparent;");

    auto* col = new QVBoxLayout(content);
    col->setContentsMargins(40, 32, 40, 40);
    col->setSpacing(0);

    auto* text = new QLabel;
    text->setWordWrap(true);
    text->setTextFormat(Qt::RichText);
    text->setAlignment(Qt::AlignTop);
    text->setMaximumWidth(900);
    text->setStyleSheet("background:transparent;");
    text->setText(R"HTML(
<div style="color:#d2c7bb; font-size:15px; line-height:160%;">
  <div style="color:#ff8c00; font-size:30px; font-weight:bold;">Справка</div>
  <div style="color:#8c847a; font-size:14px; margin-top:6px;">
    Краткое руководство по возможностям приложения для чтения книг&nbsp;FB2.
  </div>

  <div style="color:#ff8c00; font-size:21px; font-weight:bold; margin-top:26px;">🏠 Главная</div>
  <p style="margin-top:6px;">Стартовый экран с быстрым доступом к чтению.</p>
  <ul>
    <li>Крупный баннер с книгой, которую вы читали последней — нажмите, чтобы сразу продолжить чтение.</li>
    <li>Список недавних книг — открывает страницу книги с информацией о ней.</li>
    <li>Действия с карточек (избранное, удаление) сразу отражаются в библиотеке.</li>
  </ul>

  <div style="color:#ff8c00; font-size:21px; font-weight:bold; margin-top:22px;">📚 Библиотека</div>
  <p style="margin-top:6px;">Все добавленные книги в одном месте.</p>
  <ul>
    <li>Счётчик общего количества книг.</li>
    <li>Поиск по названию и автору.</li>
    <li>Сортировка по названию, автору, дате добавления и прогрессу.</li>
    <li>Переключение вида: сетка обложек или список.</li>
    <li>Меню каждой книги: «В избранное», «Отметить прочитанной», «Удалить» (с подтверждением).</li>
  </ul>

  <div style="color:#ff8c00; font-size:21px; font-weight:bold; margin-top:22px;">🗂 Избранное</div>
  <p style="margin-top:6px;">Те же возможности, что и в библиотеке, но показываются только книги,
  отмеченные как избранные. Состояние синхронизируется с библиотекой и главной.</p>

  <div style="color:#ff8c00; font-size:21px; font-weight:bold; margin-top:22px;">➕ Добавить книгу</div>
  <p style="margin-top:6px;">Кнопка в боковом меню открывает выбор файла. Поддерживается формат
  <b>.fb2</b>: извлекаются название, автор, обложка и текст по главам.</p>

  <div style="color:#ff8c00; font-size:21px; font-weight:bold; margin-top:22px;">📖 Страница книги</div>
  <p style="margin-top:6px;">Открывается при выборе книги — вся информация и прогресс.</p>
  <ul>
    <li>Обложка, название, автор, число глав и страниц.</li>
    <li>«Начать / Продолжить читать» — открывает книгу в окне читалки.</li>
    <li>«Завершить чтение» — отмечает книгу прочитанной (или сбрасывает прогресс).</li>
    <li>Полоса прогресса с процентом и числом прочитанных страниц.</li>
    <li>Содержание: список глав; глава, на которой вы остановились, подсвечена. Клик по главе открывает её.</li>
    <li>Справа — вкладки «Закладки» и «Цитаты». Клик по записи открывает книгу на нужной странице, «✕» — удаляет.</li>
    <li>Стрелка «‹» в верхнем меню — возврат на предыдущую вкладку.</li>
  </ul>

  <div style="color:#ff8c00; font-size:21px; font-weight:bold; margin-top:22px;">📕 Читалка</div>
  <p style="margin-top:6px;">Отдельное окно для чтения. Можно открыть несколько книг одновременно —
  каждая в своём окне.</p>
  <ul>
    <li>Текст разбит на страницы и показывается разворотом по две страницы.</li>
    <li>Перелистывание: колесо мыши, клик по левой/правой половине или по краю экрана.</li>
    <li>Верхнее меню: «О книге», «Настройки», флажок закладки, вкладки «Содержание», «Закладки», «Цитаты».</li>
    <li>Настройки чтения: шрифт, размер текста, межстрочный интервал, выравнивание и сброс к значениям по умолчанию.</li>
    <li>Закладка: флажок добавляет/убирает закладку на текущей странице (оранжевый — закладка стоит).</li>
    <li>Цитаты: выделите текст мышью и выберите цвет. Правый клик по цитате — изменить цвет или удалить.</li>
    <li>Внизу — индикатор прогресса. Окно сворачивается, разворачивается и закрывается своими кнопками.</li>
    <li>Прогресс и последняя открытая страница сохраняются автоматически при закрытии.</li>
  </ul>

  <div style="color:#ff8c00; font-size:21px; font-weight:bold; margin-top:22px;">👤 Аккаунт</div>
  <p style="margin-top:6px;">Регистрация и вход в личный аккаунт.</p>
  <ul>
    <li>В гостевом режиме доступна форма входа и регистрации (логин и пароль).</li>
    <li>После входа книги, добавленные в гостевом режиме, привязываются к вашему аккаунту.</li>
    <li>В аккаунте показывается профиль и кнопка «Выйти из аккаунта».</li>
    <li>После выхода приложение возвращается в гостевой режим — видны только гостевые книги.</li>
  </ul>
</div>
)HTML");

    col->addWidget(text);
    col->addStretch();

    scroll->setWidget(content);
    root->addWidget(scroll);

    return page;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // УБИРАЕМ СИСТЕМНОЕ ОКНО (флаги ставим ДО показа — иначе смена флагов
    // после showMaximized() сбивает состояние окна и кнопка "развернуть"
    // не возвращает его в оконный режим).
    setWindowFlags(Qt::FramelessWindowHint);

    // Минимальный размер, чтобы содержимое не съезжало в оконном режиме.
    setMinimumSize(1024, 680);
    // Геометрия для оконного режима (на неё возвращает кнопка "развернуть").
    resize(1280, 800);

    // Для безрамочного окна не используем showMaximized()/showNormal() —
    // они работают ненадёжно. Вместо этого управляем геометрией вручную:
    // «развёрнуто» = занять рабочую область экрана, «окно» = центр. прямоугольник.
    {
        const int w = 1280, h = 800;
        QRect avail = QApplication::primaryScreen()->availableGeometry();
        normalGeom = QRect(
            avail.x() + (avail.width() - w) / 2,
            avail.y() + (avail.height() - h) / 2,
            w, h);
        setGeometry(avail);   // стартуем в «развёрнутом» виде
    }
    windowMaximized = true;

    QWidget* central = new QWidget;
    setCentralWidget(central);

    // ===== ГЛАВНЫЙ ВЕРТИКАЛЬНЫЙ LAYOUT =====
    QVBoxLayout* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ===== TOP BAR =====
    TopBar* topBar = new TopBar;
    topBarWidget = topBar;
    topBar->installEventFilter(this); // перетаскивание окна за панель

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
    auto* favoritesPage = new LibraryPage(/*favoritesOnly*/ true);
    auto* accountPage = new AccountPage;
    stack->addWidget(homePage);
    stack->addWidget(libraryPage);
    stack->addWidget(favoritesPage);
    stack->addWidget(accountPage);             // index 3 — Аккаунт
    stack->addWidget(createHelpPage());        // index 4 — Справка

    bookInfoPage = new BookInfoPage;
    stack->addWidget(bookInfoPage); // index 5 (открывается по указателю)

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

    connect(topBar, &TopBar::backClicked, this, [this]() {
        stack->setCurrentIndex(lastNonBookPageIndex);
    });

    // Кнопка "назад" в верхнем меню видна только на странице книги.
    connect(stack, &QStackedWidget::currentChanged, this, [=](int) {
        topBar->setBackVisible(stack->currentWidget() == bookInfoPage);
    });

    // Каждое открытие книги создаёт НОВОЕ окно ридера — можно читать
    // несколько книг одновременно. Окно само удаляется при закрытии
    // (WA_DeleteOnClose), поэтому указатель не храним.
    auto openReader = [this, homePage, libraryPage, favoritesPage](int bookId, int chapterIndex, int page) {
        auto* rw = new ReaderWindow;
        connect(rw, &ReaderWindow::readingFinished,
            bookInfoPage, &BookInfoPage::updateProgress);
        connect(rw, &ReaderWindow::readingProgressChanged,
            homePage, &HomePage::reloadBooks);
        connect(rw, &ReaderWindow::readingProgressChanged,
            libraryPage, &LibraryPage::reloadBooks);
        connect(rw, &ReaderWindow::readingProgressChanged,
            favoritesPage, &LibraryPage::reloadBooks);
        rw->openBook(bookId, chapterIndex, page);
    };

    connect(bookInfoPage, &BookInfoPage::openReaderRequested, this,
        [openReader](int bookId, int chapterIndex) {
            openReader(bookId, chapterIndex, -1);
        });
    connect(bookInfoPage, &BookInfoPage::openReaderAtPageRequested, this,
        [openReader](int bookId, int page) {
            openReader(bookId, -1, page);
        });
    connect(homePage, &HomePage::openReaderRequested, this,
        [openReader](int bookId) {
            openReader(bookId, -1, -1);
        });


    auto openBookInfo = [this](int bookId) {
        bookInfoPage->setBookId(bookId);
        stack->setCurrentWidget(bookInfoPage);
    };

    connect(homePage, &HomePage::bookSelected, this, openBookInfo);
    connect(libraryPage, &LibraryPage::bookSelected, this, openBookInfo);
    connect(favoritesPage, &LibraryPage::bookSelected, this, openBookInfo);
    connect(libraryPage, &LibraryPage::booksChanged, homePage, &HomePage::reloadBooks);

    // Избранное и библиотека держат состояние книг синхронным
    connect(libraryPage, &LibraryPage::booksChanged, favoritesPage, &LibraryPage::reloadBooks);
    connect(favoritesPage, &LibraryPage::booksChanged, libraryPage, &LibraryPage::reloadBooks);
    connect(favoritesPage, &LibraryPage::booksChanged, homePage, &HomePage::reloadBooks);

    // Действия с карточек на главной обновляют библиотеку и избранное
    connect(homePage, &HomePage::booksChanged, libraryPage, &LibraryPage::reloadBooks);
    connect(homePage, &HomePage::booksChanged, favoritesPage, &LibraryPage::reloadBooks);

    // Вход/выход из аккаунта меняет набор книг (гостевые ↔ привязанные к учётке).
    auto reloadAllBooks = [homePage, libraryPage, favoritesPage]() {
        homePage->reloadBooks();
        libraryPage->reloadBooks();
        favoritesPage->reloadBooks();
    };
    connect(accountPage, &AccountPage::loggedIn, this, reloadAllBooks);
    connect(accountPage, &AccountPage::loggedOut, this, reloadAllBooks);

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

    connect(topBar, &TopBar::maximizeClicked, this, [this]() {

        // Управляем геометрией вручную (безрамочное окно): без showMaximized().
        if (windowMaximized)
        {
            setGeometry(normalGeom);   // в оконный режим
            windowMaximized = false;
        }
        else
        {
            normalGeom = geometry();   // запоминаем оконную геометрию
            setGeometry(QApplication::primaryScreen()->availableGeometry());
            windowMaximized = true;
        }

        });

    connect(topBar, &TopBar::toggleSidebarClicked,
        sidebar, &Sidebar::toggleSidebar);

    // Держим стрелку "назад" выровненной по левому краю обложки при
    // сворачивании/разворачивании боковой панели.
    connect(topBar, &TopBar::toggleSidebarClicked, this, [=]() {
        topBar->setBackOffset(sidebar->width());
    });

    // ===== СТИЛЬ =====
    setStyleSheet("background:#110E0A;"); //общий фон
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    // Перетаскивание окна за верхнюю панель (только в оконном режиме).
    if (obj == topBarWidget)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && !windowMaximized)
            {
                dragging = true;
                dragPosition = me->globalPosition().toPoint() - frameGeometry().topLeft();
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            auto* me = static_cast<QMouseEvent*>(event);
            if (dragging && (me->buttons() & Qt::LeftButton) && !windowMaximized)
            {
                move(me->globalPosition().toPoint() - dragPosition);
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            dragging = false;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}
