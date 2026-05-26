#include "bookinfopage.h"
#include "database.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QScrollArea>
#include <QFrame>
#include <QIcon>
#include <QMouseEvent>
#include <QPixmap>
#include <QSize>
#include <QEvent>
#include <QProgressBar>
#include <functional>

class ClickableRow : public QFrame
{
public:
    std::function<void()> onClick;

protected:
    void mousePressEvent(QMouseEvent*) override
    {
        if (onClick)
            onClick();
    }
};

static QLabel* makeTabLabel(const QString& text)
{
    auto* l = new QLabel(text);
    l->setCursor(Qt::PointingHandCursor);
    l->setStyleSheet("font-size:16px; color:#888;");
    return l;
}

BookInfoPage::BookInfoPage(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet("background:#110E0A; color:white;");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* pageScroll = new QScrollArea;
    pageScroll->setWidgetResizable(true);
    pageScroll->setFrameShape(QFrame::NoFrame);
    pageScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pageScroll->setStyleSheet("background:transparent;");
    pageScroll->setStyleSheet(R"(
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: #1a1a1a;
            width: 8px;
            border-radius: 4px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #4a4a4a;
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: #5a5a5a;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }
    )");

    auto* page = new QWidget;
    page->setStyleSheet("background:#110E0A;");

    auto* contentRoot = new QVBoxLayout(page);
    contentRoot->setContentsMargins(22, 18, 22, 18);
    contentRoot->setSpacing(16);

    // ===== TOP =====

    auto* top = new QHBoxLayout;
    top->setSpacing(12);

    backBtn = new QPushButton;
    backBtn->setFixedSize(36, 36);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setIconSize(QSize(18, 18));
    backBtn->setIcon(QIcon("image icon/back.png"));

    connect(backBtn, &QPushButton::clicked,
        this, &BookInfoPage::backRequested);

    tabContents = makeTabLabel("содержание");
    tabBookmarks = makeTabLabel("закладки");
    tabNotes = makeTabLabel("заметки");

    top->addWidget(backBtn);
    top->addSpacing(8);
    top->addWidget(tabContents);
    top->addSpacing(18);
    top->addWidget(tabBookmarks);
    top->addSpacing(18);
    top->addWidget(tabNotes);
    top->addStretch();

    contentRoot->addLayout(top);

    tabUnderline = new QFrame;
    tabUnderline->setFixedHeight(2);
    tabUnderline->setStyleSheet("background:#2a2a2a;");
    contentRoot->addWidget(tabUnderline);

    // ===== HEADER =====

    auto* header = new QHBoxLayout;
    header->setSpacing(18);
    header->setAlignment(Qt::AlignTop);

    coverLbl = new QLabel;
    coverLbl->setFixedSize(160, 240);
    coverLbl->setAlignment(Qt::AlignCenter);
    coverLbl->setStyleSheet(
        "background:#2a2a2a;"
        "border-radius:10px;"
    );

    auto* infoCol = new QVBoxLayout;
    infoCol->setSpacing(8);

    titleLbl = new QLabel("Название");
    titleLbl->setWordWrap(true);
    titleLbl->setStyleSheet("font-size:34px;");

    authorLbl = new QLabel("Автор");
    authorLbl->setStyleSheet(
        "color:#aaa;"
        "font-size:16px;"
    );

    statsLbl = new QLabel;
    statsLbl->setStyleSheet(
        "color:#8c847a;"
        "font-size:12px;"
    );

    infoCol->addWidget(titleLbl);
    infoCol->addWidget(authorLbl);
    infoCol->addWidget(statsLbl);

    infoCol->addStretch();

    // ===== BUTTONS =====

    continueBtn = new QPushButton("Начать читать");
    continueBtn->setCursor(Qt::PointingHandCursor);
    continueBtn->setFixedHeight(42);

    continueBtn->setStyleSheet(
        "QPushButton{"
        "background:#ff8c00;"
        "color:black;"
        "border:none;"
        "border-radius:10px;"
        "padding:10px 18px;"
        "}"
        "QPushButton:hover{"
        "background:#ffa733;"
        "}"
    );

    finishBtn = new QPushButton("Завершить чтение");
    finishBtn->setCursor(Qt::PointingHandCursor);
    finishBtn->setFixedHeight(42);

    finishBtn->setStyleSheet(R"(
        QPushButton {
            background:#2a2a2a;
            border:none;
            padding:10px 20px;
            border-radius:8px;
            color:white;
        }

        QPushButton:hover {
            background:#353535;
        }
    )");

    auto* buttonsRow = new QHBoxLayout;
    buttonsRow->setSpacing(10);

    buttonsRow->addWidget(continueBtn);
    buttonsRow->addWidget(finishBtn);
    buttonsRow->addStretch();

    // ===== PROGRESS =====

    auto* progressHeaderLayout = new QHBoxLayout;
    progressHeaderLayout->setSpacing(12);

    progressLabel = new QLabel;
    progressLabel->setStyleSheet(
        "font-size:13px;"
        "color:#aaa;"
    );

    pagesReadLabel = new QLabel;
    pagesReadLabel->setStyleSheet(
        "font-size:13px;"
        "color:#ff8c00;"
        "font-weight:bold;"
    );
    pagesReadLabel->setAlignment(Qt::AlignRight);

    progressHeaderLayout->addWidget(progressLabel);
    progressHeaderLayout->addStretch();
    progressHeaderLayout->addWidget(pagesReadLabel);

    progressBar = new QProgressBar;
    progressBar->setFixedHeight(6);
    progressBar->setTextVisible(false);

    progressBar->setStyleSheet(R"(
        QProgressBar {
            background:#2a2a2a;
            border:none;
            border-radius:3px;
        }

        QProgressBar::chunk {
            background:#ff8c00;
            border-radius:3px;
        }
    )");

    infoCol->addSpacing(12);
    infoCol->addLayout(buttonsRow);
    infoCol->addSpacing(18);
    infoCol->addLayout(progressHeaderLayout);
    infoCol->addWidget(progressBar);

    header->addWidget(coverLbl);
    header->addLayout(infoCol);
    header->addStretch();

    contentRoot->addLayout(header);

    // ===== TABS =====

    tabStack = new QStackedWidget;
    tabStack->setStyleSheet("background:transparent;");

    contentsPage = new QWidget;

    auto* contentsRoot = new QVBoxLayout(contentsPage);
    contentsRoot->setContentsMargins(0, 0, 0, 0);
    contentsRoot->setSpacing(10);

    auto* contentsTitle = new QLabel("Содержание");
    contentsTitle->setStyleSheet(
        "font-size:24px;"
        "color:#d2c7bb;"
    );

    contentsRoot->addWidget(contentsTitle);

    auto* listWidget = new QWidget;

    contentsListLayout = new QVBoxLayout(listWidget);
    contentsListLayout->setSpacing(10);

    contentsRoot->addWidget(listWidget);

    placeholdersBookmarks = new QLabel("Закладки (будет тут)");
    placeholdersNotes = new QLabel("Заметки (будет тут)");

    tabStack->addWidget(contentsPage);
    tabStack->addWidget(placeholdersBookmarks);
    tabStack->addWidget(placeholdersNotes);

    contentRoot->addWidget(tabStack);

    pageScroll->setWidget(page);

    root->addWidget(pageScroll);

    // ===== CONNECTS =====

    connect(continueBtn, &QPushButton::clicked,
        this, &BookInfoPage::onContinueButtonClicked);

    connect(finishBtn, &QPushButton::clicked,
        this, &BookInfoPage::onFinishButtonClicked);

    // ===== TABS =====

    auto setTabClick = [&](QLabel* tab, int idx) {
        tab->installEventFilter(this);
        tab->setProperty("tabIndex", idx);
        };

    setTabClick(tabContents, 0);
    setTabClick(tabBookmarks, 1);
    setTabClick(tabNotes, 2);

    setActiveTab(0);
}

void BookInfoPage::setBookId(int bookId)
{
    if (currentBookId == bookId && chaptersLoaded)
        return;

    currentBookId = bookId;
    chaptersLoaded = false;

    fullLoad();
}

void BookInfoPage::fullLoad()
{
    loadBookInfo();
    loadChapters();
    updateProgress();
}

void BookInfoPage::loadBookInfo()
{
    if (!currentBookId)
        return;

    DbBook book;
    QString err;

    Database::instance().openOrCreate();

    if (!Database::instance().getBook(currentBookId, &book, &err))
        return;

    totalPages = book.totalPages;
    currentPage = book.lastPage;

    titleLbl->setText(book.title);
    authorLbl->setText(book.author);

    // Загрузка обложки
    const QByteArray coverBytes =
        Database::instance().getBookCover(currentBookId, nullptr);

    QPixmap coverPixmap;

    if (!coverBytes.isEmpty() &&
        coverPixmap.loadFromData(coverBytes))
    {
        coverLbl->setPixmap(
            coverPixmap.scaled(
                coverLbl->size(),
                Qt::KeepAspectRatioByExpanding,
                Qt::SmoothTransformation
            )
        );
    }
    else
    {
        coverLbl->setText("Нет обложки");
    }

    // Статистика
    const auto chapters = Database::instance().listChapters(currentBookId, nullptr);
    statsLbl->setText(
        QString("%1 глав • %2 страниц")
        .arg(chapters.size())
        .arg(book.totalPages)
    );
}

void BookInfoPage::loadChapters()
{
    if (chaptersLoaded || !currentBookId)
        return;

    // Очищаем старые главы
    while (QLayoutItem* it = contentsListLayout->takeAt(0))
    {
        if (auto* w = it->widget())
            w->deleteLater();
        delete it;
    }

    const auto chapters =
        Database::instance().listChapters(currentBookId, nullptr);

    int displayIndex = 1;

    for (const auto& ch : chapters)
    {
        auto* row = new ClickableRow;

        row->setFixedHeight(62);

        row->setStyleSheet(
            "QFrame{"
            "background:#1d1914;"
            "border-radius:12px;"
            "}"
            "QFrame:hover{"
            "background:#262019;"
            "}"
        );

        auto* h = new QHBoxLayout(row);

        h->setContentsMargins(16, 0, 16, 0);

        auto* idx = new QLabel(
            QString("%1")
            .arg(displayIndex++, 2, 10, QChar('0'))
        );

        idx->setStyleSheet(
            "color:#ff8c00;"
            "font-size:18px;"
            "min-width:40px;"
        );

        auto* name = new QLabel(ch.title);

        name->setStyleSheet(
            "font-size:18px;"
            "color:#d2c7bb;"
        );

        auto* play = new QLabel;
        play->setFixedSize(22, 22);

        play->setPixmap(
            QIcon("image icon/play.png").pixmap(22, 22)
        );

        h->addWidget(idx);
        h->addWidget(name);
        h->addStretch();
        h->addWidget(play);

        const int chapterIdx = ch.index;

        row->onClick = [this, chapterIdx]() {
            emit openReaderRequested(currentBookId, chapterIdx);
            };

        contentsListLayout->addWidget(row);
    }

    contentsListLayout->addStretch();
    chaptersLoaded = true;
}

void BookInfoPage::updateProgress()
{
    if (!currentBookId)
        return;

    DbBook book;
    QString err;

    if (!Database::instance().getBook(currentBookId, &book, &err))
        return;

    totalPages = book.totalPages;
    currentPage = book.lastPage;

    bool isFinished = totalPages > 0 && currentPage >= (totalPages - 1);

    int progress = 0;
    int pagesRead = 0;

    if (totalPages > 0) {
        if (isFinished) {
            progress = 100;
            pagesRead = totalPages;
        }
        else {
            progress = (currentPage * 100) / totalPages;
            pagesRead = currentPage;
        }
    }

    // Обновляем прогресс бар
    progressBar->setValue(progress);

    // Обновляем тексты
    progressLabel->setText(QString("Прогресс чтения: %1%").arg(progress));
    pagesReadLabel->setText(QString("%1 / %2 стр.").arg(pagesRead).arg(totalPages));

    updateButtonsState();
}

void BookInfoPage::updateButtonsState()
{
    if (!currentBookId)
        return;

    bool isFinished = totalPages > 0 && currentPage >= (totalPages - 1);

    if (isFinished) {
        // Книга прочитана
        finishBtn->setText("Прочитано");
        finishBtn->setStyleSheet(R"(
            QPushButton {
                background:#1f4d2f;
                border:none;
                padding:10px 20px;
                border-radius:8px;
                color:white;
            }
            QPushButton:hover {
                background:#2a6b3f;
            }
        )");
        continueBtn->setText("Начать читать");
    }
    else if (currentPage > 0) {
        // Книга начата, но не завершена
        continueBtn->setText("Продолжить читать");
        finishBtn->setText("Завершить чтение");
        finishBtn->setStyleSheet(R"(
            QPushButton {
                background:#2a2a2a;
                border:none;
                padding:10px 20px;
                border-radius:8px;
                color:white;
            }
            QPushButton:hover {
                background:#353535;
            }
        )");
    }
    else {
        // Книга не начата
        continueBtn->setText("Начать читать");
        finishBtn->setText("Завершить чтение");
        finishBtn->setStyleSheet(R"(
            QPushButton {
                background:#2a2a2a;
                border:none;
                padding:10px 20px;
                border-radius:8px;
                color:white;
            }
            QPushButton:hover {
                background:#353535;
            }
        )");
    }
}

void BookInfoPage::onContinueButtonClicked()
{
    emit openReaderRequested(currentBookId, -1);
}

void BookInfoPage::onFinishButtonClicked()
{
    if (!currentBookId)
        return;

    DbBook book;

    if (!Database::instance().getBook(currentBookId, &book, nullptr))
        return;

    bool isFinished = book.totalPages > 0 &&
        book.lastPage >= (book.totalPages - 1);

    if (isFinished) {
        // Сбрасываем прогресс в 0
        Database::instance().updateLastPage(currentBookId, 0, nullptr);
        currentPage = 0;
    }
    else {
        // Завершаем чтение (прогресс 100%)
        Database::instance().updateLastPage(
            currentBookId,
            qMax(0, book.totalPages - 1),
            nullptr
        );
        currentPage = book.totalPages;
    }

    // Быстрое обновление UI без перезагрузки глав
    updateProgress();
}

void BookInfoPage::setActiveTab(int tabIndex)
{
    tabStack->setCurrentIndex(tabIndex);

    auto styleActive =
        "font-size:16px;"
        "color:#ff8c00;"
        "padding-bottom:6px;";

    auto styleInactive =
        "font-size:16px;"
        "color:#888;"
        "padding-bottom:6px;";

    tabContents->setStyleSheet(
        tabIndex == 0 ? styleActive : styleInactive
    );

    tabBookmarks->setStyleSheet(
        tabIndex == 1 ? styleActive : styleInactive
    );

    tabNotes->setStyleSheet(
        tabIndex == 2 ? styleActive : styleInactive
    );
}

bool BookInfoPage::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        if (watched == tabContents ||
            watched == tabBookmarks ||
            watched == tabNotes)
        {
            const int idx = watched->property("tabIndex").toInt();

            setActiveTab(idx);

            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}