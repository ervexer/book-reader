#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QVector>

#include "database.h"

class QLabel;
class QPushButton;
class QFrame;
class QScrollArea;
class QWheelEvent;
class QMouseEvent;
class QResizeEvent;
class QTextDocument;

// Рисует одну страницу общего QTextDocument (постраничная разметка Qt).
class PageView : public QWidget
{
public:
    explicit PageView(QWidget* parent = nullptr) : QWidget(parent) {}

    void setDocument(QTextDocument* doc) { m_doc = doc; update(); }
    void setPageIndex(int index) { m_pageIndex = index; update(); }

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QTextDocument* m_doc = nullptr;
    int m_pageIndex = 0;
};

class ReaderWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ReaderWindow(QWidget* parent = nullptr);

    void openBook(int bookId, int chapterIndex); // chapterIndex -1 = continue (last page)

protected:
    void wheelEvent(QWheelEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
signals:
    void readingFinished(int bookId);
    void readingProgressChanged(int bookId);
private:
    void rebuildPages();
    void applyFormatting();
    void updateUi();
    void goToPage(int pageIndex);
    void buildDocument(const QVector<DbChapter>& chapters);
    int pageForChapter(int chapterIndex) const;
    void resolvePendingStart();
    void closeEvent(QCloseEvent* event) override;

    int pageAreaHeight() const;
    int pageAreaWidth() const;

private:
    int currentBookId = 0;
    int currentPage = 0; // zero-based single-page index (showing 2 at a time)
    int pageCount = 0;

    QTextDocument* doc = nullptr;
    QVector<int> chapterStartPos; // позиция начала каждой главы в общем документе
    int requestedChapter = 0;     // глава/режим, запрошенный при открытии (-1 = продолжить)
    bool hasPendingStart = false; // нужно ли ещё установить стартовую страницу

    ReaderSettings settings;

    QWidget* central = nullptr;
    QPushButton* menuBtn = nullptr;
    QPushButton* closeBtn = nullptr;
    QPushButton* minBtn = nullptr;
    QPushButton* maxBtn = nullptr;

    PageView* leftPageLbl = nullptr;
    PageView* rightPageLbl = nullptr;

    QFrame* leftNav = nullptr;
    QFrame* rightNav = nullptr;
    QLabel* progressLbl = nullptr;
    QFrame* progressBg = nullptr;
    QFrame* progressFill = nullptr;

    QWidget* overlayMenu = nullptr; // reader preferences overlay (simplified)
};