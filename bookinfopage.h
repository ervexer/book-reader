#pragma once

#include <QWidget>

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
    void backRequested();
    void openReaderRequested(int bookId, int startChapterIndex);

private slots:
    void onContinueButtonClicked();
    void onFinishButtonClicked();

private:
    void fullLoad();           // Полная загрузка (при открытии)
    void loadBookInfo();       // Загрузка информации о книге (один раз)
    void loadChapters();       // Загрузка глав (один раз)
    void updateButtonsState(); // Обновление состояния кнопок
    void setActiveTab(int tabIndex);
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    int currentBookId = 0;
    int totalPages = 0;
    int currentPage = 0;
    bool chaptersLoaded = false;

    // Top elements
    QPushButton* backBtn = nullptr;
    QLabel* tabContents = nullptr;
    QLabel* tabBookmarks = nullptr;
    QLabel* tabNotes = nullptr;
    QFrame* tabUnderline = nullptr;
    QStackedWidget* tabStack = nullptr;

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
    QWidget* contentsPage = nullptr;
    QVBoxLayout* contentsListLayout = nullptr;
    QWidget* placeholdersBookmarks = nullptr;
    QWidget* placeholdersNotes = nullptr;
};