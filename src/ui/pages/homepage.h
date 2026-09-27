#pragma once

#include <QWidget>
#include <QVector>

class QStackedWidget;
class QLabel;
struct DbBook;

class HomePage : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget* parent = nullptr);
    void reloadBooks();

signals:
    void bookSelected(int bookId);
    void openReaderRequested(int bookId);
    void booksChanged(); // состав/состояние книг изменилось (для синхронизации других страниц)

private:
    QStackedWidget* booksStack;
    QLabel* tabRecentRead;
    QLabel* tabRecentAdded;

    // Баннер: последняя читаемая книга
    QLabel* bannerTitle = nullptr;
    QLabel* bannerAuthor = nullptr;
    QLabel* bannerAnnotation = nullptr;
    int bannerBookId = 0;

    //void setupTabs();
    QWidget* createBooksPage(const QVector<DbBook>& books, int skip = 0);

    // Действия над карточкой книги (как в библиотеке)
    void doToggleFavorite(int bookId, bool currentlyFavorite);
    void doToggleRead(int bookId, bool currentlyRead);
    void doDelete(int bookId, const QString& title);
    bool confirmDelete(const QString& title);
};