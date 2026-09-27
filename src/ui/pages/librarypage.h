#pragma once

#include <QWidget>
#include <QVector>
#include <QString>

#include "data/database.h"

class QStackedWidget;
class QPushButton;
class QLabel;

class LibraryPage : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryPage(bool favoritesOnly = false, QWidget* parent = nullptr);
    void reloadBooks();

signals:
    void bookSelected(int bookId);
    void booksChanged(); // изменился состав/состояние книг (для обновления других страниц)

private:
    QStackedWidget* viewStack = nullptr;
    QLabel* subtitleLbl = nullptr;

    int sortMode = 0;     // 0 — по дате, 1 — по автору, 2 — по названию, 3 — по прогрессу
    int currentView = 0;  // 0 — сетка, 1 — список
    QString searchText;
    bool favoritesOnly = false; // режим страницы "Избранное"

    QVector<DbBook> allBooks;

    QWidget* createGridView(const QVector<DbBook>& books);
    QWidget* createListView(const QVector<DbBook>& books);

    void rebuildViews();
    QVector<DbBook> filteredSorted() const;
    void updateSubtitle();

    void doToggleFavorite(int bookId, bool currentlyFavorite);
    void doToggleRead(int bookId, bool currentlyRead);
    void doDelete(int bookId, const QString& title);
    bool confirmDelete(const QString& title);
};
