/********************************************************************************
** Form generated from reading UI file 'bookproject.ui'
**
** Created by: Qt User Interface Compiler version 6.11.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_BOOKPROJECT_H
#define UI_BOOKPROJECT_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_bookprojectClass
{
public:
    QMenuBar *menuBar;
    QToolBar *mainToolBar;
    QWidget *centralWidget;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *bookprojectClass)
    {
        if (bookprojectClass->objectName().isEmpty())
            bookprojectClass->setObjectName("bookprojectClass");
        bookprojectClass->resize(600, 400);
        menuBar = new QMenuBar(bookprojectClass);
        menuBar->setObjectName("menuBar");
        bookprojectClass->setMenuBar(menuBar);
        mainToolBar = new QToolBar(bookprojectClass);
        mainToolBar->setObjectName("mainToolBar");
        bookprojectClass->addToolBar(mainToolBar);
        centralWidget = new QWidget(bookprojectClass);
        centralWidget->setObjectName("centralWidget");
        bookprojectClass->setCentralWidget(centralWidget);
        statusBar = new QStatusBar(bookprojectClass);
        statusBar->setObjectName("statusBar");
        bookprojectClass->setStatusBar(statusBar);

        retranslateUi(bookprojectClass);

        QMetaObject::connectSlotsByName(bookprojectClass);
    } // setupUi

    void retranslateUi(QMainWindow *bookprojectClass)
    {
        bookprojectClass->setWindowTitle(QCoreApplication::translate("bookprojectClass", "bookproject", nullptr));
    } // retranslateUi

};

namespace Ui {
    class bookprojectClass: public Ui_bookprojectClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_BOOKPROJECT_H
