#include "librarypage.h"
#include "database.h"

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
#include <QApplication>
#include <QScrollArea>
#include <QMouseEvent>
#include <QEvent>
#include <QToolButton>
#include <QWidgetAction>
#include <functional>

static QPixmap loadCoverPixmap(int bookId, const QSize& targetSize)
{
    const QByteArray coverBytes = Database::instance().getBookCover(bookId, nullptr);
    QPixmap original;

    if (!coverBytes.isEmpty() && original.loadFromData(coverBytes))
    {
        // Масштабируем с сохранением пропорций и центрируем
        return original.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // Возвращаем пустую картинку, если обложки нет
    return QPixmap();///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}

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

// GRID CARD

static QWidget* createBookCard(
    const DbBook& book,
    std::function<void()> onClick)
{
    QWidget* container = new QWidget;
    container->setFixedSize(140, 300);
    container->setCursor(Qt::PointingHandCursor);

    QVBoxLayout* vbox = new QVBoxLayout(container);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(6);

    // ===== COVER (исправлено) =====

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

    // ===== PROGRESS BAR (поверх обложки) =====

    QWidget* coverContainer = new QWidget;
    coverContainer->setFixedSize(140, 200);

    QVBoxLayout* coverContainerLayout = new QVBoxLayout(coverContainer);
    coverContainerLayout->setContentsMargins(0, 0, 0, 0);
    coverContainerLayout->setSpacing(0);

    coverContainerLayout->addWidget(coverLabel);

    QFrame* progress = new QFrame;
    progress->setFixedHeight(4);
    progress->setStyleSheet("background:#444; border-radius:2px; margin-top:-4px;"); // накладываем поверх

    if (book.totalPages > 0)
    {
        const double ratio = qBound(0.0, (double)book.lastPage / (double)book.totalPages, 1.0);
        QFrame* fill = new QFrame(progress);
        fill->setGeometry(0, 0, int(140 * ratio), 4);
        fill->setStyleSheet("background:#ff8c00; border-radius:2px;");
    }

    coverContainerLayout->addWidget(progress);
    vbox->addWidget(coverContainer);

    QLabel* titleLabel = new QLabel(book.title);
    titleLabel->setStyleSheet(R"(
        color:#fff;
        font-size:13px;
        font-weight:600;
        background:transparent;
    )");
    titleLabel->setWordWrap(true);
    titleLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    titleLabel->setFixedWidth(140);

    QLabel* authorLabel = new QLabel(book.author);
    authorLabel->setStyleSheet(R"(color:#888; font-size:11px; background:transparent;)");
    authorLabel->setWordWrap(true);
    authorLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    authorLabel->setFixedWidth(140);

    vbox->addWidget(titleLabel);
    vbox->addWidget(authorLabel);
    vbox->addStretch();

    auto* filter = new ClickFilter;
    filter->click = std::move(onClick);
    container->installEventFilter(filter);

    return container;
}

// LIST ITEM
static QWidget* createListItem(const DbBook& book, std::function<void()> onClick)
{
    QWidget* item = new QWidget;
    item->setFixedHeight(230);
    item->setStyleSheet(R"(background:#1e1e1e; border-radius:12px;)");
    item->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    item->setCursor(Qt::PointingHandCursor);
    QHBoxLayout* layout = new QHBoxLayout(item);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(16);

    // ===== COVER (исправлено) =====

    QLabel* cover = new QLabel;  // QLabel вместо QFrame
    cover->setFixedSize(140, 200);
    cover->setAlignment(Qt::AlignCenter);
    cover->setStyleSheet(R"(
        QLabel {
            background:#2a2a2a;
            border-radius:8px;
        }
    )");

    // Загружаем обложку
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

    // ... остальной код (centerWidget, progress, кнопки) без изменений ...

    // CENTER
    QWidget* centerWidget = new QWidget;
    QVBoxLayout* centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(8);

    QLabel* title = new QLabel(book.title);
    title->setWordWrap(true);
    title->setStyleSheet(R"(font-weight:600; color:white; font-size:16px;)");

    QLabel* author = new QLabel(book.author);
    author->setStyleSheet(R"(color:#888; font-size:12px;)");

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
    progressText->setStyleSheet(R"( color:#888; font-size:12px;)");
    QString pages = QString("%1/%2").arg(book.lastPage).arg(book.totalPages);
    QLabel* pageLabel = new QLabel(pages);
    pageLabel->setStyleSheet(R"( color:#ff8c00; font-size:12px;)");
    progressTop->addWidget(progressText);
    progressTop->addStretch();
    progressTop->addWidget(pageLabel);

    QFrame* progressBg = new QFrame;
    progressBg->setFixedHeight(5);
    progressBg->setStyleSheet(R"(background:#333; border-radius:2px;)");

    if (book.totalPages > 0)
    {
        const double ratio = qBound(0.0, (double)book.lastPage / (double)book.totalPages, 1.0);
        QFrame* fill = new QFrame(progressBg);
        fill->setStyleSheet(R"(background:#ff8c00; border-radius:2px;)");
        fill->setGeometry(0, 0, int(progressBg->width() * ratio), 5);
        QObject::connect(progressBg, &QFrame::windowTitleChanged, [=]() 
        {
            fill->setGeometry(0, 0, int(progressBg->width() * ratio), 5);
         });
    }

    progressLayout->addLayout(progressTop);
    progressLayout->addWidget(progressBg);
    centerLayout->addWidget(progressWidget);

    // RIGHT MENU
    QWidget* rightWidget = new QWidget;
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(12);

    auto createMenuButton = [](const QString& iconPath, const QString& tooltip) {
        QToolButton* btn = new QToolButton;
        btn->setFixedSize(34, 34);
        btn->setIcon(QIcon(iconPath));
        btn->setIconSize(QSize(18, 18));
        btn->setToolTip(tooltip);
        btn->setStyleSheet(R"(
        QToolButton {
            background:#2a2a2a;
            border:none;
            border-radius:8px;
        }
        QToolButton:hover {
            background:#363636;
        }
    )");
        return btn;
        };

    QToolButton* favBtn = createMenuButton("image icon/heart.png", "Добавить в избранное");
    QToolButton* doneBtn = createMenuButton("image icon/check.png", "Отметить прочитанным");
    QToolButton* deleteBtn = createMenuButton("image icon/trash.png", "Удалить");

    rightLayout->addWidget(favBtn);
    rightLayout->addWidget(doneBtn);
    rightLayout->addWidget(deleteBtn);
    rightLayout->addStretch();

    layout->addWidget(cover);
    layout->addWidget(centerWidget, 1);
    layout->addWidget(rightWidget);

    auto* f = new ClickFilter;
    f->click = std::move(onClick);
    item->installEventFilter(f);

    return item;
}

// GRID VIEW

QWidget* LibraryPage::createGridView()
{
    QWidget* wrapper = new QWidget;
    wrapper->setStyleSheet("background:transparent;");

    QVBoxLayout* outer = new QVBoxLayout(wrapper);
    outer->setContentsMargins(0, 0, 0, 0);

    QWidget* page = new QWidget;
    page->setStyleSheet("background:transparent;");

    QGridLayout* layout = new QGridLayout(page);
    layout->setSpacing(16);
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    int cols = 6;

    Database::instance().openOrCreate();

    const auto books = Database::instance().listBooks(nullptr);

    int i = 0;

    for (const auto& b : books)
    {
        auto* card = createBookCard(b, [this, id = b.id]()
            {
                emit bookSelected(id);
            }
        );

        layout->addWidget(card, i / cols, i % cols, Qt::AlignTop | Qt::AlignLeft);
        i++;
        if (i >= 30)
            break;
    }

    outer->addWidget(page);
    outer->addStretch();

    return wrapper;
}

// LIST VIEW

QWidget* LibraryPage::createListView()
{
    QWidget* wrapper = new QWidget;

    wrapper->setStyleSheet("background:transparent;");

    QVBoxLayout* outer = new QVBoxLayout(wrapper);

    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    QWidget* page = new QWidget;

    page->setStyleSheet( "background:transparent;");

    QGridLayout* layout = new QGridLayout(page);

    layout->setHorizontalSpacing(20);
    layout->setVerticalSpacing(15);

    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    int cols = 2;

    Database::instance().openOrCreate();

    const auto books = Database::instance().listBooks(nullptr);

    int i = 0;

    for (const auto& b : books)
    {
        layout->addWidget(createListItem( b, [this, id = b.id]()
            {
                emit bookSelected(id);
            }
        ),
            i / cols,
            i % cols
        );

        i++;

        if (i >= 20)
            break;
    }

    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);

    outer->addWidget(page);
    outer->addStretch();

    return wrapper;
}

// CONSTRUCTOR

LibraryPage::LibraryPage(QWidget* parent) : QWidget(parent)
{
    setStyleSheet( "background:#110E0A; color:white;");

    QVBoxLayout* root = new QVBoxLayout(this);

    // выравнивание header по сетке книг
    root->setContentsMargins(16, 20, 16, 20);

    // HEADER

    QHBoxLayout* header = new QHBoxLayout;
    QVBoxLayout* titles = new QVBoxLayout;

    QLabel* title = new QLabel("Библиотека");
    QLabel* subtitle = new QLabel("10 книг в вашей библиотеке");

    title->setStyleSheet("font-size:28px;");

    subtitle->setStyleSheet("color:#aaa;");

    titles->addWidget(title);
    titles->setContentsMargins(5, 0, 0, 0);
    titles->addWidget(subtitle);

    header->addLayout(titles);
    header->setContentsMargins(5, 0, 0, 0);
    header->addStretch();

    // BUTTON STYLES

    QString activeStyle = R"(
        QPushButton {
            background:#3a3a3a;
            border-radius:8px;
        }
    )";

    QString inactiveStyle = R"(
        QPushButton {
            background:#1e1e1e;
            border-radius:8px;
        }
    )";

    // SEARCH

    QLineEdit* search = new QLineEdit;

    search->setFixedHeight(50);
    search->setMaximumWidth(600);

    search->setPlaceholderText("Поиск по библиотеке");

    search->setStyleSheet(R"(
        QLineEdit {
            background:#000000;
            border-radius:10px;
            padding-left:10px;
            color:white;
            font-size: 14px;
        }
    )");

    QAction* searchIcon = new QAction(QIcon("image icon/search.png"), "", search);
    search->addAction(searchIcon,QLineEdit::LeadingPosition);

    // SORT

    QPushButton* sortBtn =new QPushButton;

    sortBtn->setFixedSize(40, 40);
    sortBtn->setIcon(QIcon("image icon/sort.png"));
    sortBtn->setIconSize(QSize(20, 20));
    sortBtn->setStyleSheet(inactiveStyle);
    sortBtn->setStyleSheet(inactiveStyle + R"(
        QPushButton::menu-indicator { image: none; width: 0; }
    )");

    QMenu* sortMenu = new QMenu(this);

    sortMenu->addAction("По дате");
    sortMenu->addAction("По автору");
    sortMenu->addAction("По названию");
    sortMenu->addAction("По прогрессу");

    sortBtn->setMenu(sortMenu);

    // VIEW BUTTONS

    QPushButton* gridBtn = new QPushButton;
    QPushButton* listBtn = new QPushButton;

    gridBtn->setFixedSize(40, 40);
    listBtn->setFixedSize(40, 40);

    gridBtn->setIcon(QIcon("image icon/grid.png"));
    listBtn->setIcon(QIcon("image icon/list.png"));

    gridBtn->setIconSize(QSize(20, 20));
    listBtn->setIconSize(QSize(20, 20));
    gridBtn->setStyleSheet( activeStyle);
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

    QScrollArea* scroll =new QScrollArea;

    scroll->setWidget(scrollContent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet("background:transparent;");

    root->addWidget(scroll);

    // SCROLLBAR STYLE

    qApp->setStyleSheet(R"(
        QScrollBar:vertical {
            background: transparent;
            width: 5px;
            border: none;
        }

        QScrollBar::handle:vertical {
            background: rgba(160, 82, 45, 200);
            border-radius: 5px;
            min-height: 30px;
        }

        QScrollBar::handle:vertical:hover {
            background: rgba(255, 160, 40, 220);
        }

        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px;
            background: none;
        }

        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
        }

        //стиль кнопок боковы в лист виде
        QToolTip {
            background-color: #2a2a2a;
            color: white;
            border: 1px solid #444;
            padding: 6px 10px;
            border-radius: 6px;
            font-size: 12px;
        }
    )");

    // SWITCH LOGIC

    connect(gridBtn, &QPushButton::clicked, this, [=]()
    {
        viewStack->setCurrentIndex(0);
        gridBtn->setStyleSheet(activeStyle);
        listBtn->setStyleSheet(inactiveStyle);
    }
    );

    connect(listBtn, &QPushButton::clicked, this, [=]()
    {
        viewStack->setCurrentIndex(1);
        listBtn->setStyleSheet(activeStyle);
        gridBtn->setStyleSheet(inactiveStyle);
    }
    );
    reloadBooks();
}

// RELOAD

void LibraryPage::reloadBooks()
{
    while (viewStack->count() > 0)
    {
        QWidget* w = viewStack->widget(0);
        viewStack->removeWidget(w);
        w->deleteLater();
    }
    viewStack->addWidget(createGridView());
    viewStack->addWidget(createListView());
    viewStack->setCurrentIndex(0);
}
