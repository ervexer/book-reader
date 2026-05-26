#pragma once

#include <QString>
#include <QVector>
#include <QByteArray>

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
    QString content;
};

struct ReaderSettings
{
    QString fontFamily = "Times New Roman";
    int fontSize = 22;
    int align = 0;
    double lineHeight = 1.4;
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

    int addOrUpdateBookFromMeta(
        const Fb2Meta& meta,
        QString* errorOut = nullptr);

    QVector<DbBook> listBooks(
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

    bool updateTotalPages(
        int bookId,
        int totalPages,
        QString* errorOut = nullptr);

    bool getChapterText(int bookId, int chapterIndex, QString* out, QString* errorOut = nullptr) const;

private:
    Database() = default;

    bool exec(
        const QString& sql,
        QString* errorOut = nullptr) const;

private:
    sqlite3* db = nullptr;
};