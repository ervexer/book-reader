#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QVector>
#include <QStringList>
#include <QColor>
#include <QRect>
#include <QPoint>

#include "data/database.h"

class QLabel;
class QPushButton;
class QFrame;
class QScrollArea;
class QVBoxLayout;
class QWheelEvent;
class QMouseEvent;
class QResizeEvent;
class QContextMenuEvent;
class QTextDocument;

class ReaderWindow;

// Рисует одну страницу общего QTextDocument (постраничная разметка Qt).
// Также обрабатывает выделение текста мышью и клики для перелистывания.
class PageView : public QWidget
{
public:
    explicit PageView(QWidget* parent = nullptr) : QWidget(parent) { setMouseTracking(true); }

    void setDocument(QTextDocument* doc) { m_doc = doc; update(); }
    void setPageIndex(int index) { m_pageIndex = index; update(); }
    void setReader(ReaderWindow* r) { m_reader = r; }
    void setSelection(int start, int end) { m_selStart = start; m_selEnd = end; update(); }

    int docPosAt(const QPoint& localPos) const;

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    QTextDocument* m_doc = nullptr;
    int m_pageIndex = 0;
    ReaderWindow* m_reader = nullptr;

    QPoint m_pressPos;
    bool m_pressed = false;
    int m_selStart = -1;
    int m_selEnd = -1;
};

class ReaderWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ReaderWindow(QWidget* parent = nullptr);

    // chapterIndex -1 = continue (last page). Если page >= 0 — открыть на этой
    // странице (приоритетнее chapterIndex; для перехода к закладке/цитате).
    void openBook(int bookId, int chapterIndex, int page = -1);

    // Вызывается из PageView при работе с мышью
    void onPagePress(PageView* src, const QPoint& localPos);
    void onPageMove(PageView* src, const QPoint& localPos);
    void onPageRelease(PageView* src, const QPoint& localPos, const QPoint& globalPos, bool isClick);
    void onPageContextMenu(PageView* src, const QPoint& localPos, const QPoint& globalPos);

protected:
    void wheelEvent(QWheelEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* event) override; // перетаскивание за панель

signals:
    void readingFinished(int bookId);
    void readingProgressChanged(int bookId);

private:
    enum class PanelMode { None, Help, Info, Contents, Bookmarks, Quotes };

    void rebuildPages();
    void applyFormatting();
    void updateUi();
    void goToPage(int pageIndex);
    void buildDocument(const QVector<DbChapter>& chapters);
    int pageForChapter(int chapterIndex) const;
    int pageForPos(int pos) const; // левая страница разворота для позиции в документе
    int chapterForPage(int page) const;
    void resolvePendingStart();
    void closeEvent(QCloseEvent* event) override;

    int pageAreaHeight() const;
    int pageAreaWidth() const;

    // Боковая панель (содержание / закладки / цитаты / инфо)
    void openLeftPanel(PanelMode mode);
    void closeLeftPanel();
    void rebuildPanelContent();
    void clearPanelContent();
    void buildHelpContent();
    void buildInfoContent();
    void buildContentsContent();
    void buildBookmarksContent();
    void buildQuotesContent();
    void updateTabHighlight();

    // Панель настроек
    void buildSettingsPanel();
    void toggleSettingsPanel();
    void applySettingsLive();
    void refreshSettingsControls();
    void resetSettings();

    // Закладки
    void toggleBookmark();
    void updateBookmarkButton();

    // Цитаты / выделение
    void applyQuoteHighlights();
    void mergeHighlight(int start, int end, const QColor& color);
    void showColorPopup(const QPoint& globalPos);
    void hideColorPopup();
    void saveQuoteWithColor(const QString& hexColor);

    void hidePopups();

private:
    int currentBookId = 0;
    int currentPage = 0;
    int pageCount = 0;

    QTextDocument* doc = nullptr;
    QVector<int> chapterStartPos;   // позиция начала каждой главы в общем документе
    QStringList chapterTitles;      // заголовки глав (для закладок/содержания)
    int requestedChapter = 0;       // глава/режим, запрошенный при открытии (-1 = продолжить)
    int requestedPage = -1;         // конкретная страница (>=0) — приоритетнее главы
    bool hasPendingStart = false;   // нужно ли ещё установить стартовую страницу

    bool winMaximized = true;     // состояние окна (развёрнуто/оконный режим)
    QRect normalGeom;             // геометрия оконного режима

    QWidget* topBarWidget = nullptr; // верхняя панель (для перетаскивания)
    bool dragging = false;
    QPoint dragPos;

    ReaderSettings settings;

    QWidget* central = nullptr;

    // Верхняя панель
    QPushButton* helpBtn = nullptr;
    QPushButton* menuBtn = nullptr;
    QPushButton* settingsBtn = nullptr;
    QPushButton* bookmarkBtn = nullptr;
    QPushButton* tabContents = nullptr;
    QPushButton* tabBookmarks = nullptr;
    QPushButton* tabQuotes = nullptr;
    QPushButton* closeBtn = nullptr;
    QPushButton* minBtn = nullptr;
    QPushButton* maxBtn = nullptr;

    PageView* leftPageLbl = nullptr;
    PageView* rightPageLbl = nullptr;
    QFrame* centerDivider = nullptr;
    QWidget* leftEdge = nullptr;   // клик-зона у левого края экрана (назад)
    QWidget* rightEdge = nullptr;  // клик-зона у правого края экрана (вперёд)

    QLabel* progressLbl = nullptr;
    QFrame* progressBg = nullptr;
    QFrame* progressFill = nullptr;

    // Левая боковая панель
    PanelMode panelMode = PanelMode::None;
    QFrame* leftPanel = nullptr;
    QLabel* panelTitle = nullptr;
    QScrollArea* panelScroll = nullptr;
    QWidget* panelContent = nullptr;
    QVBoxLayout* panelLayout = nullptr;

    // Панель настроек
    QFrame* settingsPanel = nullptr;
    QLabel* fontSizeValueLbl = nullptr;
    QLabel* lineHeightValueLbl = nullptr;
    QVector<QPushButton*> fontButtons;
    QVector<QPushButton*> alignButtons;

    // Выделение текста / цитаты
    int selAnchor = -1;
    int selStart = -1;
    int selEnd = -1;
    QFrame* colorPopup = nullptr;
};
