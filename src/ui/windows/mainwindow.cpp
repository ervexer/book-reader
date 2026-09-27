#include "ui/windows/mainwindow.h"
#include "ui/widgets/sidebar.h"
#include "ui/widgets/topbar.h"
#include "ui/pages/homepage.h"
#include "ui/pages/librarypage.h"
#include "ui/pages/bookinfopage.h"
#include "ui/windows/readerwindow.h"
#include "ui/pages/accountpage.h"

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

// оформление страниц
QWidget* createPage(const QString& text)
{
    QWidget* page = new QWidget;

    QVBoxLayout* layout = new QVBoxLayout(page);

    QLabel* label = new QLabel(text);
    label->setStyleSheet("color:white; font-size:24px;");

    layout->addStretch();
    layout->addWidget(label, 0, Qt::AlignCenter);
    layout->addStretch();

    page->setStyleSheet("background:#110E0A;");

    return page;
}

// справка
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
    col->setContentsMargins(40, 10, 40, 40);
    col->setSpacing(0);

    auto* text = new QLabel;
    text->setWordWrap(true);
    text->setTextFormat(Qt::RichText);
    text->setAlignment(Qt::AlignTop);
    text->setMaximumWidth(900);
    text->setStyleSheet("background:transparent;");
    text->setText(R"HTML(
<div style="color:#d2c7bb; font-size:15px; line-height:155%;">
  <div style="color:#ff8c00; font-size:30px; font-weight:bold;">Справка</div>
  <div style="color:#8c847a; font-size:14px; margin-top:6px;">
    Руководство по работе с программой для чтения книг формата&nbsp;FB2.
  </div>

  <div style="color:#ff8c00; font-size:20px; font-weight:bold; margin-top:24px;">Боковое меню и панель окна</div>
  <p style="margin-top:4px;">Слева расположено меню для перехода между разделами.</p>
  <ul style="margin-top:2px;">
    <li>Кнопка-меню в левом верхнем углу — сворачивает и разворачивает боковую панель.</li>
    <li>«Главная», «Библиотека», «Избранное» — переключают разделы программы.</li>
    <li>«Добавить книгу» — открывает выбор файла для добавления книги.</li>
    <li>«Аккаунт» — вход и регистрация, «Справка» — этот экран.</li>
    <li>Кнопки в правом верхнем углу — свернуть, развернуть и закрыть окно.</li>
  </ul>

  <div style="color:#ff8c00; font-size:20px; font-weight:bold; margin-top:18px;">Главная</div>
  <p style="margin-top:4px;">На этом экране отображаются недавно читаемые и недавно добавленные книги.</p>
  <ul style="margin-top:2px;">
    <li>Верхний блок — последняя открытая книга с её названием, автором и описанием.</li>
    <li>«Продолжить читать» — открывает окно чтения с того места, где вы остановились.</li>
    <li>«Подробнее» — открывает окно информации о книге.</li>
    <li>Ниже — вкладки «Недавно читали» и «Недавно добавлено» со списком книг.</li>
    <li>Нажатие на книгу открывает окно информации о ней.</li>
    <li>Кнопка меню на карточке открывает действия: «В избранное», «Отметить прочитанным», «Удалить».</li>
  </ul>

  <div style="color:#ff8c00; font-size:20px; font-weight:bold; margin-top:18px;">Библиотека</div>
  <p style="margin-top:4px;">На этом экране собраны все добавленные книги, сверху — их количество.</p>
  <ul style="margin-top:2px;">
    <li>Поле поиска — находит книгу по названию или автору.</li>
    <li>Кнопка сортировки — по дате добавления, автору, названию или прогрессу.</li>
    <li>Кнопки вида — переключают между сеткой обложек и списком.</li>
    <li>Кнопка меню на книге открывает действия: «В избранное», «Отметить прочитанным», «Удалить».</li>
    <li>Нажатие на книгу открывает окно информации о ней.</li>
  </ul>

  <div style="color:#ff8c00; font-size:20px; font-weight:bold; margin-top:18px;">Избранное</div>
  <p style="margin-top:4px;">На этом экране показаны только книги, отмеченные как избранные.</p>
  <ul style="margin-top:2px;">
    <li>Возможности поиска, сортировки и вида — те же, что в библиотеке.</li>
    <li>«Убрать из избранного» в меню книги убирает её из этого списка.</li>
  </ul>

  <div style="color:#ff8c00; font-size:20px; font-weight:bold; margin-top:18px;">Окно информации о книге</div>
  <p style="margin-top:4px;">В этом окне показаны сведения о книге и прогресс чтения.</p>
  <ul style="margin-top:2px;">
    <li>Обложка, название, автор, число глав и страниц, полоса прогресса.</li>
    <li>«Начать / Продолжить читать» — открывает окно чтения.</li>
    <li>«Завершить чтение» — отмечает книгу прочитанной.</li>
    <li>В содержании указаны главы с номерами страниц; нажатие открывает главу.</li>
    <li>Справа — закладки и цитаты; нажатие открывает нужную страницу.</li>
    <li>Стрелка «назад» сверху — возврат к предыдущему разделу.</li>
  </ul>

  <div style="color:#ff8c00; font-size:20px; font-weight:bold; margin-top:18px;">Окно чтения</div>
  <p style="margin-top:4px;">На этом экране выполняется чтение книги.</p>
  <ul style="margin-top:2px;">
    <li>Текст показывается разворотом из двух страниц.</li>
    <li>Перелистывание — колесо мыши или нажатие по краям разворота.</li>
    <li>Кнопки верхней панели — справка, сведения о книге, настройки, закладка, содержание, закладки, цитаты.</li>
    <li>Кнопка-флажок добавляет или убирает закладку на текущей странице.</li>
    <li>Прогресс и последняя страница сохраняются автоматически при закрытии окна.</li>
  </ul>

  <div style="color:#ff8c00; font-size:20px; font-weight:bold; margin-top:18px;">Аккаунт</div>
  <p style="margin-top:4px;">На этом экране выполняется регистрация и вход в личный аккаунт.</p>
  <ul style="margin-top:2px;">
    <li>«Зарегистрироваться» — создаёт аккаунт по логину и паролю.</li>
    <li>«Войти» — выполняет вход; книги из гостевого режима привязываются к аккаунту.</li>
    <li>«Выйти из аккаунта» — возвращает программу в гостевой режим.</li>
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
    setWindowFlags(Qt::FramelessWindowHint);
    setMinimumSize(1024, 680);
    resize(1280, 800);
    {
        const int w = 1280, h = 800;
        QRect avail = QApplication::primaryScreen()->availableGeometry();
        normalGeom = QRect(
            avail.x() + (avail.width() - w) / 2,
            avail.y() + (avail.height() - h) / 2,
            w, h);
        setGeometry(avail); // изначально в развернутом виде
    }
    windowMaximized = true;

    QWidget* central = new QWidget;
    setCentralWidget(central);

    // главный layout
    QVBoxLayout* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // верхнее меню
    TopBar* topBar = new TopBar;
    topBarWidget = topBar;
    topBar->installEventFilter(this); // перетаскивание окна

    // основной контент страницы
    QHBoxLayout* mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    sidebar = new Sidebar;
    // страницы
    stack = new QStackedWidget;

    auto* homePage = new HomePage;
    auto* libraryPage = new LibraryPage;
    auto* favoritesPage = new LibraryPage(true);
    auto* accountPage = new AccountPage;
    stack->addWidget(homePage);
    stack->addWidget(libraryPage);
    stack->addWidget(favoritesPage);
    stack->addWidget(accountPage);
    stack->addWidget(createHelpPage());

    bookInfoPage = new BookInfoPage;
    stack->addWidget(bookInfoPage);

    mainLayout->addWidget(sidebar);
    mainLayout->addWidget(stack);

    root->addWidget(topBar);
    root->addLayout(mainLayout);

    // переключение страниц
    connect(sidebar, &Sidebar::pageSelected,
        this, [=](int index) {
            lastNonBookPageIndex = index;
            stack->setCurrentIndex(index);
        });

    connect(topBar, &TopBar::backClicked, this, [this]() {
        stack->setCurrentIndex(lastNonBookPageIndex);
    });

    // кнопка назад
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

    // Вход/выход из аккаунта меняет набор книг (гостевые и привязанные к учётке).
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

    // кнопки верхнего меню
    connect(topBar, &TopBar::closeClicked, this, &QWidget::close);

    connect(topBar, &TopBar::minimizeClicked,
        this, &QWidget::showMinimized);

    connect(topBar, &TopBar::maximizeClicked, this, [this]() {

        if (windowMaximized)
        {
            setGeometry(normalGeom); // в оконный режим
            windowMaximized = false;
        }
        else
        {
            normalGeom = geometry(); // запоминаем оконную геометрию
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
