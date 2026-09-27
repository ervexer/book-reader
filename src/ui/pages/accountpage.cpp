#include "ui/pages/accountpage.h"
#include "data/database.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QFrame>

namespace {
const QString kEditStyle =
    "QLineEdit{background:#221c15;border:1px solid #3a2f24;border-radius:10px;"
    "padding:11px 14px;color:#e8ddd0;font-size:14px;}"
    "QLineEdit:focus{border:1px solid #ff8c00;}";
}

AccountPage::AccountPage(QWidget* parent) : QWidget(parent)
{
    setStyleSheet("background:#110E0A;");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stack = new QStackedWidget;
    root->addWidget(stack);

    buildAuthForm();   // index 0
    buildProfile();    // index 1

    refresh();
}

void AccountPage::buildAuthForm()
{
    auto* page = new QWidget;
    page->setStyleSheet("background:transparent;");

    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(20, 20, 20, 20);
    outer->addStretch();

    auto* card = new QFrame;
    card->setFixedWidth(400);
    card->setStyleSheet("QFrame{background:#17130E;border-radius:16px;}");

    auto* col = new QVBoxLayout(card);
    col->setContentsMargins(34, 30, 34, 30);
    col->setSpacing(13);

    auto* title = new QLabel("Вход в аккаунт");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color:#ff8c00;font-size:24px;font-weight:bold;background:transparent;");
    col->addWidget(title);

    auto* subtitle = new QLabel("Войдите или зарегистрируйтесь, чтобы сохранить свою библиотеку");
    subtitle->setWordWrap(true);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("color:#8c847a;font-size:13px;background:transparent;");
    col->addWidget(subtitle);
    col->addSpacing(8);

    loginEdit = new QLineEdit;
    loginEdit->setPlaceholderText("Логин");
    loginEdit->setStyleSheet(kEditStyle);
    col->addWidget(loginEdit);

    passEdit = new QLineEdit;
    passEdit->setPlaceholderText("Пароль");
    passEdit->setEchoMode(QLineEdit::Password);
    passEdit->setStyleSheet(kEditStyle);
    col->addWidget(passEdit);

    confirmEdit = new QLineEdit;
    confirmEdit->setPlaceholderText("Подтвердите пароль (для регистрации)");
    confirmEdit->setEchoMode(QLineEdit::Password);
    confirmEdit->setStyleSheet(kEditStyle);
    col->addWidget(confirmEdit);

    errorLbl = new QLabel;
    errorLbl->setWordWrap(true);
    errorLbl->setStyleSheet("color:#e0584f;font-size:12px;background:transparent;");
    errorLbl->hide();
    col->addWidget(errorLbl);

    col->addSpacing(4);

    auto* loginBtn = new QPushButton("Авторизация");
    loginBtn->setCursor(Qt::PointingHandCursor);
    loginBtn->setStyleSheet(
        "QPushButton{background:#ff8c00;color:white;border:none;border-radius:10px;"
        "padding:12px;font-size:15px;font-weight:bold;}"
        "QPushButton:hover{background:#ffa733;}");
    col->addWidget(loginBtn);

    auto* registerBtn = new QPushButton("Регистрация");
    registerBtn->setCursor(Qt::PointingHandCursor);
    registerBtn->setStyleSheet(
        "QPushButton{background:transparent;color:#ff8c00;border:1px solid #ff8c00;"
        "border-radius:10px;padding:12px;font-size:15px;font-weight:bold;}"
        "QPushButton:hover{background:#241a0e;}");
    col->addWidget(registerBtn);

    connect(loginBtn, &QPushButton::clicked, this, &AccountPage::doLogin);
    connect(registerBtn, &QPushButton::clicked, this, &AccountPage::doRegister);
    // Enter в любом поле = авторизация.
    connect(loginEdit, &QLineEdit::returnPressed, this, &AccountPage::doLogin);
    connect(passEdit, &QLineEdit::returnPressed, this, &AccountPage::doLogin);

    auto* row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(card);
    row->addStretch();
    outer->addLayout(row);

    outer->addStretch();

    stack->addWidget(page);
}

void AccountPage::buildProfile()
{
    auto* page = new QWidget;
    page->setStyleSheet("background:transparent;");

    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(20, 20, 20, 20);
    outer->addStretch();

    auto* card = new QFrame;
    card->setFixedWidth(420);
    card->setStyleSheet("QFrame{background:#17130E;border-radius:16px;}");

    auto* col = new QVBoxLayout(card);
    col->setContentsMargins(34, 34, 34, 34);
    col->setSpacing(14);

    profileAvatar = new QLabel;
    profileAvatar->setFixedSize(80, 80);
    profileAvatar->setAlignment(Qt::AlignCenter);
    profileAvatar->setStyleSheet(
        "background:#ff8c00;border-radius:40px;color:white;"
        "font-size:34px;font-weight:bold;");
    col->addWidget(profileAvatar, 0, Qt::AlignHCenter);

    auto* heading = new QLabel("Профиль");
    heading->setAlignment(Qt::AlignCenter);
    heading->setStyleSheet("color:#ff8c00;font-size:22px;font-weight:bold;background:transparent;");
    col->addWidget(heading);

    profileLogin = new QLabel;
    profileLogin->setAlignment(Qt::AlignCenter);
    profileLogin->setStyleSheet("color:#e8ddd0;font-size:17px;background:transparent;");
    col->addWidget(profileLogin);

    auto* status = new QLabel("Вы вошли в аккаунт. Ваша библиотека привязана к этой учётной записи.");
    status->setWordWrap(true);
    status->setAlignment(Qt::AlignCenter);
    status->setStyleSheet("color:#8c847a;font-size:13px;background:transparent;");
    col->addWidget(status);

    col->addSpacing(8);

    auto* logoutBtn = new QPushButton("Выйти из аккаунта");
    logoutBtn->setCursor(Qt::PointingHandCursor);
    logoutBtn->setStyleSheet(
        "QPushButton{background:transparent;color:#e0584f;border:1px solid #e0584f;"
        "border-radius:10px;padding:12px;font-size:15px;font-weight:bold;}"
        "QPushButton:hover{background:#2a1714;}");
    col->addWidget(logoutBtn);

    connect(logoutBtn, &QPushButton::clicked, this, [this]() {
        Database::instance().logoutUser();
        refresh();
        emit loggedOut();
    });

    auto* row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(card);
    row->addStretch();
    outer->addLayout(row);

    outer->addStretch();

    stack->addWidget(page);
}

void AccountPage::doLogin()
{
    const QString login = loginEdit->text().trimmed();
    const QString pass = passEdit->text();

    if (login.isEmpty() || pass.isEmpty())
    {
        showError("Введите логин и пароль");
        return;
    }

    int userId = 0;
    QString err;
    if (!Database::instance().authenticateUser(login, pass, &userId, &err))
    {
        showError(err.isEmpty() ? "Не удалось войти" : err);
        return;
    }

    Database::instance().setCurrentUser(userId, login);
    // Привязываем книги, добавленные в гостевом режиме, к этому аккаунту.
    Database::instance().attachGuestBooksToUser(userId, nullptr);

    clearForm();
    refresh();
    emit loggedIn();
}

void AccountPage::doRegister()
{
    const QString login = loginEdit->text().trimmed();
    const QString pass = passEdit->text();
    const QString confirm = confirmEdit->text();

    if (login.isEmpty() || pass.isEmpty())
    {
        showError("Введите логин и пароль");
        return;
    }
    if (pass != confirm)
    {
        showError("Пароли не совпадают");
        return;
    }

    int userId = 0;
    QString err;
    if (!Database::instance().registerUser(login, pass, &userId, &err))
    {
        showError(err.isEmpty() ? "Не удалось зарегистрироваться" : err);
        return;
    }

    Database::instance().setCurrentUser(userId, login);
    // Книги из гостевого режима автоматически привязываются к новому аккаунту.
    Database::instance().attachGuestBooksToUser(userId, nullptr);

    clearForm();
    refresh();
    emit loggedIn();
}

void AccountPage::showError(const QString& text)
{
    errorLbl->setText(text);
    errorLbl->setVisible(!text.isEmpty());
}

void AccountPage::clearForm()
{
    loginEdit->clear();
    passEdit->clear();
    confirmEdit->clear();
    errorLbl->hide();
}

void AccountPage::refresh()
{
    Database& db = Database::instance();

    if (db.isLoggedIn())
    {
        const QString login = db.currentUserLogin();
        profileLogin->setText(login);
        profileAvatar->setText(login.isEmpty() ? "?" : login.left(1).toUpper());
        stack->setCurrentIndex(1);
    }
    else
    {
        stack->setCurrentIndex(0);
    }
}
