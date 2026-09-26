#include "homepage.h"
#include "database.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QFrame>
#include <QMouseEvent>
#include <QFontDatabase>
#include <QEvent>
#include <QtGlobal>
#include <QPixmap>
#include <QPixmapCache>
#include <QByteArray>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QIcon>

#include <functional>

struct HomeCardActions
{
    std::function<void()> open;
    std::function<void()> toggleFav;
    std::function<void()> toggleRead;
    std::function<void()> remove;
    bool favorite = false;
    bool read = false;
};

// Стиль выпадающего меню (серый, как фон карточки)
static const char* kHomeCardMenuStyle = R"(
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

static void fillHomeCardMenu(QMenu* menu, const HomeCardActions& a)
{
    QAction* fav = menu->addAction(a.favorite ? "Убрать из избранного" : "В избранное");
    QObject::connect(fav, &QAction::triggered, [cb = a.toggleFav]() { if (cb) cb(); });

    QAction* read = menu->addAction(a.read ? "Сбросить прогресс" : "Отметить прочитанным");
    QObject::connect(read, &QAction::triggered, [cb = a.toggleRead]() { if (cb) cb(); });

    QAction* del = menu->addAction("Удалить");
    QObject::connect(del, &QAction::triggered, [cb = a.remove]() { if (cb) cb(); });
}

// функция для загрузки обложки
static QPixmap loadCoverPixmap(int bookId, const QSize& targetSize)
{
    // Кеш по bookId + размеру: при reloadBooks (лайк/прочитано/удаление) обложки
    // не декодируются повторно.
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

// ==== КЛИКАБЕЛЬНЫЙ LABEL ====
class ClickableLabel : public QLabel
{
public:
    std::function<void()> onClick;

protected:
    void mousePressEvent(QMouseEvent*) override
    {
        if (onClick) onClick();
    }
};

// карточка книги
QWidget* createBookCard(const DbBook& book, const HomeCardActions& actions)
{
    std::function<void()> onClick = actions.open;
    // Внешний контейнер: обложка + текст под ней
    QWidget* container = new QWidget;
    container->setFixedWidth(140);
    container->setCursor(Qt::PointingHandCursor);

    QVBoxLayout* vbox = new QVBoxLayout(container);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(6);

    // обложка с прогрессом
    QWidget* coverContainer = new QWidget;
    coverContainer->setFixedSize(140, 200);

    QVBoxLayout* coverContainerLayout = new QVBoxLayout(coverContainer);
    coverContainerLayout->setContentsMargins(0, 0, 0, 0);
    coverContainerLayout->setSpacing(0);

    // ===== ОБЛОЖКА (QLabel вместо QFrame) =====
    QLabel* coverLabel = new QLabel;
    coverLabel->setFixedSize(140, 200);
    coverLabel->setAlignment(Qt::AlignCenter);
    coverLabel->setStyleSheet(R"(
        QLabel {
            background:#1e1e1e;
            border-radius:8px;
        }
        QLabel:hover {
            background:#262626;
        }
    )");

    // Загружаем обложку
    QPixmap cover = loadCoverPixmap(book.id, QSize(140, 200));
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

    // ===== ПОЛОСА ПРОГРЕССА (поверх обложки) =====
    QFrame* progress = new QFrame;
    progress->setFixedHeight(4);
    progress->setStyleSheet("background:#444; border-radius:2px; margin-top:-4px;");

    if (book.totalPages > 0)
    {
        const double ratio = qBound(0.0, (double)book.lastPage / (double)book.totalPages, 1.0);
        QFrame* fill = new QFrame(progress);
        fill->setGeometry(0, 0, int(140 * ratio), 4);
        fill->setStyleSheet("background:#ff8c00; border-radius:2px;");
    }

    coverContainerLayout->addWidget(progress);

    // ===== КНОПКА-МЕНЮ (три точки) в правом верхнем углу обложки =====
    QToolButton* menuBtn = new QToolButton(coverContainer);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setFixedSize(28, 28);
    menuBtn->setIcon(QIcon("image icon/menu-dots.png"));
    menuBtn->setIconSize(QSize(16, 16));
    menuBtn->setPopupMode(QToolButton::InstantPopup);
    menuBtn->setStyleSheet(R"(
        QToolButton { background:rgba(20,20,20,0.65); border:none; border-radius:8px; }
        QToolButton:hover { background:rgba(42,42,42,0.95); }
        QToolButton::menu-indicator { image:none; width:0; }
    )");
    menuBtn->move(140 - 28 - 6, 6);
    menuBtn->raise();

    QMenu* cardMenu = new QMenu(menuBtn);
    cardMenu->setStyleSheet(kHomeCardMenuStyle);
    fillHomeCardMenu(cardMenu, actions);
    menuBtn->setMenu(cardMenu);

    vbox->addWidget(coverContainer);

    // ===== ТЕКСТ ПОД ОБЛОЖКОЙ =====
    QLabel* titleLabel = new QLabel(book.title);
    titleLabel->setStyleSheet("color:#fff; font-size:13px; font-weight:600;");
    titleLabel->setWordWrap(true);
    titleLabel->setFixedWidth(140);

    QLabel* authorLabel = new QLabel(book.author);
    authorLabel->setStyleSheet("color:#888; font-size:11px;");
    authorLabel->setWordWrap(true);
    authorLabel->setFixedWidth(140);

    vbox->addWidget(titleLabel);
    vbox->addWidget(authorLabel);

    // ===== КЛИК =====
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
    auto* filter = new ClickFilter;
    filter->click = std::move(onClick);
    container->installEventFilter(filter);

    return container;
}

// ==== СТРАНИЦА КНИГ ====
QWidget* HomePage::createBooksPage(const QVector<DbBook>& books, int skip)
{
    QWidget* page = new QWidget;

    QHBoxLayout* layout = new QHBoxLayout(page);
    layout->setSpacing(12);
    layout->setContentsMargins(4, 0, 0, 0); // небольшой отступ слева

    int shown = 0;
    for (int i = skip; i < books.size(); ++i)
    {
        if (shown++ >= 8) break;
        const auto& b = books[i];

        HomeCardActions a;
        a.favorite = b.favorite;
        a.read = b.totalPages > 0 && b.lastPage >= b.totalPages;
        const int id = b.id;
        const bool fav = b.favorite;
        const bool read = a.read;
        const QString title = b.title;
        a.open = [this, id]() { emit bookSelected(id); };
        a.toggleFav = [this, id, fav]() { doToggleFavorite(id, fav); };
        a.toggleRead = [this, id, read]() { doToggleRead(id, read); };
        a.remove = [this, id, title]() { doDelete(id, title); };

        layout->addWidget(createBookCard(b, a));
    }

    layout->addStretch();

    return page;
}

// ==== ОСНОВНОЙ КОНСТРУКТОР ====
HomePage::HomePage(QWidget* parent)
    : QWidget(parent)
{
    //FONTS
    int fontId = QFontDatabase::addApplicationFont("fonts/Manrope-VariableFont_wght.ttf");
    Q_UNUSED(fontId);

    setStyleSheet("background:#121212; color:white;");
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(20);

    // ===== БАННЕР =====
    QFrame* banner = new QFrame;
    banner->setFixedHeight(320);
    banner->setStyleSheet(R"(
        QFrame {
            border-radius:16px;
            background-image: url(image icon/book image.png);
            background-position: center;
        }
    )");

    QVBoxLayout* bannerLayout = new QVBoxLayout(banner);
    bannerLayout->setContentsMargins(30, 20, 30, 30);
    bannerLayout->setSpacing(10);
    bannerLayout->addStretch(3);

    // оранжевая плашка
    QLabel* tag = new QLabel("Недавно читали");
    tag->setStyleSheet(R"(
        background:#ff8c00;
        color:#442100;
        padding:4px 10px;
        border-radius:10px;
        font-size:12px;
    )");

    // название
    QLabel* title = new QLabel("Название");
    title->setStyleSheet("font-size:36px; font-family:Manrope;");
    bannerTitle = title;

    // автор
    QLabel* author = new QLabel("Автор ");
    author->setStyleSheet("font-size:16px; font-family:'Noto Serif'; color:#ccc;");
    bannerAuthor = author;

    // аннотация (краткое описание книги над кнопками) — на всю ширину блока
    QLabel* annotation = new QLabel;
    annotation->setWordWrap(true);
    annotation->setMaximumHeight(80); // ~3-4 строки
    annotation->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    annotation->setStyleSheet("font-size:15px; color:#cfcfcf; background:transparent;");
    bannerAnnotation = annotation;

    // кнопки
    QHBoxLayout* buttons = new QHBoxLayout;

    QPushButton* readBtn = new QPushButton("Продолжить читать");
    readBtn->setStyleSheet(R"(
        QPushButton {
            background:#ff8c00;
            border:none;
            padding:10px 20px;
            border-radius:8px;
        }
    )");

    QPushButton* detailsBtn = new QPushButton("Детали");
    detailsBtn->setStyleSheet(R"(
        QPushButton {
            background:#2a2a2a;
            border:none;
            padding:10px 20px;
            border-radius:8px;
        }
    )");

    buttons->addWidget(readBtn);
    buttons->addWidget(detailsBtn);
    buttons->addStretch();

    connect(readBtn, &QPushButton::clicked, this, [this]() {
        if (bannerBookId > 0)
            emit openReaderRequested(bannerBookId);
        });
    connect(detailsBtn, &QPushButton::clicked, this, [this]() {
        if (bannerBookId > 0)
            emit bookSelected(bannerBookId);
        });

    bannerLayout->addWidget(tag, 0, Qt::AlignLeft);
    bannerLayout->addWidget(title);
    bannerLayout->addWidget(author);
    bannerLayout->addWidget(annotation);
    bannerLayout->addSpacing(6);
    bannerLayout->addLayout(buttons);
    bannerLayout->addStretch(1);

    root->addWidget(banner);

    // ===== ТАБЫ =====
    QHBoxLayout* tabsLayout = new QHBoxLayout;

    tabRecentRead = new ClickableLabel;
    tabRecentAdded = new ClickableLabel;

    tabRecentRead->setText("Недавно читали");
    tabRecentAdded->setText("Недавно добавлено");

    tabRecentRead->setStyleSheet(R"(
        QLabel {
            background: transparent;
            border: none;
            font-size:16px;
            color:white;
        }
    )");

    tabRecentAdded->setStyleSheet("font-size:16px; color:#888;");

    tabsLayout->addWidget(tabRecentRead);
    tabsLayout->addSpacing(20);
    tabsLayout->addWidget(tabRecentAdded);
    tabsLayout->addStretch();

    root->addLayout(tabsLayout);

    // линия под табами
    QFrame* underline = new QFrame;
    underline->setFixedHeight(2);
    underline->setStyleSheet("background:#333;");
    root->addWidget(underline);

    // ===== STACK С КНИГАМИ =====
    booksStack = new QStackedWidget;
    booksStack->setContentsMargins(0, 10, 0, 0);
    booksStack->setStyleSheet("background:#110E0A;");
    booksStack->addWidget(new QWidget); // 0 recently read (filled in reloadBooks)
    booksStack->addWidget(new QWidget); // 1 recently added (filled in reloadBooks)
    booksStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    root->addWidget(booksStack);
    root->addStretch();

    // ===== ЛОГИКА ПЕРЕКЛЮЧЕНИЯ =====
    static_cast<ClickableLabel*>(tabRecentRead)->onClick = [=]()
        {
            booksStack->setCurrentIndex(0);
            tabRecentRead->setStyleSheet("font-size:16px; color:white; border-bottom:2px solid #ff8c00; padding-bottom:4px;");
            tabRecentAdded->setStyleSheet("font-size:16px; color:#888; border-bottom:none; padding-bottom:4px;");
        };

    static_cast<ClickableLabel*>(tabRecentAdded)->onClick = [=]()
        {
            booksStack->setCurrentIndex(1);
            tabRecentAdded->setStyleSheet("font-size:16px; color:white; border-bottom:2px solid #ff8c00; padding-bottom:4px;");
            tabRecentRead->setStyleSheet("font-size:16px; color:#888; border-bottom:none; padding-bottom:4px;");
        };

    // активный по умолчанию
    tabRecentRead->setStyleSheet("font-size:16px; color:white; border-bottom:2px solid #ff8c00; padding-bottom:4px;");

    reloadBooks();
}

void HomePage::reloadBooks()
{
    Database::instance().openOrCreate();

    const auto recentlyRead = Database::instance().listRecentlyRead(/*limit*/ 9, nullptr);
    const auto recentlyAdded = Database::instance().listRecentlyAdded(/*limit*/ 8, nullptr);

    // Баннер — самая последняя читаемая книга; если ни одной не читали, берём
    // самую недавно добавленную.
    const DbBook* bannerBook = nullptr;
    if (!recentlyRead.isEmpty())
        bannerBook = &recentlyRead.first();
    else if (!recentlyAdded.isEmpty())
        bannerBook = &recentlyAdded.first();

    if (bannerBook)
    {
        bannerBookId = bannerBook->id;
        if (bannerTitle) bannerTitle->setText(bannerBook->title);
        if (bannerAuthor) bannerAuthor->setText(bannerBook->author);
        if (bannerAnnotation)
        {
            QString ann = bannerBook->annotation.simplified();
            if (ann.size() > 420)
                ann = ann.left(420).trimmed() + "…";
            bannerAnnotation->setText(ann);
        }
    }
    else
    {
        bannerBookId = 0;
        if (bannerTitle) bannerTitle->setText("Нет книг");
        if (bannerAuthor) bannerAuthor->setText("");
        if (bannerAnnotation) bannerAnnotation->setText("");
    }

    // replace pages
    QWidget* old0 = booksStack->widget(0);
    QWidget* old1 = booksStack->widget(1);

    // В "Недавно читали" первая книга показана на баннере — пропускаем её.
    QWidget* p0 = createBooksPage(recentlyRead, 1);
    QWidget* p1 = createBooksPage(recentlyAdded, 0);

    booksStack->removeWidget(old0);
    booksStack->insertWidget(0, p0);
    old0->deleteLater();
 
    booksStack->removeWidget(old1);
    booksStack->insertWidget(1, p1);
    old1->deleteLater();
}

// ===== Действия над карточкой книги =====
void HomePage::doToggleFavorite(int bookId, bool currentlyFavorite)
{
    Database::instance().setFavorite(bookId, !currentlyFavorite, nullptr);
    reloadBooks();
    emit booksChanged();
}

void HomePage::doToggleRead(int bookId, bool currentlyRead)
{
    if (currentlyRead)
        Database::instance().updateLastPage(bookId, 0, nullptr); // сброс прогресса
    else
        Database::instance().markAsRead(bookId, nullptr);

    reloadBooks();
    emit booksChanged();
}

void HomePage::doDelete(int bookId, const QString& title)
{
    if (!confirmDelete(title))
        return;

    Database::instance().deleteBook(bookId, nullptr);
    reloadBooks();
    emit booksChanged();
}

bool HomePage::confirmDelete(const QString& title)
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