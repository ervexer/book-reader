#include "readerwindow.h"
#include "database.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QIcon>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSize>
#include <QApplication>
#include <QScreen>
#include <QCloseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QAbstractTextDocumentLayout>
#include <QMenu>
#include <QContextMenuEvent>
#include <QPixmap>
#include <QPair>
#include <functional>

// ===== Пастельные цвета выделения =====
static const QStringList kQuoteColors = {
    "#f7a8a8", // красный
    "#a8e6a3", // зелёный
    "#f7d3a8", // оранжевый
    "#a8c8f7", // синий
    "#d3b3f7"  // фиолетовый
};

// Тонкий скроллбар для боковых панелей
static const char* kScrollStyle = R"(
    QScrollArea { background: transparent; border: none; }
    QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }
    QScrollBar::handle:vertical { background: #4a4038; border-radius: 4px; min-height: 24px; }
    QScrollBar::handle:vertical:hover { background: #5e5249; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
)";

// Прозрачная клик-зона у краёв экрана — перелистывание по щелчку.
class EdgeZone : public QWidget
{
public:
    explicit EdgeZone(QWidget* parent = nullptr) : QWidget(parent)
    {
        setCursor(Qt::PointingHandCursor);
    }
    std::function<void()> onClick;

protected:
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && onClick)
            onClick();
        QWidget::mouseReleaseEvent(e);
    }
};

// Кликабельный блок (для карточек цитат в боковой панели).
class ClickableFrame : public QFrame
{
public:
    explicit ClickableFrame(QWidget* parent = nullptr) : QFrame(parent)
    {
        setCursor(Qt::PointingHandCursor);
    }
    std::function<void()> onClick;

protected:
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && onClick)
            onClick();
        QFrame::mouseReleaseEvent(e);
    }
};

// Тонирует иконку в заданный цвет (для активной закладки).
static QIcon tintedIcon(const QString& path, const QColor& color, const QSize& size)
{
    QPixmap src(path);
    if (src.isNull())
        return QIcon(path);

    QPixmap result = src.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPainter p(&result);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(result.rect(), color);
    p.end();
    return QIcon(result);
}

// ============================================================
//                       PageView
// ============================================================

void PageView::paintEvent(QPaintEvent*)
{
    if (!m_doc)
        return;

    const QSizeF ps = m_doc->pageSize();
    if (ps.height() <= 0)
        return;

    QPainter p(this);

    // Сдвигаем документ так, чтобы нужная страница оказалась в начале виджета.
    p.translate(0, -m_pageIndex * ps.height());

    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.palette.setColor(QPalette::Text, QColor("#d7d0c8"));
    ctx.clip = QRectF(0, m_pageIndex * ps.height(), ps.width(), ps.height());

    // Текущее выделение (живое) — полупрозрачная подсветка
    if (m_selStart >= 0 && m_selEnd > m_selStart)
    {
        QAbstractTextDocumentLayout::Selection sel;
        QTextCursor c(m_doc);
        c.setPosition(m_selStart);
        c.setPosition(m_selEnd, QTextCursor::KeepAnchor);
        sel.cursor = c;
        sel.format.setBackground(QColor(255, 140, 0, 90));
        ctx.selections.append(sel);
    }

    m_doc->documentLayout()->draw(&p, ctx);
}

int PageView::docPosAt(const QPoint& localPos) const
{
    if (!m_doc)
        return -1;

    const qreal ph = m_doc->pageSize().height();
    QPointF docPt(localPos.x(), localPos.y() + m_pageIndex * ph);
    return m_doc->documentLayout()->hitTest(docPt, Qt::FuzzyHit);
}

void PageView::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
    {
        m_pressed = true;
        m_pressPos = e->pos();
        if (m_reader)
            m_reader->onPagePress(this, e->pos());
    }
    QWidget::mousePressEvent(e);
}

void PageView::mouseMoveEvent(QMouseEvent* e)
{
    if (m_pressed && (e->buttons() & Qt::LeftButton) && m_reader)
        m_reader->onPageMove(this, e->pos());
    QWidget::mouseMoveEvent(e);
}

void PageView::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && m_pressed)
    {
        m_pressed = false;
        const bool isClick = (e->pos() - m_pressPos).manhattanLength() < 8;
        if (m_reader)
            m_reader->onPageRelease(this, e->pos(),
                e->globalPosition().toPoint(), isClick);
    }
    QWidget::mouseReleaseEvent(e);
}

void PageView::contextMenuEvent(QContextMenuEvent* e)
{
    if (m_reader)
        m_reader->onPageContextMenu(this, e->pos(), e->globalPos());
}

static PageView* makePageView()
{
    auto* v = new PageView;
    v->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    return v;
}

// ============================================================
//                     ReaderWindow
// ============================================================

ReaderWindow::ReaderWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Reader");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumSize(960, 640);
    resize(1180, 760); // геометрия оконного режима (на неё возвращает "развернуть")

    // Центрированная оконная геометрия (управляем вручную: безрамочное окно).
    {
        const int w = 1180, h = 760;
        QRect avail = QApplication::primaryScreen()->availableGeometry();
        normalGeom = QRect(
            avail.x() + (avail.width() - w) / 2,
            avail.y() + (avail.height() - h) / 2,
            w, h);
    }

    setStyleSheet("background:#0d0b08;");

    central = new QWidget;
    setCentralWidget(central);

    // ===== ВЕРХНЯЯ ПАНЕЛЬ =====
    auto* topBar = new QFrame(central);
    topBar->setObjectName("topBar");
    topBar->setStyleSheet("background:#0d0b08;");
    topBar->setFixedHeight(50);
    topBarWidget = topBar;
    topBar->installEventFilter(this); // перетаскивание окна за панель

    auto* top = new QHBoxLayout(topBar);
    top->setContentsMargins(12, 8, 12, 8);
    top->setSpacing(6);

    auto makeIconBtn = [&](const QString& icon) {
        auto* b = new QPushButton(topBar);
        b->setFixedSize(34, 34);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet("QPushButton{background:transparent;border:none;border-radius:8px;}"
                         "QPushButton:hover{background:#1d1914;}");
        b->setIcon(QIcon(icon));
        b->setIconSize(QSize(18, 18));
        return b;
    };

    menuBtn = makeIconBtn("image icon/menu.png");
    settingsBtn = makeIconBtn("image icon/settings.png");
    bookmarkBtn = makeIconBtn("image icon/bookmark.png");

    top->addWidget(menuBtn);
    top->addWidget(settingsBtn);
    top->addWidget(bookmarkBtn);

    top->addStretch();

    auto makeTab = [&](const QString& text) {
        auto* b = new QPushButton(text, topBar);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(
            "QPushButton{background:transparent;border:none;color:#8c847a;font-size:13px;padding:6px 12px;}"
            "QPushButton:hover{color:#d2c7bb;}");
        return b;
    };

    tabContents = makeTab("Содержание");
    tabBookmarks = makeTab("Закладки");
    tabQuotes = makeTab("Цитаты");

    top->addWidget(tabContents);
    top->addWidget(tabBookmarks);
    top->addWidget(tabQuotes);

    top->addStretch();

    minBtn = new QPushButton(topBar);
    maxBtn = new QPushButton(topBar);
    closeBtn = new QPushButton(topBar);
    minBtn->setIcon(QIcon("image icon/minus.png"));
    maxBtn->setIcon(QIcon("image icon/copy.png"));
    closeBtn->setIcon(QIcon("image icon/cross.png"));

    for (auto* b : { minBtn, maxBtn, closeBtn })
    {
        b->setFixedSize(30, 30);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(
            "QPushButton{background:transparent;border:none;border-radius:8px;}"
            "QPushButton:hover{background:#1d1914;}");
        top->addWidget(b);
    }

    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    connect(minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(maxBtn, &QPushButton::clicked, this, [this]() {
        // Управляем геометрией вручную (безрамочное окно): без showMaximized().
        if (winMaximized)
        {
            setGeometry(normalGeom);
            winMaximized = false;
        }
        else
        {
            normalGeom = geometry();
            setGeometry(QApplication::primaryScreen()->availableGeometry());
            winMaximized = true;
        }
        });

    connect(menuBtn, &QPushButton::clicked, this, [this]() { openLeftPanel(PanelMode::Info); });
    connect(settingsBtn, &QPushButton::clicked, this, [this]() { toggleSettingsPanel(); });
    connect(bookmarkBtn, &QPushButton::clicked, this, [this]() { toggleBookmark(); });
    connect(tabContents, &QPushButton::clicked, this, [this]() { openLeftPanel(PanelMode::Contents); });
    connect(tabBookmarks, &QPushButton::clicked, this, [this]() { openLeftPanel(PanelMode::Bookmarks); });
    connect(tabQuotes, &QPushButton::clicked, this, [this]() { openLeftPanel(PanelMode::Quotes); });

    // ===== НИЖНЯЯ ПАНЕЛЬ =====
    auto* bottomBar = new QFrame(central);
    bottomBar->setObjectName("bottomBar");
    bottomBar->setStyleSheet("background:#0d0b08;");
    bottomBar->setFixedHeight(52);

    auto* bottom = new QVBoxLayout(bottomBar);
    bottom->setContentsMargins(40, 6, 40, 10);
    bottom->setSpacing(6);

    progressLbl = new QLabel("0/0", bottomBar);
    progressLbl->setStyleSheet("color:#8c847a; font-size:12px;");

    progressBg = new QFrame(bottomBar);
    progressBg->setFixedHeight(3);
    progressBg->setStyleSheet("background:#2a241e; border-radius:2px;");

    progressFill = new QFrame(progressBg);
    progressFill->setGeometry(0, 0, 0, 3);
    progressFill->setStyleSheet("background:#ff8c00; border-radius:2px;");

    bottom->addWidget(progressLbl);
    bottom->addWidget(progressBg);

    // ===== ОБЛАСТЬ ТЕКСТА =====
    doc = new QTextDocument(this);
    doc->setDocumentMargin(4);

    leftPageLbl = makePageView();
    leftPageLbl->setParent(central);
    leftPageLbl->setDocument(doc);
    leftPageLbl->setReader(this);

    rightPageLbl = makePageView();
    rightPageLbl->setParent(central);
    rightPageLbl->setDocument(doc);
    rightPageLbl->setReader(this);

    // Разделительная полоса между страницами
    centerDivider = new QFrame(central);
    centerDivider->setStyleSheet("background:rgba(255,255,255,0.07);");

    // Клик-зоны у самых краёв экрана — листание назад/вперёд.
    auto* lEdge = new EdgeZone(central);
    lEdge->onClick = [this]() {
        if (colorPopup->isVisible() || leftPanel->isVisible() || settingsPanel->isVisible())
            hidePopups();
        else
            goToPage(currentPage - 2);
    };
    leftEdge = lEdge;

    auto* rEdge = new EdgeZone(central);
    rEdge->onClick = [this]() {
        if (colorPopup->isVisible() || leftPanel->isVisible() || settingsPanel->isVisible())
            hidePopups();
        else
            goToPage(currentPage + 2);
    };
    rightEdge = rEdge;

    // ===== ЛЕВАЯ БОКОВАЯ ПАНЕЛЬ =====
    leftPanel = new QFrame(central);
    leftPanel->setStyleSheet("background:#161310; border-right:1px solid #241f18;");
    leftPanel->hide();

    auto* lpRoot = new QVBoxLayout(leftPanel);
    lpRoot->setContentsMargins(18, 16, 12, 16);
    lpRoot->setSpacing(12);

    auto* lpHeader = new QHBoxLayout;
    panelTitle = new QLabel("Содержание");
    panelTitle->setStyleSheet("color:#d2c7bb; font-size:16px; font-weight:bold;");
    auto* lpClose = new QPushButton("✕");
    lpClose->setFixedSize(28, 28);
    lpClose->setCursor(Qt::PointingHandCursor);
    lpClose->setStyleSheet(
        "QPushButton{background:transparent;border:none;color:#8c847a;font-size:14px;}"
        "QPushButton:hover{color:#d2c7bb;}");
    connect(lpClose, &QPushButton::clicked, this, [this]() { closeLeftPanel(); });
    lpHeader->addWidget(panelTitle);
    lpHeader->addStretch();
    lpHeader->addWidget(lpClose);
    lpRoot->addLayout(lpHeader);

    panelScroll = new QScrollArea(leftPanel);
    panelScroll->setWidgetResizable(true);
    panelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    panelScroll->setStyleSheet(kScrollStyle);

    panelContent = new QWidget;
    panelContent->setStyleSheet("background:transparent;");
    panelLayout = new QVBoxLayout(panelContent);
    panelLayout->setContentsMargins(0, 0, 8, 0);
    panelLayout->setSpacing(8);
    panelLayout->addStretch();

    panelScroll->setWidget(panelContent);
    lpRoot->addWidget(panelScroll, 1);

    // ===== ПАНЕЛЬ НАСТРОЕК =====
    buildSettingsPanel();

    // ===== ПОПАП ВЫБОРА ЦВЕТА =====
    colorPopup = new QFrame(central);
    colorPopup->setStyleSheet("background:#1c1610; border:1px solid #2a241e; border-radius:10px;");
    colorPopup->hide();

    auto* cpl = new QHBoxLayout(colorPopup);
    cpl->setContentsMargins(8, 8, 8, 8);
    cpl->setSpacing(8);

    for (const QString& c : kQuoteColors)
    {
        auto* sw = new QPushButton(colorPopup);
        sw->setFixedSize(26, 26);
        sw->setCursor(Qt::PointingHandCursor);
        sw->setStyleSheet(QString(
            "QPushButton{background:%1;border:none;border-radius:13px;}"
            "QPushButton:hover{border:2px solid #ffffff;}").arg(c));
        connect(sw, &QPushButton::clicked, this, [this, c]() { saveQuoteWithColor(c); });
        cpl->addWidget(sw);
    }
    colorPopup->adjustSize();
}

// ============================================================
//                      Открытие книги
// ============================================================

void ReaderWindow::openBook(int bookId, int chapterIndex, int page)
{
    currentBookId = bookId;

    Database::instance().openOrCreate();
    Database::instance().getSettings(bookId, &settings, nullptr);

    DbBook bookMeta;
    if (Database::instance().getBook(bookId, &bookMeta, nullptr))
        setWindowTitle(bookMeta.title.isEmpty() ? "Чтение" : bookMeta.title);

    currentPage = 0;

    const auto chapters = Database::instance().listChapters(bookId, nullptr);
    buildDocument(chapters);
    applyFormatting();
    applyQuoteHighlights();

    requestedChapter = chapterIndex;
    requestedPage = page;
    hasPendingStart = true;

    refreshSettingsControls();

    // Показываем развёрнутым на рабочую область (вручную, без showMaximized()).
    // Это вызовет resizeEvent → rebuildPages + resolvePendingStart + updateUi.
    setGeometry(QApplication::primaryScreen()->availableGeometry());
    winMaximized = true;
    show();

    if (hasPendingStart)
    {
        rebuildPages();
        resolvePendingStart();
        updateUi();
    }

    raise();
    activateWindow();
}

void ReaderWindow::buildDocument(const QVector<DbChapter>& chapters)
{
    chapterStartPos.clear();
    chapterTitles.clear();
    doc->clear();

    if (chapters.isEmpty())
    {
        doc->setHtml("<p>Текст книги не найден</p>");
        chapterStartPos.append(0);
        chapterTitles.append("Без названия");
        return;
    }

    QTextCursor cur(doc);
    for (int i = 0; i < chapters.size(); ++i)
    {
        if (i > 0)
            cur.insertBlock();

        chapterStartPos.append(cur.position());

        const QString title = chapters[i].title.trimmed();
        chapterTitles.append(title.isEmpty() ? QString("Глава %1").arg(i + 1) : title);

        if (!title.isEmpty())
        {
            cur.insertHtml("<h3>" + title.toHtmlEscaped() + "</h3>");
            cur.insertBlock();
        }

        cur.insertHtml(chapters[i].content);
    }
}

int ReaderWindow::pageForChapter(int chapterIndex) const
{
    if (chapterIndex < 0 || chapterIndex >= chapterStartPos.size())
        return 0;

    const qreal ph = doc->pageSize().height();
    if (ph <= 0)
        return 0;

    QTextCursor c(doc);
    c.setPosition(chapterStartPos[chapterIndex]);
    const QRectF r = doc->documentLayout()->blockBoundingRect(c.block());

    return int(r.top() / ph);
}

int ReaderWindow::pageForPos(int pos) const
{
    const qreal ph = doc->pageSize().height();
    if (ph <= 0)
        return 0;

    QTextCursor c(doc);
    c.setPosition(qBound(0, pos, qMax(0, doc->characterCount() - 1)));
    const QRectF r = doc->documentLayout()->blockBoundingRect(c.block());

    int p = int(r.top() / ph);
    if (p % 2 != 0) // выравниваем на левую страницу разворота
        p -= 1;
    return p;
}

int ReaderWindow::chapterForPage(int page) const
{
    int result = 0;
    for (int i = 0; i < chapterStartPos.size(); ++i)
    {
        if (pageForChapter(i) <= page)
            result = i;
        else
            break;
    }
    return result;
}

void ReaderWindow::resolvePendingStart()
{
    int startPage = 0;

    if (requestedPage >= 0)
    {
        startPage = requestedPage;
    }
    else if (requestedChapter < 0)
    {
        DbBook b;
        if (Database::instance().getBook(currentBookId, &b, nullptr))
            startPage = b.lastPage;
    }
    else
    {
        startPage = pageForChapter(requestedChapter);
    }

    if (startPage % 2 != 0)
        startPage -= 1;

    currentPage = qBound(0, startPage, qMax(0, pageCount - 1));
    hasPendingStart = false;
}

int ReaderWindow::pageAreaHeight() const
{
    return qMax(100, height() - 50 - 52 - 20);
}

int ReaderWindow::pageAreaWidth() const
{
    return qMax(200, width() - 80);
}

void ReaderWindow::applyFormatting()
{
    QFont f(settings.fontFamily);
    f.setPointSize(settings.fontSize);
    doc->setDefaultFont(f);

    Qt::Alignment alignment = Qt::AlignLeft;
    switch (settings.align)
    {
    case 1: alignment = Qt::AlignHCenter; break;
    case 2: alignment = Qt::AlignRight; break;
    case 3: alignment = Qt::AlignJustify; break;
    default: alignment = Qt::AlignLeft; break;
    }

    QTextCursor cursor(doc);
    cursor.select(QTextCursor::Document);

    QTextBlockFormat bf;
    bf.setLineHeight(settings.lineHeight * 100.0, QTextBlockFormat::ProportionalHeight);
    bf.setAlignment(alignment);
    cursor.mergeBlockFormat(bf);
}

void ReaderWindow::mergeHighlight(int start, int end, const QColor& color)
{
    if (!doc || start < 0 || end <= start)
        return;

    QTextCursor cur(doc);
    cur.setPosition(start);
    cur.setPosition(end, QTextCursor::KeepAnchor);

    QTextCharFormat fmt;
    fmt.setBackground(color);
    fmt.setForeground(QColor("#1a1a1a"));
    cur.mergeCharFormat(fmt);
}

void ReaderWindow::applyQuoteHighlights()
{
    if (!currentBookId)
        return;

    const auto quotes = Database::instance().listQuotes(currentBookId, nullptr);
    for (const auto& q : quotes)
        mergeHighlight(q.startPos, q.endPos, QColor(q.color));
}

void ReaderWindow::rebuildPages()
{
    const int pageW = qMax(50, (pageAreaWidth() - 40) / 2);
    const int pageH = qMax(50, pageAreaHeight());

    doc->setPageSize(QSizeF(pageW, pageH));

    pageCount = qMax(1, doc->documentLayout()->pageCount());

    const int maxPage = qMax(0, pageCount - 1);
    currentPage = qBound(0, currentPage, maxPage);

    Database::instance().updateTotalPages(currentBookId, pageCount, nullptr);
}

void ReaderWindow::goToPage(int pageIndex)
{
    const int maxPage = qMax(0, pageCount - 1);
    currentPage = qBound(0, pageIndex, maxPage);
    updateUi();
}

void ReaderWindow::updateUi()
{
    leftPageLbl->setPageIndex(currentPage);
    rightPageLbl->setPageIndex(currentPage + 1);

    const int total = pageCount;
    const int shown = qMin(currentPage + 2, total);
    progressLbl->setText(QString("%1 / %2").arg(shown).arg(total));

    const int w = progressBg->width();
    const double ratio = total > 0 ? (double)shown / (double)total : 0.0;
    progressFill->setGeometry(0, 0, int(w * ratio), progressBg->height());

    updateBookmarkButton();
}

bool ReaderWindow::eventFilter(QObject* obj, QEvent* event)
{
    // Перетаскивание окна ридера за верхнюю панель (только в оконном режиме).
    if (obj == topBarWidget)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && !winMaximized)
            {
                dragging = true;
                dragPos = me->globalPosition().toPoint() - frameGeometry().topLeft();
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            auto* me = static_cast<QMouseEvent*>(event);
            if (dragging && (me->buttons() & Qt::LeftButton) && !winMaximized)
            {
                move(me->globalPosition().toPoint() - dragPos);
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            dragging = false;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void ReaderWindow::resizeEvent(QResizeEvent* e)
{
    QMainWindow::resizeEvent(e);

    const int W = width();
    const int H = height();
    const int topH = 50;
    const int bottomH = 52;
    const int padH = 10;
    const int padSide = 40;
    const int gap = 40;
    const int panelW = 360;
    const int settingsW = 380;

    findChild<QFrame*>("topBar")->setGeometry(0, 0, W, topH);
    findChild<QFrame*>("bottomBar")->setGeometry(0, H - bottomH, W, bottomH);

    const int textY = topH + padH;
    const int textH = H - topH - bottomH - padH * 2;
    const int textW = (W - padSide * 2 - gap) / 2;

    leftPageLbl->setGeometry(padSide, textY, textW, textH);
    rightPageLbl->setGeometry(padSide + textW + gap, textY, textW, textH);

    centerDivider->setGeometry(padSide + textW + gap / 2, textY, 1, textH);

    // Клик-зоны у краёв (поля по бокам страниц)
    leftEdge->setGeometry(0, textY, padSide, textH);
    rightEdge->setGeometry(W - padSide, textY, padSide, textH);

    // И боковая панель, и панель настроек теперь открываются слева.
    leftPanel->setGeometry(0, topH, panelW, H - topH);
    settingsPanel->setGeometry(0, topH, settingsW, H - topH);

    rebuildPages();
    if (hasPendingStart)
        resolvePendingStart();
    updateUi();
}

void ReaderWindow::wheelEvent(QWheelEvent* e)
{
    const int delta = e->angleDelta().y();
    if (delta < 0)
        goToPage(currentPage + 2);
    else if (delta > 0)
        goToPage(currentPage - 2);
    e->accept();
}

// ============================================================
//                   Левая боковая панель
// ============================================================

void ReaderWindow::clearPanelContent()
{
    // Скрываем и удаляем все элементы. hide() сразу убирает их с экрана,
    // не дожидаясь асинхронного deleteLater(), иначе панели наслаиваются.
    while (QLayoutItem* it = panelLayout->takeAt(0))
    {
        if (auto* w = it->widget())
        {
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete it;
    }
}

void ReaderWindow::openLeftPanel(PanelMode mode)
{
    if (leftPanel->isVisible() && panelMode == mode)
    {
        closeLeftPanel();
        return;
    }

    panelMode = mode;
    settingsPanel->hide();
    rebuildPanelContent();
    leftPanel->show();
    leftPanel->raise();
    updateTabHighlight();
}

void ReaderWindow::closeLeftPanel()
{
    leftPanel->hide();
    panelMode = PanelMode::None;
    updateTabHighlight();
}

void ReaderWindow::rebuildPanelContent()
{
    clearPanelContent();

    switch (panelMode)
    {
    case PanelMode::Info:      panelTitle->setText("О книге");      buildInfoContent();      break;
    case PanelMode::Contents:  panelTitle->setText("Содержание");   buildContentsContent();  break;
    case PanelMode::Bookmarks: panelTitle->setText("Закладки");     buildBookmarksContent(); break;
    case PanelMode::Quotes:    panelTitle->setText("Цитаты");       buildQuotesContent();    break;
    default: break;
    }

    panelLayout->addStretch();
}

void ReaderWindow::buildInfoContent()
{
    DbBook b;
    if (!Database::instance().getBook(currentBookId, &b, nullptr))
        return;

    auto* title = new QLabel(b.title.isEmpty() ? "Без названия" : b.title);
    title->setWordWrap(true);
    title->setStyleSheet("color:#ff8c00; font-size:20px; font-weight:bold; background:transparent;");
    panelLayout->addWidget(title);

    auto* author = new QLabel(b.author.isEmpty() ? "Неизвестный автор" : b.author);
    author->setWordWrap(true);
    author->setStyleSheet("color:#d2c7bb; font-size:15px; background:transparent;");
    panelLayout->addWidget(author);

    auto addField = [&](const QString& label, const QString& value) {
        if (value.trimmed().isEmpty())
            return;
        auto* cap = new QLabel(label);
        cap->setStyleSheet("color:#8c847a; font-size:11px; background:transparent; margin-top:6px;");
        panelLayout->addWidget(cap);
        auto* val = new QLabel(value);
        val->setWordWrap(true);
        val->setStyleSheet("color:#cfc7bd; font-size:13px; background:transparent;");
        panelLayout->addWidget(val);
    };

    addField("Переводчик", b.translator);
    addField("Жанр", b.genre);
    addField("Страниц", b.totalPages > 0 ? QString::number(b.totalPages) : QString());
    addField("Аннотация", b.annotation);
}

void ReaderWindow::buildContentsContent()
{
    const int activeChapter = chapterForPage(currentPage);

    const QString rowStyle =
        "QPushButton{background:#1d1914;border:none;border-radius:8px;color:#cfc7bd;"
        "text-align:left;padding:8px 12px;font-size:13px;}"
        "QPushButton:hover{background:#262019;}";
    const QString activeStyle =
        "QPushButton{background:#3a2e1c;border:none;border-radius:8px;color:#ff8c00;"
        "text-align:left;padding:8px 12px;font-size:13px;font-weight:bold;}";

    for (int i = 0; i < chapterTitles.size(); ++i)
    {
        const bool active = (i == activeChapter);
        auto* row = new QPushButton(chapterTitles[i]);
        row->setCursor(Qt::PointingHandCursor);
        row->setMinimumHeight(40);
        row->setStyleSheet(active ? activeStyle : rowStyle);

        const int idx = i;
        connect(row, &QPushButton::clicked, this, [this, idx]() {
            int p = pageForChapter(idx);
            if (p % 2 != 0) p -= 1;
            goToPage(p);
            closeLeftPanel();
            });

        panelLayout->addWidget(row);
    }
}

void ReaderWindow::buildBookmarksContent()
{
    const auto bookmarks = Database::instance().listBookmarks(currentBookId, nullptr);

    if (bookmarks.isEmpty())
    {
        auto* empty = new QLabel("Закладок пока нет.\nДобавьте флажком в верхнем меню.");
        empty->setWordWrap(true);
        empty->setStyleSheet("color:#8c847a; font-size:13px; background:transparent;");
        panelLayout->addWidget(empty);
        return;
    }

    const QString navStyle =
        "QPushButton{background:transparent;border:none;color:#cfc7bd;"
        "text-align:left;padding:8px 4px;font-size:13px;}"
        "QPushButton:hover{color:#ff8c00;}";
    const QString delStyle =
        "QPushButton{background:transparent;border:none;color:#8c847a;font-size:14px;}"
        "QPushButton:hover{color:#ff8c00;}";

    for (const auto& bm : bookmarks)
    {
        // Каждая строка — отдельный контейнер-виджет, чтобы clearPanelContent
        // мог корректно удалить её (вложенные layout'ы не наслаивались).
        auto* rowWidget = new QWidget;
        rowWidget->setStyleSheet("background:transparent;");
        auto* rowLay = new QHBoxLayout(rowWidget);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->setSpacing(6);

        const QString text = QString("Стр. %1  •  %2")
            .arg(bm.page + 1)
            .arg(bm.chapterTitle.isEmpty() ? "—" : bm.chapterTitle);

        auto* nav = new QPushButton(text);
        nav->setCursor(Qt::PointingHandCursor);
        nav->setMinimumHeight(38);
        nav->setStyleSheet(navStyle);
        const int page = bm.page;
        connect(nav, &QPushButton::clicked, this, [this, page]() {
            int p = page; if (p % 2 != 0) p -= 1;
            goToPage(p);
            closeLeftPanel();
            });

        auto* del = new QPushButton("✕");
        del->setCursor(Qt::PointingHandCursor);
        del->setFixedSize(34, 38);
        del->setStyleSheet(delStyle);
        const int id = bm.id;
        connect(del, &QPushButton::clicked, this, [this, id]() {
            Database::instance().removeBookmark(id, nullptr);
            rebuildPanelContent();
            updateBookmarkButton();
            });

        rowLay->addWidget(nav, 1);
        rowLay->addWidget(del);
        panelLayout->addWidget(rowWidget);
    }
}

void ReaderWindow::buildQuotesContent()
{
    const auto quotes = Database::instance().listQuotes(currentBookId, nullptr);

    if (quotes.isEmpty())
    {
        auto* empty = new QLabel("Цитат пока нет.\nВыделите текст мышью и выберите цвет.");
        empty->setWordWrap(true);
        empty->setStyleSheet("color:#8c847a; font-size:13px; background:transparent;");
        panelLayout->addWidget(empty);
        return;
    }

    for (const auto& q : quotes)
    {
        // Весь блок кликабельный — переносит к странице цитаты.
        auto* block = new ClickableFrame;
        block->setStyleSheet(QString(
            "QFrame{background:%1;border-radius:10px;}").arg(q.color));

        const int s = q.startPos, en = q.endPos;
        block->onClick = [this, s]() {
            goToPage(pageForPos(s));
            closeLeftPanel();
            };

        auto* v = new QVBoxLayout(block);
        v->setContentsMargins(12, 10, 12, 8);
        v->setSpacing(6);

        // Убираем разделители строк/абзацев (U+2028/U+2029) — иначе в QLabel
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
        connect(del, &QPushButton::clicked, this, [this, id, s, en]() {
            Database::instance().removeQuote(id, nullptr);
            // Снять подсветку (вернуть прозрачный фон и обычный цвет текста)
            QTextCursor cur(doc);
            cur.setPosition(s);
            cur.setPosition(en, QTextCursor::KeepAnchor);
            QTextCharFormat fmt;
            fmt.setBackground(Qt::transparent);
            fmt.setForeground(QColor("#d7d0c8"));
            cur.mergeCharFormat(fmt);
            leftPageLbl->update();
            rightPageLbl->update();
            rebuildPanelContent();
            });

        footer->addWidget(pg);
        footer->addStretch();
        footer->addWidget(del);
        v->addLayout(footer);

        panelLayout->addWidget(block);
    }
}

void ReaderWindow::updateTabHighlight()
{
    const QString base =
        "QPushButton{background:transparent;border:none;color:#8c847a;font-size:13px;padding:6px 12px;}"
        "QPushButton:hover{color:#d2c7bb;}";
    const QString active =
        "QPushButton{background:transparent;border:none;color:#ff8c00;font-size:13px;font-weight:bold;padding:6px 12px;}";

    const bool open = leftPanel->isVisible();
    tabContents->setStyleSheet(open && panelMode == PanelMode::Contents ? active : base);
    tabBookmarks->setStyleSheet(open && panelMode == PanelMode::Bookmarks ? active : base);
    tabQuotes->setStyleSheet(open && panelMode == PanelMode::Quotes ? active : base);
}

// ============================================================
//                    Панель настроек
// ============================================================

void ReaderWindow::buildSettingsPanel()
{
    settingsPanel = new QFrame(central);
    settingsPanel->setStyleSheet("background:#161310; border-right:1px solid #241f18;");
    settingsPanel->hide();

    auto* root = new QVBoxLayout(settingsPanel);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(16);

    // Заголовок
    auto* header = new QHBoxLayout;
    auto* title = new QLabel("Настройки чтения");
    title->setStyleSheet("color:#d2c7bb; font-size:16px; font-weight:bold; background:transparent;");
    auto* closeX = new QPushButton("✕");
    closeX->setFixedSize(28, 28);
    closeX->setCursor(Qt::PointingHandCursor);
    closeX->setStyleSheet(
        "QPushButton{background:transparent;border:none;color:#8c847a;font-size:14px;}"
        "QPushButton:hover{color:#d2c7bb;}");
    connect(closeX, &QPushButton::clicked, this, [this]() { settingsPanel->hide(); });
    header->addWidget(title);
    header->addStretch();
    header->addWidget(closeX);
    root->addLayout(header);

    auto addCaption = [&](const QString& text) {
        auto* c = new QLabel(text);
        c->setStyleSheet("color:#8c847a; font-size:11px; letter-spacing:1px; background:transparent;");
        root->addWidget(c);
    };

    // ----- Шрифт -----
    addCaption("ШРИФТ");
    const QVector<QPair<QString, QString>> fonts = {
        { "Times New Roman", "Times New Roman" },
        { "Georgia", "Georgia" },
        { "Verdana", "Verdana" },
        { "Courier New", "Courier New" }
    };

    auto* fontGrid = new QVBoxLayout;
    fontGrid->setSpacing(8);
    QHBoxLayout* curRow = nullptr;
    for (int i = 0; i < fonts.size(); ++i)
    {
        if (i % 2 == 0)
        {
            curRow = new QHBoxLayout;
            curRow->setSpacing(8);
            fontGrid->addLayout(curRow);
        }
        auto* b = new QPushButton(fonts[i].first);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(38);
        const QString fam = fonts[i].second;
        connect(b, &QPushButton::clicked, this, [this, fam]() {
            settings.fontFamily = fam;
            applySettingsLive();
            refreshSettingsControls();
            });
        fontButtons.push_back(b);
        curRow->addWidget(b);
    }
    root->addLayout(fontGrid);

    // ----- Размер шрифта -----
    addCaption("РАЗМЕР ТЕКСТА");
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(10);
        auto* minus = new QPushButton("A−");
        auto* plus = new QPushButton("A+");
        fontSizeValueLbl = new QLabel("22");
        fontSizeValueLbl->setAlignment(Qt::AlignCenter);
        fontSizeValueLbl->setStyleSheet("color:#d2c7bb; font-size:15px; background:transparent;");
        for (auto* b : { minus, plus })
        {
            b->setFixedHeight(38);
            b->setCursor(Qt::PointingHandCursor);
            b->setStyleSheet(
                "QPushButton{background:#1d1914;border:none;border-radius:8px;color:#cfc7bd;font-size:15px;}"
                "QPushButton:hover{background:#262019;}");
        }
        connect(minus, &QPushButton::clicked, this, [this]() {
            settings.fontSize = qMax(12, settings.fontSize - 1);
            applySettingsLive(); refreshSettingsControls();
            });
        connect(plus, &QPushButton::clicked, this, [this]() {
            settings.fontSize = qMin(40, settings.fontSize + 1);
            applySettingsLive(); refreshSettingsControls();
            });
        row->addWidget(minus, 1);
        row->addWidget(fontSizeValueLbl, 1);
        row->addWidget(plus, 1);
        root->addLayout(row);
    }

    // ----- Межстрочный интервал -----
    addCaption("ИНТЕРВАЛ");
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(10);
        auto* minus = new QPushButton("−");
        auto* plus = new QPushButton("+");
        lineHeightValueLbl = new QLabel("1.4");
        lineHeightValueLbl->setAlignment(Qt::AlignCenter);
        lineHeightValueLbl->setStyleSheet("color:#d2c7bb; font-size:15px; background:transparent;");
        for (auto* b : { minus, plus })
        {
            b->setFixedHeight(38);
            b->setCursor(Qt::PointingHandCursor);
            b->setStyleSheet(
                "QPushButton{background:#1d1914;border:none;border-radius:8px;color:#cfc7bd;font-size:16px;}"
                "QPushButton:hover{background:#262019;}");
        }
        connect(minus, &QPushButton::clicked, this, [this]() {
            settings.lineHeight = qMax(1.0, settings.lineHeight - 0.1);
            applySettingsLive(); refreshSettingsControls();
            });
        connect(plus, &QPushButton::clicked, this, [this]() {
            settings.lineHeight = qMin(2.5, settings.lineHeight + 0.1);
            applySettingsLive(); refreshSettingsControls();
            });
        row->addWidget(minus, 1);
        row->addWidget(lineHeightValueLbl, 1);
        row->addWidget(plus, 1);
        root->addLayout(row);
    }

    // ----- Выравнивание -----
    addCaption("ВЫРАВНИВАНИЕ");
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        const QStringList labels = { "◧", "▥", "◨", "▦" };
        const QStringList tips = { "Слева", "По центру", "Справа", "По ширине" };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = new QPushButton(labels[i]);
            b->setToolTip(tips[i]);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedHeight(38);
            const int alignVal = i;
            connect(b, &QPushButton::clicked, this, [this, alignVal]() {
                settings.align = alignVal;
                applySettingsLive(); refreshSettingsControls();
                });
            alignButtons.push_back(b);
            row->addWidget(b, 1);
        }
        root->addLayout(row);
    }

    root->addStretch();

    // ----- Сброс к настройкам по умолчанию -----
    auto* resetBtn = new QPushButton("Сбросить настройки");
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setFixedHeight(40);
    resetBtn->setStyleSheet(
        "QPushButton{background:#2a201a;border:1px solid #3a2f22;border-radius:8px;color:#cfc7bd;font-size:13px;}"
        "QPushButton:hover{background:#3a2f22;color:#ff8c00;}");
    connect(resetBtn, &QPushButton::clicked, this, [this]() { resetSettings(); });
    root->addWidget(resetBtn);
}

void ReaderWindow::resetSettings()
{
    settings = ReaderSettings(); // значения по умолчанию из структуры
    applySettingsLive();
    refreshSettingsControls();
}

void ReaderWindow::toggleSettingsPanel()
{
    if (settingsPanel->isVisible())
    {
        settingsPanel->hide();
        return;
    }
    closeLeftPanel();
    refreshSettingsControls();
    settingsPanel->show();
    settingsPanel->raise();
}

void ReaderWindow::applySettingsLive()
{
    applyFormatting();
    applyQuoteHighlights();
    rebuildPages();
    updateUi();
    leftPageLbl->update();
    rightPageLbl->update();
    if (currentBookId)
        Database::instance().upsertSettings(currentBookId, settings, nullptr);
}

void ReaderWindow::refreshSettingsControls()
{
    if (fontSizeValueLbl)
        fontSizeValueLbl->setText(QString::number(settings.fontSize));
    if (lineHeightValueLbl)
        lineHeightValueLbl->setText(QString::number(settings.lineHeight, 'f', 1));

    const QString fontBase =
        "QPushButton{background:#1d1914;border:none;border-radius:8px;color:#cfc7bd;font-size:13px;}"
        "QPushButton:hover{background:#262019;}";
    const QString fontActive =
        "QPushButton{background:#ff8c00;border:none;border-radius:8px;color:#1a1a1a;font-size:13px;font-weight:bold;}";

    const QStringList fams = { "Times New Roman", "Georgia", "Verdana", "Courier New" };
    for (int i = 0; i < fontButtons.size() && i < fams.size(); ++i)
        fontButtons[i]->setStyleSheet(fams[i] == settings.fontFamily ? fontActive : fontBase);

    const QString alignBase =
        "QPushButton{background:#1d1914;border:none;border-radius:8px;color:#cfc7bd;font-size:15px;}"
        "QPushButton:hover{background:#262019;}";
    const QString alignActive =
        "QPushButton{background:#ff8c00;border:none;border-radius:8px;color:#1a1a1a;font-size:15px;}";
    for (int i = 0; i < alignButtons.size(); ++i)
        alignButtons[i]->setStyleSheet(i == settings.align ? alignActive : alignBase);
}

// ============================================================
//                       Закладки
// ============================================================

void ReaderWindow::toggleBookmark()
{
    if (!currentBookId)
        return;

    if (Database::instance().hasBookmark(currentBookId, currentPage))
    {
        Database::instance().removeBookmarkByPage(currentBookId, currentPage, nullptr);
    }
    else
    {
        const int ch = chapterForPage(currentPage);
        const QString t = (ch >= 0 && ch < chapterTitles.size()) ? chapterTitles[ch] : QString();
        Database::instance().addBookmark(currentBookId, currentPage, ch, t, nullptr);
    }

    updateBookmarkButton();
    if (leftPanel->isVisible() && panelMode == PanelMode::Bookmarks)
        rebuildPanelContent();
}

void ReaderWindow::updateBookmarkButton()
{
    const bool on = currentBookId
        && Database::instance().hasBookmark(currentBookId, currentPage);

    const QSize sz(18, 18);
    bookmarkBtn->setIcon(on
        ? tintedIcon("image icon/bookmark.png", QColor("#ff8c00"), sz)   // оранжевая — закладка есть
        : tintedIcon("image icon/bookmark.png", QColor("#8c847a"), sz)); // серая — закладки нет

    bookmarkBtn->setStyleSheet(
        "QPushButton{background:transparent;border:none;border-radius:8px;}"
        "QPushButton:hover{background:#1d1914;}");
}

// ============================================================
//                  Выделение / цитаты
// ============================================================

void ReaderWindow::onPagePress(PageView* src, const QPoint& localPos)
{
    hideColorPopup();
    const int pos = src->docPosAt(localPos);
    selAnchor = pos;
    selStart = -1;
    selEnd = -1;
    leftPageLbl->setSelection(-1, -1);
    rightPageLbl->setSelection(-1, -1);
}

void ReaderWindow::onPageMove(PageView* src, const QPoint& localPos)
{
    const int pos = src->docPosAt(localPos);
    if (selAnchor < 0 || pos < 0)
        return;

    selStart = qMin(selAnchor, pos);
    selEnd = qMax(selAnchor, pos);
    leftPageLbl->setSelection(selStart, selEnd);
    rightPageLbl->setSelection(selStart, selEnd);
}

void ReaderWindow::onPageRelease(PageView* src, const QPoint& localPos,
    const QPoint& globalPos, bool isClick)
{
    Q_UNUSED(localPos);

    if (isClick)
    {
        // Клик: если открыты панели/попап — закрыть; иначе перелистнуть.
        if (colorPopup->isVisible() || leftPanel->isVisible() || settingsPanel->isVisible())
        {
            hidePopups();
            return;
        }
        const int dir = (src == leftPageLbl) ? -1 : +1;
        goToPage(currentPage + dir * 2);
        return;
    }

    // Завершено выделение
    if (selStart >= 0 && selEnd > selStart)
    {
        showColorPopup(globalPos);
    }
    else
    {
        leftPageLbl->setSelection(-1, -1);
        rightPageLbl->setSelection(-1, -1);
    }
}

void ReaderWindow::showColorPopup(const QPoint& globalPos)
{
    QPoint p = central->mapFromGlobal(globalPos);
    colorPopup->adjustSize();

    int x = p.x() - colorPopup->width() / 2;
    int y = p.y() + 12;
    x = qBound(8, x, central->width() - colorPopup->width() - 8);
    y = qBound(8, y, central->height() - colorPopup->height() - 8);

    colorPopup->move(x, y);
    colorPopup->show();
    colorPopup->raise();
}

void ReaderWindow::hideColorPopup()
{
    colorPopup->hide();
}

void ReaderWindow::saveQuoteWithColor(const QString& hexColor)
{
    if (selStart < 0 || selEnd <= selStart)
    {
        hideColorPopup();
        return;
    }

    QTextCursor cur(doc);
    cur.setPosition(selStart);
    cur.setPosition(selEnd, QTextCursor::KeepAnchor);
    QString text = cur.selectedText();
    text.replace(QChar(0x2028), '\n'); // разделитель строк → перевод строки
    text.replace(QChar(0x2029), '\n'); // разделитель абзацев → перевод строки
    text = text.trimmed();

    // Страница и глава начала выделения
    int startPage = 0;
    const qreal ph = doc->pageSize().height();
    if (ph > 0)
    {
        QTextCursor c(doc);
        c.setPosition(selStart);
        const QRectF r = doc->documentLayout()->blockBoundingRect(c.block());
        startPage = int(r.top() / ph);
    }
    const int ch = chapterForPage(startPage);

    DbQuote q;
    q.bookId = currentBookId;
    q.page = startPage;
    q.chapterIndex = ch;
    q.chapterTitle = (ch >= 0 && ch < chapterTitles.size()) ? chapterTitles[ch] : QString();
    q.startPos = selStart;
    q.endPos = selEnd;
    q.text = text;
    q.color = hexColor;

    Database::instance().addQuote(q, nullptr);

    mergeHighlight(selStart, selEnd, QColor(hexColor));

    // Сбросить выделение
    selAnchor = selStart = selEnd = -1;
    leftPageLbl->setSelection(-1, -1);
    rightPageLbl->setSelection(-1, -1);
    hideColorPopup();
    leftPageLbl->update();
    rightPageLbl->update();

    if (leftPanel->isVisible() && panelMode == PanelMode::Quotes)
        rebuildPanelContent();
}

void ReaderWindow::onPageContextMenu(PageView* src, const QPoint& localPos, const QPoint& globalPos)
{
    if (!currentBookId)
        return;

    const int pos = src->docPosAt(localPos);
    if (pos < 0)
        return;

    // Ищем цитату, попавшую под курсор
    const auto quotes = Database::instance().listQuotes(currentBookId, nullptr);
    DbQuote target;
    bool found = false;
    for (const auto& q : quotes)
    {
        if (pos >= q.startPos && pos < q.endPos)
        {
            target = q;
            found = true;
            break;
        }
    }
    if (!found)
        return;

    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu { background:#1c1610; border:1px solid #2a241e; border-radius:10px; padding:6px; color:#e0d8cd; font-size:13px; }
        QMenu::item { padding:8px 18px; border-radius:6px; }
        QMenu::item:selected { background:#2a241e; color:#ff8c00; }
    )");

    QMenu* colorMenu = menu.addMenu("Изменить цвет");
    colorMenu->setStyleSheet(menu.styleSheet());
    for (const QString& c : kQuoteColors)
    {
        // Кружок цвета вместо квадрата
        QPixmap pm(16, 16);
        pm.fill(Qt::transparent);
        QPainter pmp(&pm);
        pmp.setRenderHint(QPainter::Antialiasing);
        pmp.setPen(Qt::NoPen);
        pmp.setBrush(QColor(c));
        pmp.drawEllipse(0, 0, 16, 16);
        pmp.end();
        QAction* act = colorMenu->addAction(QIcon(pm), QString());
        connect(act, &QAction::triggered, this, [this, target, c]() {
            // Перекрашиваем: удаляем старую запись и создаём с новым цветом.
            Database::instance().removeQuote(target.id, nullptr);
            DbQuote nq = target;
            nq.color = c;
            Database::instance().addQuote(nq, nullptr);
            mergeHighlight(target.startPos, target.endPos, QColor(c));
            leftPageLbl->update();
            rightPageLbl->update();
            if (leftPanel->isVisible() && panelMode == PanelMode::Quotes)
                rebuildPanelContent();
            });
    }

    menu.addSeparator();
    QAction* del = menu.addAction("Удалить цитату");
    connect(del, &QAction::triggered, this, [this, target]() {
        Database::instance().removeQuote(target.id, nullptr);
        QTextCursor cur(doc);
        cur.setPosition(target.startPos);
        cur.setPosition(target.endPos, QTextCursor::KeepAnchor);
        QTextCharFormat fmt;
        fmt.setBackground(Qt::transparent);
        fmt.setForeground(QColor("#d7d0c8"));
        cur.mergeCharFormat(fmt);
        leftPageLbl->update();
        rightPageLbl->update();
        if (leftPanel->isVisible() && panelMode == PanelMode::Quotes)
            rebuildPanelContent();
        });

    menu.exec(globalPos);
}

void ReaderWindow::hidePopups()
{
    hideColorPopup();
    leftPanel->hide();
    settingsPanel->hide();
    panelMode = PanelMode::None;
    updateTabHighlight();
    selAnchor = selStart = selEnd = -1;
    leftPageLbl->setSelection(-1, -1);
    rightPageLbl->setSelection(-1, -1);
}

// ============================================================

void ReaderWindow::closeEvent(QCloseEvent* event)
{
    if (currentBookId > 0)
    {
        Database::instance().updateLastPage(currentBookId, currentPage, nullptr);
        Database::instance().updateLastChapter(currentBookId, chapterForPage(currentPage), nullptr);
        emit readingFinished(currentBookId);
        emit readingProgressChanged(currentBookId);
    }

    QMainWindow::closeEvent(event);
}
