#pragma once

#include <QWidget>
#include <QVector>

class QLabel;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
class QFrame;
class QProgressBar;

class BookInfoPage : public QWidget
{
    Q_OBJECT

public:
    explicit BookInfoPage(QWidget* parent = nullptr);

    void setBookId(int bookId);
    int bookId() const { return currentBookId; }
    void updateProgress();

signals:
    void openReaderRequested(int bookId, int startChapterIndex);
    void openReaderAtPageRequested(int bookId, int page);

private slots:
    void onContinueButtonClicked();
    void onFinishButtonClicked();

private:
    void fullLoad();           // Полная загрузка (при открытии)
    void loadBookInfo();       // Загрузка информации о книге (один раз)
    void loadChapters();       // Загрузка глав (один раз)
    void updateButtonsState(); // Обновление состояния кнопок
    void setActiveTab(int tabIndex);
    void applyChapterHighlight(); // Подсветка главы, на которой остановились
    void refreshSidePanels();     // Список закладок и цитат справа
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    int currentBookId = 0;
    int totalPages = 0;
    int currentPage = 0;
    bool chaptersLoaded = false;

    // Top elements
    QLabel* tabBookmarks = nullptr; // вкладка "закладки"
    QLabel* tabNotes = nullptr;     // вкладка "цитаты"
    QStackedWidget* tabStack = nullptr; // правый блок: закладки / цитаты

    // Header elements
    QLabel* coverLbl = nullptr;
    QLabel* titleLbl = nullptr;
    QLabel* authorLbl = nullptr;
    QLabel* statsLbl = nullptr;

    // Buttons
    QPushButton* continueBtn = nullptr;
    QPushButton* finishBtn = nullptr;

    // Progress elements
    QLabel* progressLabel = nullptr;
    QLabel* pagesReadLabel = nullptr;
    QProgressBar* progressBar = nullptr;

    // Contents
    QVBoxLayout* contentsListLayout = nullptr;
    QWidget* placeholdersBookmarks = nullptr;
    QWidget* placeholdersNotes = nullptr;

    // Списки глав/закладок/цитат
    QVector<QWidget*> chapterRows; // строки содержания (для подсветки)
    int chapterCount = 0;
    QVBoxLayout* bookmarksLayout = nullptr; // содержимое вкладки "закладки"
    QVBoxLayout* quotesLayout = nullptr;    // содержимое вкладки "цитаты"
};