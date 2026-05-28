#include "database.h"

#include "fb2metaparser.h"
#include "fb2textparser.h"

#include <SQLite/sqlite3.h>

#include <QDateTime>

static QString sqliteErr(sqlite3* db)
{
    const char* msg = db
        ? sqlite3_errmsg(db)
        : "no db";

    return QString::fromUtf8(msg);
}

static bool replaceChapters(sqlite3* db, int bookId, const QVector<ParsedChapter>& chapters, QString* errorOut)
{
    sqlite3_stmt* deleteStmt = nullptr;

    if (sqlite3_prepare_v2(
        db,
        "DELETE FROM chapters WHERE book_id=?;",
        -1,
        &deleteStmt,
        nullptr) != SQLITE_OK)
    {
        if (errorOut)
            *errorOut = sqliteErr(db);

        return false;
    }

    sqlite3_bind_int(deleteStmt, 1, bookId);

    if (sqlite3_step(deleteStmt) != SQLITE_DONE)
    {
        if (errorOut)
            *errorOut = sqliteErr(db);

        sqlite3_finalize(deleteStmt);
        return false;
    }

    sqlite3_finalize(deleteStmt);

    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "INSERT INTO chapters("
        "book_id,"
        "idx,"
        "title,"
        "text"
        ") VALUES(?,?,?,?);";

    if (sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &stmt,
        nullptr) != SQLITE_OK)
    {
        if (errorOut)
            *errorOut = sqliteErr(db);

        return false;
    }

    for (const auto& chapter : chapters)
    {
        QByteArray titleUtf8 =
            chapter.title.toUtf8();

        QByteArray textUtf8 =
            chapter.text.toUtf8();

        sqlite3_bind_int(stmt, 1, bookId);

        sqlite3_bind_int(stmt, 2, chapter.index);

        sqlite3_bind_text(
            stmt,
            3,
            titleUtf8.constData(),
            titleUtf8.size(),
            SQLITE_TRANSIENT);

        sqlite3_bind_text(
            stmt,
            4,
            textUtf8.constData(),
            textUtf8.size(),
            SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            if (errorOut)
                *errorOut = sqliteErr(db);

            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    sqlite3_finalize(stmt);

    return true;
}

Database::~Database()
{
    if (db)
    {
        sqlite3_close(db);
        db = nullptr;
    }
}

Database& Database::instance()
{
    static Database inst;
    return inst;
}

bool Database::openOrCreate(
    const QString& dbPath)
{
    if (db)
        return true;

    QByteArray pathUtf8 =
        dbPath.toUtf8();

    if (sqlite3_open(
        pathUtf8.constData(),
        &db) != SQLITE_OK)
    {
        sqlite3_close(db);
        db = nullptr;
        return false;
    }

    return ensureSchema();
}

bool Database::exec(
    const QString& sql,
    QString* errorOut) const
{
    char* msg = nullptr;

    QByteArray utf8 =
        sql.toUtf8();

    int rc = sqlite3_exec(
        db,
        utf8.constData(),
        nullptr,
        nullptr,
        &msg);

    if (rc != SQLITE_OK)
    {
        if (errorOut)
        {
            *errorOut =
                QString::fromUtf8(
                    msg ? msg : "sqlite error");
        }

        sqlite3_free(msg);
        return false;
    }

    return true;
}

bool Database::ensureSchema(
    QString* errorOut)
{
    QString schema = R"SQL(

CREATE TABLE IF NOT EXISTS books (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    title         TEXT NOT NULL,
    author        TEXT NOT NULL,
    translator    TEXT NOT NULL DEFAULT '',
    annotation    TEXT NOT NULL DEFAULT '',
    genre         TEXT NOT NULL DEFAULT '',
    total_pages   INTEGER NOT NULL DEFAULT 0,
    last_page     INTEGER NOT NULL DEFAULT 0,
    created_at    INTEGER NOT NULL,
    last_read_at  INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS chapters (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    book_id   INTEGER NOT NULL,
    idx       INTEGER NOT NULL,
    title     TEXT NOT NULL,
    text      TEXT NOT NULL,
    FOREIGN KEY(book_id)
        REFERENCES books(id)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS reader_settings (
    book_id      INTEGER PRIMARY KEY,
    font_family  TEXT NOT NULL,
    font_size    INTEGER NOT NULL,
    align        INTEGER NOT NULL,
    line_height  REAL NOT NULL,
    FOREIGN KEY(book_id)
        REFERENCES books(id)
        ON DELETE CASCADE
);

)SQL";

    return exec(schema, errorOut);
}

int Database::addOrUpdateBookFromMeta(
    const Fb2Meta& meta,
    QString* errorOut)
{
    if (!openOrCreate())
    {
        if (errorOut)
            *errorOut = "Не удалось открыть БД";

        return 0;
    }

    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "INSERT INTO books("
        "file_data,"
        "title,"
        "author,"
        "translator,"
        "annotation,"
        "genre,"
        "created_at"
        ") VALUES(?,?,?,?,?,?,?);";

    if (sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &stmt,
        nullptr) != SQLITE_OK)
    {
        if (errorOut)
            *errorOut = sqliteErr(db);

        return 0;
    }

    QByteArray titleUtf8 =
        meta.title.toUtf8();

    QByteArray authorUtf8 =
        meta.author.toUtf8();

    QByteArray translatorUtf8 =
        meta.translator.toUtf8();

    QByteArray annotationUtf8 =
        meta.annotation.toUtf8();

    QByteArray genreUtf8 =
        meta.genre.toUtf8();

    sqlite3_bind_blob(
        stmt,
        1,
        fileUtf8.constData(),
        fileUtf8.size(),
        SQLITE_TRANSIENT);

    sqlite3_bind_text(
        stmt,
        2,
        titleUtf8.constData(),
        titleUtf8.size(),
        SQLITE_TRANSIENT);

    sqlite3_bind_text(
        stmt,
        3,
        authorUtf8.constData(),
        authorUtf8.size(),
        SQLITE_TRANSIENT);

    sqlite3_bind_text(
        stmt,
        4,
        translatorUtf8.constData(),
        translatorUtf8.size(),
        SQLITE_TRANSIENT);

    sqlite3_bind_text(
        stmt,
        5,
        annotationUtf8.constData(),
        annotationUtf8.size(),
        SQLITE_TRANSIENT);

    sqlite3_bind_text(
        stmt,
        6,
        genreUtf8.constData(),
        genreUtf8.size(),
        SQLITE_TRANSIENT);

    sqlite3_bind_int64(
        stmt,
        7,
        QDateTime::currentSecsSinceEpoch());

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        if (errorOut)
            *errorOut = sqliteErr(db);

        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);

    int bookId =
        (int)sqlite3_last_insert_rowid(db);

    QString parseError;

    QVector<ParsedChapter> chapters =
        Fb2TextParser::parseFile(
            bookId,
            meta.rawFileData,
            &parseError);

    if (chapters.isEmpty())
    {
        if (errorOut)
            *errorOut = parseError;

        return 0;
    }

    if (!replaceChapters(
        db,
        bookId,
        chapters,
        errorOut))
    {
        return 0;
    }

    ReaderSettings settings;

    upsertSettings(
        bookId,
        settings,
        nullptr);

    return bookId;
}

QVector<DbBook> Database::listBooks(
    QString* errorOut) const
{
    QVector<DbBook> result;

    if (!db)
        return result;

    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "SELECT "
        "id,"
        "title,"
        "author,"
        "translator,"
        "annotation,"
        "genre,"
        "total_pages,"
        "last_page "
        "FROM books "
        "ORDER BY created_at DESC;";

    if (sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &stmt,
        nullptr) != SQLITE_OK)
    {
        if (errorOut)
            *errorOut = sqliteErr(db);

        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        DbBook book;

        book.id =
            sqlite3_column_int(stmt, 0);

        book.title =
            QString::fromUtf8(
                (const char*)sqlite3_column_text(stmt, 1));

        book.author =
            QString::fromUtf8(
                (const char*)sqlite3_column_text(stmt, 2));

        book.translator =
            QString::fromUtf8(
                (const char*)sqlite3_column_text(stmt, 3));

        book.annotation =
            QString::fromUtf8(
                (const char*)sqlite3_column_text(stmt, 4));

        book.genre =
            QString::fromUtf8(
                (const char*)sqlite3_column_text(stmt, 5));

        book.totalPages =
            sqlite3_column_int(stmt, 6);

        book.lastPage =
            sqlite3_column_int(stmt, 7);

        result.push_back(book);
    }

    sqlite3_finalize(stmt);

    return result;
}

QVector<DbChapter> Database::listChapters(
    int bookId,
    QString* errorOut) const
{
    QVector<DbChapter> result;

    if (!db)
        return result;

    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "SELECT "
        "id,"
        "book_id,"
        "idx,"
        "title,"
        "text "
        "FROM chapters "
        "WHERE book_id=? "
        "ORDER BY idx ASC;";

    if (sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &stmt,
        nullptr) != SQLITE_OK)
    {
        if (errorOut)
            *errorOut = sqliteErr(db);

        return result;
    }

    sqlite3_bind_int(stmt, 1, bookId);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        DbChapter chapter;

        chapter.id =
            sqlite3_column_int(stmt, 0);

        chapter.bookId =
            sqlite3_column_int(stmt, 1);

        chapter.index =
            sqlite3_column_int(stmt, 2);

        chapter.title =
            QString::fromUtf8(
                reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, 3)));

        chapter.content =
            QString::fromUtf8(
                reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, 4)));

        result.push_back(chapter);
    }

    sqlite3_finalize(stmt);

    return result;
}

bool Database::getBook(
    int bookId,
    DbBook* out,
    QString* errorOut) const
{
    if (!db || !out)
        return false;

    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "SELECT "
        "id,"
        "title,"
        "author,"
        "translator,"
        "annotation,"
        "genre,"
        "total_pages,"
        "last_page "
        "FROM books "
        "WHERE id=?;";

    if (sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &stmt,
        nullptr) != SQLITE_OK)
    {
        if (errorOut)
            *errorOut = sqliteErr(db);

        return false;
    }

    sqlite3_bind_int(stmt, 1, bookId);

    bool found =
        sqlite3_step(stmt) == SQLITE_ROW;

    if (found)
    {
        out->id =
            sqlite3_column_int(stmt, 0);

        out->title =
            QString::fromUtf8(
                (const char*)sqlite3_column_text(stmt, 1));

        out->author =
            QString::fromUtf8(
                (const char*)sqlite3_column_text(stmt, 2));

        out->translator =
            QString::fromUtf8(
                (const char*)sqlite3_column_text(stmt, 3));

        out->annotation =
            QString::fromUtf8(
                (const char*)sqlite3_column_text(stmt, 4));

        out->genre =
            QString::fromUtf8(
                (const char*)sqlite3_column_text(stmt, 5));

        out->totalPages =
            sqlite3_column_int(stmt, 6);

        out->lastPage =
            sqlite3_column_int(stmt, 7);
    }

    sqlite3_finalize(stmt);

    return found;
}

bool Database::getSettings(
    int bookId,
    ReaderSettings* out,
    QString* errorOut) const
{
    if (!db || !out)
        return false;

    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "SELECT "
        "font_family,"
        "font_size,"
        "align,"
        "line_height "
        "FROM reader_settings "
        "WHERE book_id=?;";

    if (sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &stmt,
        nullptr) != SQLITE_OK)
    {
        if (errorOut)
            *errorOut = sqliteErr(db);

        return false;
    }

    sqlite3_bind_int(stmt, 1, bookId);

    bool found =
        sqlite3_step(stmt) == SQLITE_ROW;

    if (found)
    {
        out->fontFamily =
            QString::fromUtf8(
                (const char*)sqlite3_column_text(stmt, 0));

        out->fontSize =
            sqlite3_column_int(stmt, 1);

        out->align =
            sqlite3_column_int(stmt, 2);

        out->lineHeight =
            sqlite3_column_double(stmt, 3);
    }

    sqlite3_finalize(stmt);

    return found;
}

bool Database::upsertSettings(
    int bookId,
    const ReaderSettings& s,
    QString* errorOut)
{
    if (!openOrCreate())
        return false;

    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "INSERT INTO reader_settings("
        "book_id,"
        "font_family,"
        "font_size,"
        "align,"
        "line_height"
        ") VALUES(?,?,?,?,?) "
        "ON CONFLICT(book_id) DO UPDATE SET "
        "font_family=excluded.font_family,"
        "font_size=excluded.font_size,"
        "align=excluded.align,"
        "line_height=excluded.line_height;";

    if (sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &stmt,
        nullptr) != SQLITE_OK)
    {
        if (errorOut)
            *errorOut = sqliteErr(db);

        return false;
    }

    QByteArray ffUtf8 =
        s.fontFamily.toUtf8();

    sqlite3_bind_int(stmt, 1, bookId);

    sqlite3_bind_text(
        stmt,
        2,
        ffUtf8.constData(),
        ffUtf8.size(),
        SQLITE_TRANSIENT);

    sqlite3_bind_int(
        stmt,
        3,
        s.fontSize);

    sqlite3_bind_int(
        stmt,
        4,
        s.align);

    sqlite3_bind_double(
        stmt,
        5,
        s.lineHeight);

    bool ok =
        sqlite3_step(stmt) == SQLITE_DONE;

    if (!ok && errorOut)
        *errorOut = sqliteErr(db);

    sqlite3_finalize(stmt);

    return ok;
}

bool Database::updateLastPage(
    int bookId,
    int lastPage,
    QString* errorOut)
{
    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "UPDATE books "
        "SET last_page=?, "
        "last_read_at=? "
        "WHERE id=?;";

    if (sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &stmt,
        nullptr) != SQLITE_OK)
    {
        if (errorOut)
            *errorOut = sqliteErr(db);

        return false;
    }

    sqlite3_bind_int(stmt, 1, lastPage);

    sqlite3_bind_int64(
        stmt,
        2,
        QDateTime::currentSecsSinceEpoch());

    sqlite3_bind_int(stmt, 3, bookId);

    bool ok =
        sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);

    return ok;
}

bool Database::updateTotalPages(
    int bookId,
    int totalPages,
    QString* errorOut)
{
    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "UPDATE books "
        "SET total_pages=? "
        "WHERE id=?;";

    if (sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &stmt,
        nullptr) != SQLITE_OK)
    {
        if (errorOut)
            *errorOut = sqliteErr(db);

        return false;
    }

    sqlite3_bind_int(stmt, 1, totalPages);
    sqlite3_bind_int(stmt, 2, bookId);

    bool ok =
        sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);

    return ok;
}

bool Database::getChapterText(
    int bookId,
    int chapterIndex,
    QString* out,
    QString* errorOut
) const
{
    if (!db || !out)
        return false;

    out->clear();

    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "SELECT text "
        "FROM chapters "
        "WHERE book_id=? AND idx=?;";

    if (sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &stmt,
        nullptr) != SQLITE_OK)
    {
        if (errorOut)
            *errorOut = sqliteErr(db);

        return false;
    }

    sqlite3_bind_int(stmt, 1, bookId);
    sqlite3_bind_int(stmt, 2, chapterIndex);

    bool ok = false;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char* txt =
            reinterpret_cast<const char*>(
                sqlite3_column_text(stmt, 0));

        if (txt)
        {
            *out = QString::fromUtf8(txt);
            ok = true;
        }
    }

    sqlite3_finalize(stmt);

    return ok;
}