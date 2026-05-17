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

private:
    QStackedWidget* booksStack;
    QLabel* tabRecentRead;
    QLabel* tabRecentAdded;

    //void setupTabs();
    QWidget* createBooksPage(const QVector<DbBook>& books);
};