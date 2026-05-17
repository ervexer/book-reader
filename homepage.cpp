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
#include <QByteArray>

#include <functional>

// ==== ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ ДЛЯ ЗАГРУЗКИ ОБЛОЖКИ ====
static QPixmap loadCoverPixmap(int bookId, const QSize& targetSize)
{
    const QByteArray coverBytes = Database::instance().getBookCover(bookId, nullptr);
    QPixmap original;

    if (!coverBytes.isEmpty() && original.loadFromData(coverBytes))
    {
        return original.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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

// ==== КАРТОЧКА КНИГИ (С ОБЛОЖКОЙ) ====
QWidget* createBookCard(const DbBook& book, std::function<void()> onClick)
{
    // Внешний контейнер: обложка + текст под ней
    QWidget* container = new QWidget;
    container->setFixedWidth(140);
    container->setCursor(Qt::PointingHandCursor);

    QVBoxLayout* vbox = new QVBoxLayout(container);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(6);

    // ===== КОНТЕЙНЕР ДЛЯ ОБЛОЖКИ С ПРОГРЕССОМ =====
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
QWidget* HomePage::createBooksPage(const QVector<DbBook>& books)
{
    QWidget* page = new QWidget;

    QHBoxLayout* layout = new QHBoxLayout(page);
    layout->setSpacing(12);
    layout->setContentsMargins(4, 0, 0, 0); // небольшой отступ слева

    int shown = 0;
    for (const auto& b : books)
    {
        if (shown++ >= 8) break;
        layout->addWidget(createBookCard(b, [this, id = b.id]() {
            emit bookSelected(id);
            }));
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
    bannerLayout->addStretch();

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

    // автор
    QLabel* author = new QLabel("Автор ");
    author->setStyleSheet("font-size:16px; font-family:'Noto Serif'; color:#ccc;");

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

    bannerLayout->addWidget(tag, 0, Qt::AlignLeft);
    bannerLayout->addWidget(title);
    bannerLayout->addWidget(author);
    bannerLayout->addLayout(buttons);

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

    const auto recentlyRead = Database::instance().listRecentlyRead(/*limit*/ 8, nullptr);
    const auto recentlyAdded = Database::instance().listRecentlyAdded(/*limit*/ 8, nullptr);

    // replace pages
    QWidget* old0 = booksStack->widget(0);
    QWidget* old1 = booksStack->widget(1);

    QWidget* p0 = createBooksPage(recentlyRead);
    QWidget* p1 = createBooksPage(recentlyAdded);

    booksStack->removeWidget(old0);
    booksStack->insertWidget(0, p0);
    old0->deleteLater();
 
    booksStack->removeWidget(old1);
    booksStack->insertWidget(1, p1);
    old1->deleteLater();
}