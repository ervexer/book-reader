#include "ui/pages/librarypage.h"
#include "data/database.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QIcon>
#include <QSize>
#include <QStackedWidget>
#include <QLineEdit>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QScrollArea>
#include <QMouseEvent>
#include <QEvent>
#include <QToolButton>
#include <QDialog>
#include <QPixmap>
#include <QPixmapCache>
#include <algorithm>
#include <functional>

// ===== Действия над карточкой книги =====
struct CardActions
{
    std::function<void()> open;
    std::function<void()> toggleFav;
    std::function<void()> markRead;
    std::function<void()> remove;
    bool favorite = false;
    bool read = false; // книга полностью прочитана (прогресс на максимуме)
};

static QPixmap loadCoverPixmap(int bookId, const QSize& targetSize)
{
    // Кеш по bookId + размеру: при перестроении списка (лайк/прочитано/удаление,
    // переключение вкладок) обложки не декодируются и не масштабируются повторно.
    const QString cacheKey = QString("cover_%1_%2x%3")
        .arg(bookId).arg(targetSize.width()).arg(targetSize.height());

    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached))
        return cached;

    const QByteArray coverBytes = Database::instance().getBookCover(bookId, nullptr);
    QPixmap original;

    if (!coverBytes.isEmpty() && original.loadFromData(coverBytes))
    {
        const QPixmap scaled =
            original.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmapCache::insert(cacheKey, scaled);
        return scaled;
    }

    return QPixmap();
}

// Корректная форма слова "книга" для числа
static QString bookCountWord(int n)
{
    const int n10 = n % 10;
    const int n100 = n % 100;

    if (n10 == 1 && n100 != 11)
        return "книга";
    if (n10 >= 2 && n10 <= 4 && !(n100 >= 12 && n100 <= 14))
        return "книги";
    return "книг";
}

// Фильтр клика по карточке (клики по кнопкам внутри не перехватываются)
class ClickFilter : public QObject
{
public:
    std::function<void()> click;

    bool eventFilter(QObject* obj, QEvent* ev) override
    {
        if (ev->type() == QEvent::MouseButtonPress)
        {
            if (click) click();
            return true;
        }
        return QObject::eventFilter(obj, ev);
    }
};

// Стиль выпадающего меню (серый, как фон карточки)
static const char* kCardMenuStyle = R"(
    QMenu {
        background:#2a2a2a;
        border:1px solid #3a3a3a;
        border-radius:10px;
        padding:6px;
        color:#e0e0e0;
        font-size:13px;
    }
    QMenu::item {
        padding:9px 18px;
        border-radius:6px;
    }
    QMenu::item:selected {
        background:#3a3a3a;
        color:#ff8c00;
    }
)";

// Наполняет меню действиями книги
static void fillCardMenu(QMenu* menu, const CardActions& a)
{
    QAction* fav = menu->addAction(a.favorite ? "Убрать из избранного" : "В избранное");
    QObject::connect(fav, &QAction::triggered, [cb = a.toggleFav]() { if (cb) cb(); });

    QAction* read = menu->addAction(a.read ? "Сбросить прогресс" : "Отметить прочитанным");
    QObject::connect(read, &QAction::triggered, [cb = a.markRead]() { if (cb) cb(); });

    QAction* del = menu->addAction("Удалить");
    QObject::connect(del, &QAction::triggered, [cb = a.remove]() { if (cb) cb(); });
}

// ===== GRID CARD =====
static QWidget* createBookCard(const DbBook& book, const CardActions& actions)
{
    QWidget* container = new QWidget;
    container->setFixedSize(150, 310);
    container->setCursor(Qt::PointingHandCursor);

    QVBoxLayout* vbox = new QVBoxLayout(container);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(6);

    // Контейнер обложки (для прогресса и кнопки-меню поверх)
    QWidget* coverContainer = new QWidget;
    coverContainer->setFixedSize(150, 210);

    QVBoxLayout* coverContainerLayout = new QVBoxLayout(coverContainer);
    coverContainerLayout->setContentsMargins(0, 0, 0, 0);
    coverContainerLayout->setSpacing(0);

    QLabel* coverLabel = new QLabel;
    coverLabel->setFixedSize(150, 206);
    coverLabel->setAlignment(Qt::AlignCenter);
    coverLabel->setStyleSheet(R"(
        QLabel { background:#1e1e1e; border-radius:8px; }
        QLabel:hover { background:#262626; }
    )");

    QPixmap cover = loadCoverPixmap(book.id, QSize(150, 206));
    if (!cover.isNull())
    {
        coverLabel->setPixmap(cover);
        coverLabel->setScaledContents(false);
    }
    else
    {
        coverLabel->setText("Нет\nобложки");
        coverLabel->setStyleSheet(coverLabel->styleSheet() + "color:#666; font-size:11px;");
    }

    coverContainerLayout->addWidget(coverLabel);

    // Полоса прогресса поверх нижней части обложки
    QFrame* progress = new QFrame;
    progress->setFixedHeight(4);
    progress->setStyleSheet("background:#444; border-radius:2px; margin-top:-4px;");

    if (book.totalPages > 0)
    {
        const double ratio = qBound(0.0, (double)book.lastPage / (double)book.totalPages, 1.0);
        QFrame* fill = new QFrame(progress);
        fill->setGeometry(0, 0, int(150 * ratio), 4);
        fill->setStyleSheet("background:#ff8c00; border-radius:2px;");
    }

    coverContainerLayout->addWidget(progress);

    // Кнопка-меню (три точки) в правом верхнем углу обложки
    QToolButton* menuBtn = new QToolButton(coverContainer);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setFixedSize(28, 28);
    menuBtn->setIcon(QIcon(":/icons/menu-dots.png"));
    menuBtn->setIconSize(QSize(16, 16));
    menuBtn->setPopupMode(QToolButton::InstantPopup);
    menuBtn->setStyleSheet(R"(
        QToolButton { background:rgba(20,20,20,0.65); border:none; border-radius:8px; }
        QToolButton:hover { background:rgba(42,42,42,0.95); }
        QToolButton::menu-indicator { image:none; width:0; }
    )");
    menuBtn->move(150 - 28 - 6, 6);
    menuBtn->raise();

    QMenu* cardMenu = new QMenu(menuBtn);
    cardMenu->setStyleSheet(kCardMenuStyle);
    fillCardMenu(cardMenu, actions);
    menuBtn->setMenu(cardMenu);

    vbox->addWidget(coverContainer);

    QLabel* titleLabel = new QLabel(book.title);
    titleLabel->setStyleSheet("color:#fff; font-size:13px; font-weight:600; background:transparent;");
    titleLabel->setWordWrap(true);
    titleLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    titleLabel->setFixedWidth(150);

    QLabel* authorLabel = new QLabel(book.author);
    authorLabel->setStyleSheet("color:#888; font-size:11px; background:transparent;");
    authorLabel->setWordWrap(true);
    authorLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    authorLabel->setFixedWidth(150);

    vbox->addWidget(titleLabel);
    vbox->addWidget(authorLabel);
    vbox->addStretch();

    auto* filter = new ClickFilter;
    filter->click = actions.open;
    container->installEventFilter(filter);

    return container;
}

// ===== LIST ITEM =====
static QWidget* createListItem(const DbBook& book, const CardActions& actions)
{
    QWidget* item = new QWidget;
    item->setFixedHeight(230);
    item->setStyleSheet("background:#1e1e1e; border-radius:12px;");
    item->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    item->setCursor(Qt::PointingHandCursor);

    QHBoxLayout* layout = new QHBoxLayout(item);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(16);

    QLabel* cover = new QLabel;
    cover->setFixedSize(140, 200);
    cover->setAlignment(Qt::AlignCenter);
    cover->setStyleSheet("QLabel { background:#2a2a2a; border-radius:8px; }");

    QPixmap coverPix = loadCoverPixmap(book.id, QSize(140, 200));
    if (!coverPix.isNull())
    {
        cover->setPixmap(coverPix);
        cover->setScaledContents(false);
    }
    else
    {
        cover->setText("Нет обложки");
        cover->setStyleSheet(cover->styleSheet() + "color:#777; font-size:10px;");
    }

    // CENTER
    QWidget* centerWidget = new QWidget;
    QVBoxLayout* centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(8);

    QLabel* title = new QLabel(book.title);
    title->setWordWrap(true);
    title->setStyleSheet("font-weight:600; color:white; font-size:16px;");

    QLabel* author = new QLabel(book.author);
    author->setStyleSheet("color:#888; font-size:12px;");

    centerLayout->addWidget(title);
    centerLayout->addWidget(author);
    centerLayout->addStretch();

    // PROGRESS
    QWidget* progressWidget = new QWidget;
    QVBoxLayout* progressLayout = new QVBoxLayout(progressWidget);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(6);

    QHBoxLayout* progressTop = new QHBoxLayout;
    progressTop->setContentsMargins(0, 0, 0, 0);
    QLabel* progressText = new QLabel("Прогресс чтения");
    progressText->setStyleSheet("color:#888; font-size:12px;");
    QString pages = QString("%1/%2").arg(book.lastPage).arg(book.totalPages);
    QLabel* pageLabel = new QLabel(pages);
    pageLabel->setStyleSheet("color:#ff8c00; font-size:12px;");
    progressTop->addWidget(progressText);
    progressTop->addStretch();
    progressTop->addWidget(pageLabel);

    QFrame* progressBg = new QFrame;
    progressBg->setFixedHeight(5);
    progressBg->setStyleSheet("background:#333; border-radius:2px;");

    if (book.totalPages > 0)
    {
        const double ratio = qBound(0.0, (double)book.lastPage / (double)book.totalPages, 1.0);
        QFrame* fill = new QFrame(progressBg);
        fill->setStyleSheet("background:#ff8c00; border-radius:2px;");
        fill->setGeometry(0, 0, 100, 5);

        // Ширина заполнения пересчитывается по фактической ширине фона
        // (она известна только после раскладки) — следим через eventFilter.
        class FillResizer : public QObject {
        public:
            QFrame* bg; QFrame* fl; double r;
            bool eventFilter(QObject* o, QEvent* e) override {
                if (e->type() == QEvent::Resize || e->type() == QEvent::Show)
                    fl->setGeometry(0, 0, int(bg->width() * r), 5);
                return QObject::eventFilter(o, e);
            }
        };
        auto* rs = new FillResizer;
        rs->bg = progressBg;
        rs->fl = fill;
        rs->r = ratio;
        rs->setParent(progressBg);
        progressBg->installEventFilter(rs);
    }

    progressLayout->addLayout(progressTop);
    progressLayout->addWidget(progressBg);
    centerLayout->addWidget(progressWidget);

    // RIGHT MENU (кнопки действий)
    QWidget* rightWidget = new QWidget;
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(12);

    auto createMenuButton = [](const QString& iconPath, const QString& tooltip) {
        QToolButton* btn = new QToolButton;
        btn->setFixedSize(34, 34);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setIcon(QIcon(iconPath));
        btn->setIconSize(QSize(18, 18));
        btn->setToolTip(tooltip);
        btn->setStyleSheet(R"(
            QToolButton { background:#2a2a2a; border:none; border-radius:8px; }
            QToolButton:hover { background:#363636; }
        )");
        return btn;
        };

    QToolButton* favBtn = createMenuButton(
        actions.favorite ? ":/icons/heart_orange.png" : ":/icons/heart.png",
        actions.favorite ? "Убрать из избранного" : "Добавить в избранное");
    if (actions.favorite)
        favBtn->setStyleSheet(R"(
            QToolButton { background:#3a2410; border:none; border-radius:8px; }
            QToolButton:hover { background:#4a2e14; }
        )");
    QToolButton* doneBtn = createMenuButton(
        actions.read ? ":/icons/check-orange.png" : ":/icons/check.png",
        actions.read ? "Сбросить прогресс" : "Отметить прочитанным");
    if (actions.read)
        doneBtn->setStyleSheet(R"(
            QToolButton { background:#3a2410; border:none; border-radius:8px; }
            QToolButton:hover { background:#4a2e14; }
        )");
    QToolButton* deleteBtn = createMenuButton(":/icons/trash.png", "Удалить");

    QObject::connect(favBtn, &QToolButton::clicked, [cb = actions.toggleFav]() { if (cb) cb(); });
    QObject::connect(doneBtn, &QToolButton::clicked, [cb = actions.markRead]() { if (cb) cb(); });
    QObject::connect(deleteBtn, &QToolButton::clicked, [cb = actions.remove]() { if (cb) cb(); });

    rightLayout->addWidget(favBtn);
    rightLayout->addWidget(doneBtn);
    rightLayout->addWidget(deleteBtn);
    rightLayout->addStretch();

    layout->addWidget(cover);
    layout->addWidget(centerWidget, 1);
    layout->addWidget(rightWidget);

    auto* f = new ClickFilter;
    f->click = actions.open;
    item->installEventFilter(f);

    return item;
}

// ===== Сборка действий для книги =====
static CardActions makeActions(const DbBook& book,
    std::function<void(int)> open,
    std::function<void(int, bool)> toggleFav,
    std::function<void(int, bool)> toggleRead,
    std::function<void(int, QString)> remove)
{
    CardActions a;
    a.favorite = book.favorite;
    a.read = book.totalPages > 0 && book.lastPage >= book.totalPages;
    const int id = book.id;
    const bool fav = book.favorite;
    const bool read = a.read;
    const QString title = book.title;
    a.open = [open, id]() { open(id); };
    a.toggleFav = [toggleFav, id, fav]() { toggleFav(id, fav); };
    a.markRead = [toggleRead, id, read]() { toggleRead(id, read); };
    a.remove = [remove, id, title]() { remove(id, title); };
    return a;
}

// ===== GRID VIEW =====
QWidget* LibraryPage::createGridView(const QVector<DbBook>& books)
{
    QWidget* wrapper = new QWidget;
    wrapper->setStyleSheet("background:transparent;");

    QVBoxLayout* outer = new QVBoxLayout(wrapper);
    outer->setContentsMargins(0, 0, 0, 0);

    QWidget* page = new QWidget;
    page->setStyleSheet("background:transparent;");

    QGridLayout* layout = new QGridLayout(page);
    layout->setHorizontalSpacing(28);
    layout->setVerticalSpacing(36);
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    const int cols = 6;
    int i = 0;

    for (const auto& b : books)
    {
        CardActions a = makeActions(b,
            [this](int id) { emit bookSelected(id); },
            [this](int id, bool fav) { doToggleFavorite(id, fav); },
            [this](int id, bool read) { doToggleRead(id, read); },
            [this](int id, QString t) { doDelete(id, t); });

        layout->addWidget(createBookCard(b, a), i / cols, i % cols, Qt::AlignTop | Qt::AlignLeft);
        i++;
        if (i >= 60)
            break;
    }

    outer->addWidget(page);
    outer->addStretch();

    return wrapper;
}

// ===== LIST VIEW =====
QWidget* LibraryPage::createListView(const QVector<DbBook>& books)
{
    QWidget* wrapper = new QWidget;
    wrapper->setStyleSheet("background:transparent;");

    QVBoxLayout* outer = new QVBoxLayout(wrapper);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    QWidget* page = new QWidget;
    page->setStyleSheet("background:transparent;");

    QGridLayout* layout = new QGridLayout(page);
    layout->setHorizontalSpacing(20);
    layout->setVerticalSpacing(15);
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    const int cols = 2;
    int i = 0;

    for (const auto& b : books)
    {
        CardActions a = makeActions(b,
            [this](int id) { emit bookSelected(id); },
            [this](int id, bool fav) { doToggleFavorite(id, fav); },
            [this](int id, bool read) { doToggleRead(id, read); },
            [this](int id, QString t) { doDelete(id, t); });

        layout->addWidget(createListItem(b, a), i / cols, i % cols);
        i++;
        if (i >= 40)
            break;
    }

    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);

    outer->addWidget(page);
    outer->addStretch();

    return wrapper;
}

// ===== Фильтрация и сортировка =====
QVector<DbBook> LibraryPage::filteredSorted() const
{
    QVector<DbBook> books;
    const QString q = searchText.trimmed();

    for (const auto& b : allBooks)
    {
        if (q.isEmpty()
            || b.title.startsWith(q, Qt::CaseInsensitive)
            || b.author.startsWith(q, Qt::CaseInsensitive))
        {
            books.push_back(b);
        }
    }

    switch (sortMode)
    {
    case 1: // по автору
        std::stable_sort(books.begin(), books.end(), [](const DbBook& a, const DbBook& b) {
            return a.author.compare(b.author, Qt::CaseInsensitive) < 0;
            });
        break;
    case 2: // по названию
        std::stable_sort(books.begin(), books.end(), [](const DbBook& a, const DbBook& b) {
            return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
            });
        break;
    case 3: // по прогрессу (убывание)
        std::stable_sort(books.begin(), books.end(), [](const DbBook& a, const DbBook& b) {
            const double ra = a.totalPages > 0 ? (double)a.lastPage / a.totalPages : 0.0;
            const double rb = b.totalPages > 0 ? (double)b.lastPage / b.totalPages : 0.0;
            return ra > rb;
            });
        break;
    default: // 0 — по дате добавления (allBooks уже в этом порядке)
        break;
    }

    return books;
}

void LibraryPage::updateSubtitle()
{
    if (!subtitleLbl)
        return;
    const int n = allBooks.size();
    if (favoritesOnly)
        subtitleLbl->setText(QString("%1 %2 в избранном").arg(n).arg(bookCountWord(n)));
    else
        subtitleLbl->setText(QString("%1 %2 в вашей библиотеке").arg(n).arg(bookCountWord(n)));
}

void LibraryPage::rebuildViews()
{
    if (!viewStack)
        return;

    const int keepIndex = currentView;

    while (viewStack->count() > 0)
    {
        QWidget* w = viewStack->widget(0);
        viewStack->removeWidget(w);
        w->deleteLater();
    }

    const QVector<DbBook> books = filteredSorted();

    viewStack->addWidget(createGridView(books));
    viewStack->addWidget(createListView(books));
    viewStack->setCurrentIndex(keepIndex);
}

// ===== Действия =====
void LibraryPage::doToggleFavorite(int bookId, bool currentlyFavorite)
{
    Database::instance().setFavorite(bookId, !currentlyFavorite, nullptr);
    reloadBooks();
    emit booksChanged();
}

void LibraryPage::doToggleRead(int bookId, bool currentlyRead)
{
    if (currentlyRead)
        Database::instance().updateLastPage(bookId, 0, nullptr); // сброс прогресса
    else
        Database::instance().markAsRead(bookId, nullptr);

    reloadBooks();
    emit booksChanged();
}

void LibraryPage::doDelete(int bookId, const QString& title)
{
    if (!confirmDelete(title))
        return;

    Database::instance().deleteBook(bookId, nullptr);
    reloadBooks();
    emit booksChanged();
}

bool LibraryPage::confirmDelete(const QString& title)
{
    QDialog dlg(this);
    dlg.setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg.setModal(true);
    dlg.setStyleSheet("QDialog { background:#1e1e1e; border-radius:14px; }");
    dlg.setFixedWidth(380);

    QVBoxLayout* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(14);

    QLabel* header = new QLabel("Удалить книгу?");
    header->setStyleSheet("color:white; font-size:18px; font-weight:600; background:transparent;");

    QLabel* text = new QLabel(QString("«%1» будет удалена из библиотеки безвозвратно.").arg(title));
    text->setWordWrap(true);
    text->setStyleSheet("color:#aaa; font-size:13px; background:transparent;");

    QHBoxLayout* btns = new QHBoxLayout;
    btns->addStretch();

    QPushButton* cancel = new QPushButton("Отмена");
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setFixedHeight(38);
    cancel->setStyleSheet(R"(
        QPushButton { background:#2a2a2a; color:#ddd; border:none; border-radius:9px; padding:0 18px; }
        QPushButton:hover { background:#363636; }
    )");

    QPushButton* ok = new QPushButton("Удалить");
    ok->setCursor(Qt::PointingHandCursor);
    ok->setFixedHeight(38);
    ok->setStyleSheet(R"(
        QPushButton { background:#c0392b; color:white; border:none; border-radius:9px; padding:0 18px; }
        QPushButton:hover { background:#d8453a; }
    )");

    btns->addWidget(cancel);
    btns->addWidget(ok);

    root->addWidget(header);
    root->addWidget(text);
    root->addLayout(btns);

    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);

    return dlg.exec() == QDialog::Accepted;
}

// ===== CONSTRUCTOR =====
LibraryPage::LibraryPage(bool favoritesOnly, QWidget* parent)
    : QWidget(parent), favoritesOnly(favoritesOnly)
{
    setStyleSheet("background:#110E0A; color:white;");

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 20, 16, 20);

    // HEADER
    QHBoxLayout* header = new QHBoxLayout;
    QVBoxLayout* titles = new QVBoxLayout;

    QLabel* title = new QLabel(favoritesOnly ? "Избранное" : "Библиотека");
    subtitleLbl = new QLabel;

    title->setStyleSheet("font-size:28px;");
    subtitleLbl->setStyleSheet("color:#aaa;");

    titles->addWidget(title);
    titles->setContentsMargins(5, 0, 0, 0);
    titles->addWidget(subtitleLbl);

    header->addLayout(titles);
    header->setContentsMargins(5, 0, 0, 0);
    header->addStretch();

    QString activeStyle = "QPushButton { background:#3a3a3a; border-radius:8px; }";
    QString inactiveStyle = "QPushButton { background:#1e1e1e; border-radius:8px; }";

    // SEARCH
    QLineEdit* search = new QLineEdit;
    search->setFixedHeight(50);
    search->setMaximumWidth(600);
    search->setPlaceholderText(favoritesOnly ? "Поиск по избранному" : "Поиск по библиотеке");
    search->setClearButtonEnabled(true);
    search->setStyleSheet(R"(
        QLineEdit { background:#000000; border-radius:10px; padding-left:10px; color:white; font-size:14px; }
    )");

    QAction* searchIcon = new QAction(QIcon(":/icons/search.png"), "", search);
    search->addAction(searchIcon, QLineEdit::LeadingPosition);

    connect(search, &QLineEdit::textChanged, this, [this](const QString& t) {
        searchText = t;
        rebuildViews();
        });

    // SORT
    QPushButton* sortBtn = new QPushButton;
    sortBtn->setFixedSize(40, 40);
    sortBtn->setIcon(QIcon(":/icons/sort.png"));
    sortBtn->setIconSize(QSize(20, 20));
    sortBtn->setCursor(Qt::PointingHandCursor);
    sortBtn->setStyleSheet(inactiveStyle + "QPushButton::menu-indicator { image: none; width: 0; }");

    QMenu* sortMenu = new QMenu(this);
    sortMenu->setStyleSheet(R"(
        QMenu {
            background:#241d15;
            border:1px solid #3a2f22;
            border-radius:12px;
            padding:8px;
            color:#e7ddd2;
            font-size:14px;
        }
        QMenu::item {
            padding:11px 36px 11px 20px;
            border-radius:8px;
            margin:2px 0;
        }
        QMenu::item:selected { background:#3a2f22; }
        QMenu::item:checked { color:#ff8c00; }
        QMenu::indicator {
            width:16px;
            height:16px;
            margin-right:8px;
        }
        QMenu::indicator:checked {
            image: url(":/icons/check-orange.png");
        }
        QMenu::indicator:unchecked { image: none; }
    )");

    QActionGroup* sortGroup = new QActionGroup(this);
    sortGroup->setExclusive(true);

    const QStringList sortNames = {
        "По дате добавления", "По автору", "По названию", "По прогрессу"
    };
    for (int i = 0; i < sortNames.size(); ++i)
    {
        QAction* act = sortMenu->addAction(sortNames[i]);
        act->setCheckable(true);
        act->setChecked(i == 0);
        sortGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, i]() {
            sortMode = i;
            rebuildViews();
            });
    }

    sortBtn->setMenu(sortMenu);

    // VIEW BUTTONS
    QPushButton* gridBtn = new QPushButton;
    QPushButton* listBtn = new QPushButton;
    gridBtn->setFixedSize(40, 40);
    listBtn->setFixedSize(40, 40);
    gridBtn->setCursor(Qt::PointingHandCursor);
    listBtn->setCursor(Qt::PointingHandCursor);
    gridBtn->setIcon(QIcon(":/icons/grid.png"));
    listBtn->setIcon(QIcon(":/icons/list.png"));
    gridBtn->setIconSize(QSize(20, 20));
    listBtn->setIconSize(QSize(20, 20));
    gridBtn->setStyleSheet(activeStyle);
    listBtn->setStyleSheet(inactiveStyle);

    header->addWidget(search);
    header->addSpacing(10);
    header->addWidget(sortBtn);
    header->addSpacing(10);
    header->addWidget(gridBtn);
    header->addWidget(listBtn);

    root->addLayout(header);

    // STACK + SCROLL
    viewStack = new QStackedWidget;
    viewStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QWidget* scrollContent = new QWidget;
    scrollContent->setStyleSheet("background:transparent;");

    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->addWidget(viewStack);

    QScrollArea* scroll = new QScrollArea;
    scroll->setWidget(scrollContent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet("background:transparent;");

    root->addWidget(scroll);

    qApp->setStyleSheet(R"(
        QScrollBar:vertical { background: transparent; width: 5px; border: none; }
        QScrollBar::handle:vertical { background: rgba(160, 82, 45, 200); border-radius: 5px; min-height: 30px; }
        QScrollBar::handle:vertical:hover { background: rgba(255, 160, 40, 220); }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; background: none; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QToolTip {
            background-color: #2a2a2a; color: white; border: 1px solid #444;
            padding: 6px 10px; border-radius: 6px; font-size: 12px;
        }
    )");

    // SWITCH LOGIC
    connect(gridBtn, &QPushButton::clicked, this, [=]() {
        currentView = 0;
        viewStack->setCurrentIndex(0);
        gridBtn->setStyleSheet(activeStyle);
        listBtn->setStyleSheet(inactiveStyle);
        });

    connect(listBtn, &QPushButton::clicked, this, [=]() {
        currentView = 1;
        viewStack->setCurrentIndex(1);
        listBtn->setStyleSheet(activeStyle);
        gridBtn->setStyleSheet(inactiveStyle);
        });

    reloadBooks();
}

// ===== RELOAD =====
void LibraryPage::reloadBooks()
{
    Database::instance().openOrCreate();
    allBooks = Database::instance().listBooks(nullptr);

    if (favoritesOnly)
    {
        QVector<DbBook> favs;
        for (const auto& b : allBooks)
            if (b.favorite)
                favs.push_back(b);
        allBooks = favs;
    }

    updateSubtitle();
    rebuildViews();
}
