#pragma once

#include <QWidget>

class QStackedWidget;
class QPushButton;

class LibraryPage : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryPage(QWidget* parent = nullptr);
    void reloadBooks();

signals:
    void bookSelected(int bookId);

private:
    QStackedWidget* viewStack;

    QWidget* createGridView();
    QWidget* createListView();
};