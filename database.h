#pragma once

#include <QString>
#include <QVector>
#include <QByteArray>

#include "fb2parser.h"

struct sqlite3;

struct DbUser
{
    int id = 0;
    QString login;
};

struct DbBook
{
    int id = 0;

    QString title;
    QString author;
    QString translator;
    QString annotation;
    QString genre;

    int totalPages = 0;
    int lastPage = 0;
    int lastChapter = 0;
    bool favorite = false;
};

struct DbChapter
{
    int id = 0;
    int bookId = 0;
    int index = 0;

    QString title;
    QString content;
};

struct ReaderSettings
{
    QString fontFamily = "Times New Roman";
    int fontSize = 22;
    int align = 0;
    double lineHeight = 1.4;
};

struct DbBookmark
{
    int id = 0;
    int bookId = 0;
    int page = 0;
    int chapterIndex = 0;
    QString chapterTitle;
};

struct DbQuote
{
    int id = 0;
    int bookId = 0;
    int page = 0;
    int chapterIndex = 0;
    int startPos = 0;   // позиция начала выделения в общем QTextDocument
    int endPos = 0;     // позиция конца выделения
    QString text;
    QString color;      // hex-цвет подсветки (#rrggbb)
    QString chapterTitle;
};

class Database
{
public:
    static Database& instance();

    ~Database();

    bool openOrCreate(
        const QString& dbPath = "library.db");

    bool ensureSchema(
        QString* errorOut = nullptr);

    // ===== Пользователи / авторизация =====
    // Регистрирует нового пользователя. Возвращает false, если логин занят.
    bool registerUser(
        const QString& login,
        const QString& password,
        int* outUserId = nullptr,
        QString* errorOut = nullptr);

    // Проверяет логин/пароль. Возвращает false при неверных данных.
    bool authenticateUser(
        const QString& login,
        const QString& password,
        int* outUserId = nullptr,
        QString* errorOut = nullptr);

    // Текущий вошедший пользователь (0 = гостевой режим).
    void setCurrentUser(int userId, const QString& login);
    void logoutUser();
    int currentUserId() const { return curUserId; }
    QString currentUserLogin() const { return curUserLogin; }
    bool isLoggedIn() const { return curUserId != 0; }

    // Привязывает все гостевые книги (user_id=0) к указанному пользователю.
    bool attachGuestBooksToUser(int userId, QString* errorOut = nullptr);

    int addBook(
        const ParsedBook& book,
        QString* errorOut = nullptr);

    QVector<DbBook> listBooks(
        QString* errorOut = nullptr) const;

    QVector<DbBook> listRecentlyAdded(
        int limit = 0,
        QString* errorOut = nullptr) const;

    QVector<DbBook> listRecentlyRead(
        int limit = 0,
        QString* errorOut = nullptr) const;

    QByteArray getBookCover(
        int bookId,
        QString* errorOut = nullptr) const;

    QVector<DbChapter> listChapters(
        int bookId,
        QString* errorOut = nullptr) const;

    bool getBook(
        int bookId,
        DbBook* out,
        QString* errorOut = nullptr) const;

    bool getSettings(
        int bookId,
        ReaderSettings* out,
        QString* errorOut = nullptr) const;

    bool upsertSettings(
        int bookId,
        const ReaderSettings& s,
        QString* errorOut = nullptr);

    bool updateLastPage(
        int bookId,
        int lastPage,
        QString* errorOut = nullptr);

    bool updateLastChapter(
        int bookId,
        int chapterIndex,
        QString* errorOut = nullptr);

    bool updateTotalPages(
        int bookId,
        int totalPages,
        QString* errorOut = nullptr);

    bool getChapterText(int bookId, int chapterIndex, QString* out, QString* errorOut = nullptr) const;

    bool setFavorite(int bookId, bool favorite, QString* errorOut = nullptr);
    bool deleteBook(int bookId, QString* errorOut = nullptr);
    bool markAsRead(int bookId, QString* errorOut = nullptr);

    // Закладки
    int addBookmark(int bookId, int page, int chapterIndex,
        const QString& chapterTitle, QString* errorOut = nullptr);
    bool removeBookmark(int id, QString* errorOut = nullptr);
    bool removeBookmarkByPage(int bookId, int page, QString* errorOut = nullptr);
    bool hasBookmark(int bookId, int page) const;
    QVector<DbBookmark> listBookmarks(int bookId, QString* errorOut = nullptr) const;

    // Цитаты (выделения)
    int addQuote(const DbQuote& q, QString* errorOut = nullptr);
    bool removeQuote(int id, QString* errorOut = nullptr);
    QVector<DbQuote> listQuotes(int bookId, QString* errorOut = nullptr) const;

private:
    Database() = default;

    bool exec(
        const QString& sql,
        QString* errorOut = nullptr) const;

private:
    sqlite3* db = nullptr;

    int curUserId = 0;       // 0 = гостевой режим
    QString curUserLogin;
};