#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;
class QStackedWidget;

// Страница «Аккаунт»: в гостевом режиме показывает форму входа/регистрации,
// после авторизации — профиль пользователя с кнопкой выхода.
class AccountPage : public QWidget
{
    Q_OBJECT

public:
    explicit AccountPage(QWidget* parent = nullptr);

    // Перестраивает вид в зависимости от состояния авторизации.
    void refresh();

signals:
    void loggedIn();   // пользователь вошёл/зарегистрировался
    void loggedOut();  // пользователь вышел в гостевой режим

private:
    void buildAuthForm();
    void buildProfile();

    void doLogin();
    void doRegister();
    void showError(const QString& text);
    void clearForm();

    QStackedWidget* stack = nullptr;

    // Форма авторизации
    QLineEdit* loginEdit = nullptr;
    QLineEdit* passEdit = nullptr;
    QLineEdit* confirmEdit = nullptr;
    QLabel* errorLbl = nullptr;

    // Профиль
    QLabel* profileAvatar = nullptr;
    QLabel* profileLogin = nullptr;
};
