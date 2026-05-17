#pragma once

#include <QMainWindow>
#include <QVector>

#include "database.h"

class QLabel;
class QPushButton;
class QFrame;
class QScrollArea;
class QWheelEvent;
class QMouseEvent;
class QResizeEvent;

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
    void updateUi();
    void goToPage(int pageIndex);
    void closeEvent(QCloseEvent* event) override;

    QString pageText(int pageIndex) const;

    int pageAreaHeight() const;
    int pageAreaWidth() const;

private:
    int currentBookId = 0;
    int currentPage = 0; // zero-based "spread" index (2 pages per spread)
    QVector<QString> pages; // single pages, we show 2 at a time

    ReaderSettings settings;

    QWidget* central = nullptr;
    QPushButton* menuBtn = nullptr;
    QPushButton* closeBtn = nullptr;
    QPushButton* minBtn = nullptr;
    QPushButton* maxBtn = nullptr;

    QLabel* leftPageLbl = nullptr;
    QLabel* rightPageLbl = nullptr;

    QFrame* leftNav = nullptr;
    QFrame* rightNav = nullptr;
    QLabel* progressLbl = nullptr;
    QFrame* progressBg = nullptr;
    QFrame* progressFill = nullptr;

    QWidget* overlayMenu = nullptr; // reader preferences overlay (simplified)
};