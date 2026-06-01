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
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(16);

    // Тонкий скроллбар для внутренних областей (страница больше не прокручивается)
    const QString scrollStyle = R"(
        QScrollArea { background: transparent; border: none; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }
        QScrollBar::handle:vertical { background: #4a4038; border-radius: 4px; min-height: 24px; }
        QScrollBar::handle:vertical:hover { background: #5e5249; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
    )";

    // ===== HEADER (информация о книге) =====

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

    // Растягиваем колонку с информацией на всю ширину левого столбца,
    // чтобы правый край блока совпадал с блоком «Содержание».
    header->addWidget(coverLbl);
    header->addLayout(infoCol, 1);

    // ===== ОСНОВНАЯ ОБЛАСТЬ: две колонки =====

    auto* mainRow = new QHBoxLayout;
    mainRow->setSpacing(22);

    // --- ЛЕВАЯ КОЛОНКА: информация о книге + содержание ---
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(14);

    leftCol->addLayout(header);

    auto* contentsTitle = new QLabel("Содержание");
    contentsTitle->setStyleSheet(
        "font-size:24px;"
        "color:#d2c7bb;"
    );
    leftCol->addWidget(contentsTitle);

    auto* leftScroll = new QScrollArea;
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    leftScroll->setStyleSheet(scrollStyle);

    auto* listWidget = new QWidget;
    listWidget->setStyleSheet("background:transparent;");

    contentsListLayout = new QVBoxLayout(listWidget);
    contentsListLayout->setContentsMargins(0, 0, 8, 0);
    contentsListLayout->setSpacing(10);

    leftScroll->setWidget(listWidget);
    leftCol->addWidget(leftScroll, 1);

    // --- ПРАВАЯ КОЛОНКА: закладки / цитаты ---
    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(14);

    // Кнопки переключения над блоком
    auto* rightTabs = new QHBoxLayout;
    rightTabs->setSpacing(18);

    tabBookmarks = makeTabLabel("закладки");
    tabNotes = makeTabLabel("цитаты");

    rightTabs->addWidget(tabBookmarks);
    rightTabs->addWidget(tabNotes);
    rightTabs->addStretch();

    rightCol->addLayout(rightTabs);

    // Блок чуть светлее основного фона (как подсветка главы)
    auto* rightBlock = new QFrame;
    rightBlock->setStyleSheet("QFrame{background:#1d1914; border-radius:14px;}");

    auto* rightBlockLayout = new QVBoxLayout(rightBlock);
    rightBlockLayout->setContentsMargins(8, 8, 8, 8);

    auto* rightScroll = new QScrollArea;
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightScroll->setStyleSheet(scrollStyle);

    tabStack = new QStackedWidget;
    tabStack->setStyleSheet("background:transparent;");

    // Вкладка "закладки" — контейнер со своим вертикальным layout'ом
    auto* bookmarksPage = new QWidget;
    bookmarksPage->setStyleSheet("background:transparent;");
    bookmarksLayout = new QVBoxLayout(bookmarksPage);
    bookmarksLayout->setContentsMargins(4, 4, 4, 4);
    bookmarksLayout->setSpacing(8);
    bookmarksLayout->addStretch();

    // Вкладка "цитаты"
    auto* notesPage = new QWidget;
    notesPage->setStyleSheet("background:transparent;");
    quotesLayout = new QVBoxLayout(notesPage);
    quotesLayout->setContentsMargins(4, 4, 4, 4);
    quotesLayout->setSpacing(8);
    quotesLayout->addStretch();

    placeholdersBookmarks = bookmarksPage;
    placeholdersNotes = notesPage;

    tabStack->addWidget(bookmarksPage);
    tabStack->addWidget(notesPage);

    rightScroll->setWidget(tabStack);
    rightBlockLayout->addWidget(rightScroll);

    rightCol->addWidget(rightBlock, 1);

    mainRow->addLayout(leftCol, 3);
    mainRow->addLayout(rightCol, 2);

    root->addLayout(mainRow, 1);

    // ===== CONNECTS =====

    connect(continueBtn, &QPushButton::clicked,
        this, &BookInfoPage::onContinueButtonClicked);

    connect(finishBtn, &QPushButton::clicked,
        this, &BookInfoPage::onFinishButtonClicked);

    // ===== TABS (только закладки / цитаты) =====

    auto setTabClick = [&](QLabel* tab, int idx) {
        tab->installEventFilter(this);
        tab->setProperty("tabIndex", idx);
        };

    setTabClick(tabBookmarks, 0);
    setTabClick(tabNotes, 1);

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

    // Статистика (число глав) проставляется в loadChapters(), где список глав
    // уже запрашивается — чтобы не дёргать listChapters дважды.
}

void BookInfoPage::loadChapters()
{
    if (chaptersLoaded || !currentBookId)
        return;

    // Очищаем старые главы
    chapterRows.clear();
    while (QLayoutItem* it = contentsListLayout->takeAt(0))
    {
        if (auto* w = it->widget())
            w->deleteLater();
        delete it;
    }

    const auto chapters =
        Database::instance().listChapters(currentBookId, nullptr);

    chapterCount = chapters.size();

    statsLbl->setText(
        QString("%1 глав • %2 страниц")
        .arg(chapterCount)
        .arg(totalPages)
    );

    int displayIndex = 1;

    for (const auto& ch : chapters)
    {
        // Вся строка главы — одна сплошная кнопка
        auto* row = new ClickableRow;

        row->setFixedHeight(62);
        row->setCursor(Qt::PointingHandCursor);

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
            "background:transparent;"
        );

        auto* name = new QLabel(ch.title);
        name->setObjectName("chapterName");

        name->setStyleSheet(
            "font-size:18px;"
            "color:#d2c7bb;"
            "background:transparent;"
        );

        // Клики по содержимому не должны перехватываться дочерними виджетами —
        // вся карточка целиком работает как одна кнопка.
        idx->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        name->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        h->addWidget(idx);
        h->addWidget(name);
        h->addStretch();

        const int chapterIdx = ch.index;

        row->onClick = [this, chapterIdx]() {
            emit openReaderRequested(currentBookId, chapterIdx);
            };

        contentsListLayout->addWidget(row);
        chapterRows.push_back(row);
    }

    contentsListLayout->addStretch();
    chaptersLoaded = true;

    applyChapterHighlight();
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

    // Обновляем число страниц в статистике (после закрытия ридера totalPages
    // уже посчитан — счётчик появляется сразу, без перезапуска приложения).
    if (chapterCount > 0)
        statsLbl->setText(
            QString("%1 глав • %2 страниц").arg(chapterCount).arg(totalPages));

    applyChapterHighlight();
    refreshSidePanels();

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

void BookInfoPage::applyChapterHighlight()
{
    if (chapterRows.isEmpty())
        return;

    // Подсветка как в ридере (buildContentsContent): заливка без обводки,
    // активная глава — оранжевый жирный заголовок.
    const QString base =
        "QFrame{background:#1d1914;border-radius:12px;}"
        "QFrame:hover{background:#262019;}";
    const QString activeStyle =
        "QFrame{background:#3a2e1c;border-radius:12px;}";

    const QString nameBase =
        "font-size:18px;color:#d2c7bb;background:transparent;";
    const QString nameActive =
        "font-size:18px;color:#ff8c00;font-weight:bold;background:transparent;";

    // Подсвечиваем только если чтение действительно начато.
    int active = -1;
    if (currentPage > 0)
    {
        DbBook book;
        if (Database::instance().getBook(currentBookId, &book, nullptr))
            active = book.lastChapter;
    }

    for (int i = 0; i < chapterRows.size(); ++i)
    {
        const bool isActive = (i == active);
        chapterRows[i]->setStyleSheet(isActive ? activeStyle : base);
        if (auto* name = chapterRows[i]->findChild<QLabel*>("chapterName"))
            name->setStyleSheet(isActive ? nameActive : nameBase);
    }
}

void BookInfoPage::refreshSidePanels()
{
    if (!bookmarksLayout || !quotesLayout || !currentBookId)
        return;

    auto clearLayout = [](QVBoxLayout* lay) {
        while (QLayoutItem* it = lay->takeAt(0))
        {
            if (auto* w = it->widget())
            {
                w->hide();
                w->deleteLater();
            }
            delete it;
        }
    };

    clearLayout(bookmarksLayout);
    clearLayout(quotesLayout);

    const QString delStyle =
        "QPushButton{background:transparent;border:none;color:#8c847a;font-size:14px;}"
        "QPushButton:hover{color:#ff8c00;}";

    // ---- Закладки ----
    const auto bookmarks = Database::instance().listBookmarks(currentBookId, nullptr);
    if (bookmarks.isEmpty())
    {
        auto* e = new QLabel("Здесь будут ваши закладки");
        e->setAlignment(Qt::AlignCenter);
        e->setStyleSheet("color:#8c847a; font-size:14px; background:transparent;");
        bookmarksLayout->addWidget(e);
    }
    else
    {
        for (const auto& bm : bookmarks)
        {
            auto* row = new ClickableRow;
            row->setCursor(Qt::PointingHandCursor);
            row->setStyleSheet(
                "QFrame{background:#241f18;border-radius:8px;}"
                "QFrame:hover{background:#2e2820;}");
            const int page = bm.page;
            row->onClick = [this, page]() {
                emit openReaderAtPageRequested(currentBookId, page);
                };

            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(12, 8, 12, 8);
            h->setSpacing(8);

            auto* col = new QVBoxLayout;
            col->setContentsMargins(0, 0, 0, 0);
            col->setSpacing(3);

            auto* t = new QLabel(QString("Стр. %1").arg(bm.page + 1));
            t->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            t->setStyleSheet("color:#ff8c00; font-size:13px; font-weight:bold; background:transparent;");
            auto* c = new QLabel(bm.chapterTitle.isEmpty() ? "—" : bm.chapterTitle);
            c->setWordWrap(true);
            c->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            c->setStyleSheet("color:#cfc7bd; font-size:12px; background:transparent;");
            col->addWidget(t);
            col->addWidget(c);

            auto* del = new QPushButton("✕");
            del->setCursor(Qt::PointingHandCursor);
            del->setFixedSize(28, 28);
            del->setStyleSheet(delStyle);
            const int id = bm.id;
            connect(del, &QPushButton::clicked, this, [this, id]() {
                Database::instance().removeBookmark(id, nullptr);
                refreshSidePanels();
                });

            h->addLayout(col, 1);
            h->addWidget(del, 0, Qt::AlignTop);
            bookmarksLayout->addWidget(row);
        }
    }
    bookmarksLayout->addStretch();

    // ---- Цитаты ----
    const auto quotes = Database::instance().listQuotes(currentBookId, nullptr);
    if (quotes.isEmpty())
    {
        auto* e = new QLabel("Здесь будут ваши цитаты");
        e->setAlignment(Qt::AlignCenter);
        e->setStyleSheet("color:#8c847a; font-size:14px; background:transparent;");
        quotesLayout->addWidget(e);
    }
    else
    {
        for (const auto& q : quotes)
        {
            auto* block = new ClickableRow;
            block->setCursor(Qt::PointingHandCursor);
            block->setStyleSheet(QString("QFrame{background:%1;border-radius:10px;}").arg(q.color));
            const int page = q.page;
            block->onClick = [this, page]() {
                emit openReaderAtPageRequested(currentBookId, page);
                };

            auto* v = new QVBoxLayout(block);
            v->setContentsMargins(12, 10, 12, 8);
            v->setSpacing(6);

            // Убираем разделители строк/абзацев (U+2028/U+2029), иначе в QLabel
            // они рисуются вертикальной полоской вдоль текста.
            QString quoteText = q.text;
            quoteText.replace(QChar(0x2028), '\n');
            quoteText.replace(QChar(0x2029), '\n');

            auto* txt = new QLabel(quoteText);
            txt->setWordWrap(true);
            txt->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            txt->setStyleSheet("color:#1a1a1a; font-size:13px; background:transparent;");
            v->addWidget(txt);

            auto* footer = new QHBoxLayout;
            footer->setContentsMargins(0, 0, 0, 0);
            auto* pg = new QLabel(QString("Стр. %1").arg(q.page + 1));
            pg->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            pg->setStyleSheet("color:#333333; font-size:11px; font-weight:bold; background:transparent;");

            auto* del = new QPushButton("✕");
            del->setCursor(Qt::PointingHandCursor);
            del->setFixedSize(22, 22);
            del->setStyleSheet(
                "QPushButton{background:transparent;border:none;color:#444444;font-size:12px;}"
                "QPushButton:hover{color:#000000;}");
            const int id = q.id;
            connect(del, &QPushButton::clicked, this, [this, id]() {
                Database::instance().removeQuote(id, nullptr);
                refreshSidePanels();
                });

            footer->addWidget(pg);
            footer->addStretch();
            footer->addWidget(del);
            v->addLayout(footer);

            quotesLayout->addWidget(block);
        }
    }
    quotesLayout->addStretch();
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

    tabBookmarks->setStyleSheet(
        tabIndex == 0 ? styleActive : styleInactive
    );

    tabNotes->setStyleSheet(
        tabIndex == 1 ? styleActive : styleInactive
    );
}

bool BookInfoPage::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        if (watched == tabBookmarks ||
            watched == tabNotes)
        {
            const int idx = watched->property("tabIndex").toInt();

            setActiveTab(idx);

            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}
