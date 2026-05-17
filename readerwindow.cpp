#include "readerwindow.h"
#include "database.h"
#include "bookinfopage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QIcon>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSize>
#include <QApplication>
#include <QScrollArea>
#include <QFontMetrics>
#include <QCloseEvent>

// Создаёт лейбл для страницы текста
static QLabel* makePageLabel()
{
    auto* l = new QLabel;
    l->setWordWrap(true);
    l->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    l->setStyleSheet("color:#d7d0c8; font-family:'Noto Serif';");
    l->setTextFormat(Qt::PlainText);

    // Критично: запрещаем лейблу расширяться вертикально
    // иначе он выталкивает нижнее меню
    l->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    return l;
}

// Эвристика: сколько символов помещается на одну страницу
static int charsPerPageFor(const ReaderSettings& s, int pageHeightPx, int pageWidthPx)
{
    // Считаем реально по размеру шрифта и размеру области
    QFont font(s.fontFamily);
    font.setPointSize(s.fontSize);
    QFontMetrics fm(font);

    // Высота одной строки
    const int lineH = fm.lineSpacing();
    if (lineH <= 0) return 1200;

    // Сколько строк помещается по высоте (с учётом межстрочного интервала)
    const int linesPerPage = qMax(1, int(pageHeightPx / (lineH * s.lineHeight)));

    // Примерно сколько символов в строке
    const int charsPerLine = qMax(10, pageWidthPx / qMax(1, fm.averageCharWidth()));

    return linesPerPage * charsPerLine;
}

ReaderWindow::ReaderWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Reader");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setStyleSheet("background:#0d0b08;");
    setMouseTracking(true);
    //showMaximized();

    central = new QWidget;
    central->setMouseTracking(true);
    setCentralWidget(central);

    // Используем абсолютное позиционирование для фиксации меню
    // central не имеет layout — всё позиционируем вручную в resizeEvent

    // ===== ВЕРХНЯЯ ПАНЕЛЬ =====
    auto* topBar = new QFrame(central);
    topBar->setObjectName("topBar");
    topBar->setStyleSheet("background:#0d0b08;");
    topBar->setFixedHeight(50);

    auto* top = new QHBoxLayout(topBar);
    top->setContentsMargins(12, 8, 12, 8);
    top->setSpacing(8);

    menuBtn = new QPushButton(topBar);
    menuBtn->setFixedSize(34, 34);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setStyleSheet("background:transparent;");
    menuBtn->setIcon(QIcon("image icon/reader_menu.png"));
    menuBtn->setIconSize(QSize(18, 18));
    top->addWidget(menuBtn);
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
        b->setStyleSheet("background:transparent;");
        top->addWidget(b);
    }

    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    connect(minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(maxBtn, &QPushButton::clicked, this, [this]() {
        isMaximized() ? showNormal() : showMaximized();
        });

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

    // ===== ОБЛАСТЬ ТЕКСТА (между верхней и нижней панелью) =====
    // Два лейбла для левой и правой страниц
    leftPageLbl = makePageLabel();
    leftPageLbl->setParent(central);

    rightPageLbl = makePageLabel();
    rightPageLbl->setParent(central);

    // ===== НАВИГАЦИЯ (стрелки по бокам) =====
    leftNav = new QFrame(central);
    rightNav = new QFrame(central);
    leftNav->setStyleSheet("background:rgba(255,255,255,0.03); border-radius:10px;");
    rightNav->setStyleSheet("background:rgba(255,255,255,0.03); border-radius:10px;");
    leftNav->hide();
    rightNav->hide();
    leftNav->setMouseTracking(true);
    rightNav->setMouseTracking(true);

    auto* leftArrow = new QLabel(leftNav);
    leftArrow->setPixmap(QIcon("image icon/arrow_left.png").pixmap(22, 22));
    leftArrow->setAlignment(Qt::AlignCenter);

    auto* rightArrow = new QLabel(rightNav);
    rightArrow->setPixmap(QIcon("image icon/arrow_right.png").pixmap(22, 22));
    rightArrow->setAlignment(Qt::AlignCenter);

    leftNav->setCursor(Qt::PointingHandCursor);
    rightNav->setCursor(Qt::PointingHandCursor);
    leftNav->installEventFilter(this);
    rightNav->installEventFilter(this);
    leftNav->setProperty("navDir", -1);
    rightNav->setProperty("navDir", +1);

    // ===== МЕНЮ НАСТРОЕК =====
    overlayMenu = new QFrame(central);
    overlayMenu->setStyleSheet("background:#1c1610; border-radius:16px;");
    overlayMenu->hide();
    overlayMenu->setFixedSize(420, 300);
    overlayMenu->raise();

    auto* omRoot = new QVBoxLayout(overlayMenu);
    omRoot->setContentsMargins(20, 18, 20, 18);
    omRoot->setSpacing(14);

    auto* omTitle = new QLabel("Настройки чтения");
    omTitle->setStyleSheet("font-size:14px; color:#d2c7bb;");
    omRoot->addWidget(omTitle);

    auto* omHint = new QLabel("Шрифт, выравнивание, размер текста, интервал.");
    omHint->setStyleSheet("color:#8c847a; font-size:12px;");
    omHint->setWordWrap(true);
    omRoot->addWidget(omHint);

    auto* closePref = new QPushButton("Применить и закрыть");
    closePref->setCursor(Qt::PointingHandCursor);
    closePref->setFixedHeight(44);
    closePref->setStyleSheet(
        "QPushButton{background:#ff8c00;border:none;border-radius:12px;color:black;}"
        "QPushButton:hover{background:#ffa733;}"
    );
    connect(closePref, &QPushButton::clicked, this, [this]() {
        overlayMenu->hide();
        if (currentBookId)
            Database::instance().upsertSettings(currentBookId, settings, nullptr);
        rebuildPages();
        updateUi();
        });
    omRoot->addStretch();
    omRoot->addWidget(closePref);

    connect(menuBtn, &QPushButton::clicked, this, [this]() {
        overlayMenu->setVisible(!overlayMenu->isVisible());
        overlayMenu->raise();
        updateUi();
        });
}

void ReaderWindow::openBook(int bookId, int chapterIndex)
{
    currentBookId = bookId;
    Database::instance().openOrCreate();
    Database::instance().getSettings(bookId, &settings, nullptr);

    rebuildPages();

    DbBook book;
    Database::instance().getBook(bookId, &book, nullptr);

    if (chapterIndex < 0)
    {
        goToPage(qMax(0, book.lastPage));
    }
    else
    {
        const auto chapters = Database::instance().listChapters(bookId, nullptr);
        int startOffset = 0;
        for (const auto& ch : chapters)
        {
            if (ch.index == chapterIndex)
            {
                startOffset = ch.startOffset;
                break;
            }
        }

        const int cpp = charsPerPageFor(settings, pageAreaHeight(), pageAreaWidth() / 2);
        int page = cpp > 0 ? (startOffset / cpp) : 0;
        if (page % 2 == 1) page--;
        goToPage(page);
    }

    showMaximized();
    raise();
    activateWindow();
}

// Высота области текста (между верхней и нижней панелями)
int ReaderWindow::pageAreaHeight() const
{
    return qMax(100, height() - 50 - 52 - 20); // 50=topBar, 52=bottomBar, 20=отступы
}

int ReaderWindow::pageAreaWidth() const
{
    return qMax(200, width() - 80); // 80 = отступы по 40 с каждой стороны
}

void ReaderWindow::rebuildPages()
{
    pages.clear();

    QString text;
    if (!Database::instance().getParsedText(currentBookId, &text, nullptr))
        text = "(Не удалось загрузить текст книги)";

    const int cpp = charsPerPageFor(settings, pageAreaHeight(), pageAreaWidth() / 2);
    for (int i = 0; i < text.size(); i += cpp)
        pages.push_back(text.mid(i, cpp));

    if (pages.isEmpty())
        pages.push_back("(Пусто)");

    Database::instance().updateTotalPages(currentBookId, pages.size(), nullptr);
}

QString ReaderWindow::pageText(int pageIndex) const
{
    if (pageIndex < 0 || pageIndex >= pages.size())
        return "";
    return pages[pageIndex];
}

void ReaderWindow::goToPage(int pageIndex)
{
    const int maxPage = qMax(0, pages.size() - 1);
    currentPage = qBound(0, pageIndex, maxPage);

    updateUi();
}

void ReaderWindow::updateUi()
{
    QFont f(settings.fontFamily);
    f.setPointSize(settings.fontSize);

    leftPageLbl->setFont(f);
    rightPageLbl->setFont(f);
    leftPageLbl->setText(pageText(currentPage));
    rightPageLbl->setText(pageText(currentPage + 1));

    // Прогресс
    const int total = pages.size();
    const int shown = qMin(currentPage + 2, total);
    progressLbl->setText(QString("%1 / %2").arg(shown).arg(total));

    const int w = progressBg->width();
    const double ratio = total > 0 ? (double)shown / (double)total : 0.0;
    progressFill->setGeometry(0, 0, int(w * ratio), progressBg->height());

    // Позиция меню настроек
    overlayMenu->move(width() - overlayMenu->width() - 24, 60);
}

void ReaderWindow::resizeEvent(QResizeEvent* e)
{
    QMainWindow::resizeEvent(e);

    const int W = width();
    const int H = height();
    const int topH = 50;
    const int bottomH = 52;
    const int padH = 10; // отступ сверху/снизу от текста
    const int padSide = 40;
    const int navW = 56;

    // Верхняя панель — прибита к верху
    findChild<QFrame*>("topBar")->setGeometry(0, 0, W, topH);

    // Нижняя панель — прибита к низу
    findChild<QFrame*>("bottomBar")->setGeometry(0, H - bottomH, W, bottomH);

    // Область текста между панелями
    const int textY = topH + padH;
    const int textH = H - topH - bottomH - padH * 2;
    const int textW = (W - padSide * 2 - 40) / 2; // 40 = зазор между страницами

    leftPageLbl->setGeometry(padSide, textY, textW, textH);
    rightPageLbl->setGeometry(padSide + textW + 40, textY, textW, textH);

    // Навигационные стрелки
    const int navH = H - 180;
    const int navY = 80;
    leftNav->setGeometry(10, navY, navW, navH);
    rightNav->setGeometry(W - navW - 10, navY, navW, navH);

    // Центрируем иконки стрелок
    if (auto* arr = leftNav->findChild<QLabel*>())
        arr->setGeometry(0, navH / 2 - 20, navW, 40);
    if (auto* arr = rightNav->findChild<QLabel*>())
        arr->setGeometry(0, navH / 2 - 20, navW, 40);

    updateUi();
    rebuildPages(); // пересчитываем страницы при изменении размера окна
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

void ReaderWindow::mouseMoveEvent(QMouseEvent* e)
{
    const int margin = 70;
    leftNav->setVisible(e->pos().x() < margin);
    rightNav->setVisible(e->pos().x() > (width() - margin));
    QMainWindow::mouseMoveEvent(e);
}

bool ReaderWindow::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == leftNav || watched == rightNav)
        && event->type() == QEvent::MouseButtonPress)
    {
        const int dir = watched->property("navDir").toInt();
        goToPage(currentPage + dir * 2);
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

//при закрытии обновляется прогресс чтения
void ReaderWindow::closeEvent(QCloseEvent* event)
{
    if (currentBookId > 0)
    {
        Database::instance().updateLastPage(currentBookId, currentPage, nullptr);
        emit readingFinished(currentBookId);
        emit readingProgressChanged(currentBookId); 
    }

    QMainWindow::closeEvent(event);
}