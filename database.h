#pragma once

#include <QString>
#include <QByteArray>
#include <QVector>

struct sqlite3;
struct Fb2Meta;

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
};

struct DbChapter
{
    int id = 0;
    int bookId = 0;
    int index = 0;
    QString title;
    int startOffset = 0;
};

struct ReaderSettings
{
    QString fontFamily = "Noto Serif";
    int fontSize = 18;
    int align = 0;
    double lineHeight = 1.4;
};

class Database
{
public:
    static Database& instance();

    bool openOrCreate(const QString& dbPath = "bookproject.db");

    // Добавить/обновить книгу из метаданных (после парсинга fb2metaparser)
    int addOrUpdateBookFromMeta(const Fb2Meta& meta, QString* errorOut = nullptr);

    QVector<DbBook> listBooks(QString* errorOut = nullptr) const;
    QVector<DbBook> listRecentlyAdded(int limit = 0, QString* errorOut = nullptr) const;
    QVector<DbBook> listRecentlyRead(int limit = 0, QString* errorOut = nullptr) const;
    bool getBook(int bookId, DbBook* out, QString* errorOut = nullptr) const;
    QVector<DbChapter> listChapters(int bookId, QString* errorOut = nullptr) const;

    QByteArray getBookImage(int bookId, const QString& imageId, QString* errorOut = nullptr) const;
    QByteArray getBookFileData(int bookId, QString* errorOut = nullptr) const;
    QByteArray getBookCover(int bookId, QString* errorOut = nullptr) const;
    bool ensureTextCache(int bookId, QString* errorOut = nullptr);
    bool getParsedText(int bookId, QString* out, QString* errorOut = nullptr);

    bool getSettings(int bookId, ReaderSettings* out, QString* errorOut = nullptr) const;
    bool upsertSettings(int bookId, const ReaderSettings& s, QString* errorOut = nullptr);

    bool updateLastPage(int bookId, int lastPage, QString* errorOut = nullptr);
    bool updateTotalPages(int bookId, int totalPages, QString* errorOut = nullptr);

private:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool ensureSchema(QString* errorOut);
    bool exec(const QString& sql, QString* errorOut) const;

    sqlite3* db = nullptr;
};
