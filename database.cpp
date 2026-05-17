#include "database.h"
#include "fb2metaparser.h"
#include "fb2textparser.h"

#include <SQLite/sqlite3.h>
#include <QDateTime>
#include <QDir>

static QString sqliteErr(sqlite3* db)
{
    const char* msg = db ? sqlite3_errmsg(db) : "no db";
    return QString::fromUtf8(msg ? msg : "unknown error");
}

static bool columnExists(sqlite3* db, const char* table, const char* column)
{
    const QString sql = QString("PRAGMA table_info(%1);").arg(QString::fromLatin1(table));
    const QByteArray sqlUtf8 = sql.toUtf8();

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sqlUtf8.constData(), -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name && QString::fromUtf8(name) == QString::fromLatin1(column))
        {
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

static bool tableExists(sqlite3* db, const char* table)
{
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?;",
        -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, table, -1, SQLITE_TRANSIENT);
    const bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

static bool schemaNeedsReset(sqlite3* db)
{
    if (!tableExists(db, "books"))
        return false;

    return columnExists(db, "books", "file_path")
        || columnExists(db, "books", "parsed_text")
        || !columnExists(db, "books", "file_data")
        || !columnExists(db, "books", "translator")
        || !columnExists(db, "books", "annotation")
        || !columnExists(db, "books", "genre")
        || !columnExists(db, "books", "last_read_at");
}

static bool resetSchema(sqlite3* db, QString* errorOut)
{
    char* msg = nullptr;
    const char* sql =
        "PRAGMA foreign_keys = OFF;"
        "DROP TABLE IF EXISTS notes;"
        "DROP TABLE IF EXISTS bookmarks;"
        "DROP TABLE IF EXISTS reader_settings;"
        "DROP TABLE IF EXISTS book_images;"
        "DROP TABLE IF EXISTS chapters;"
        "DROP TABLE IF EXISTS books;"
        "PRAGMA foreign_keys = ON;";

    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &msg);
    if (rc != SQLITE_OK)
    {
        if (errorOut) *errorOut = QString::fromUtf8(msg ? msg : "sqlite schema reset error");
        sqlite3_free(msg);
        return false;
    }
    return true;
}

static bool replaceChapters(sqlite3* db, int bookId, const QVector<ParsedChapter>& chapters, QString* errorOut)
{
    sqlite3_stmt* deleteStmt = nullptr;
    if (sqlite3_prepare_v2(db, "DELETE FROM chapters WHERE book_id=?;", -1, &deleteStmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return false;
    }
    sqlite3_bind_int(deleteStmt, 1, bookId);
    const bool deleted = (sqlite3_step(deleteStmt) == SQLITE_DONE);
    sqlite3_finalize(deleteStmt);
    if (!deleted)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return false;
    }

    sqlite3_stmt* chapterStmt = nullptr;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO chapters(book_id,idx,title,start_offset) VALUES(?,?,?,?);",
        -1, &chapterStmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return false;
    }

    int startOffset = 0;
    for (const auto& chapter : chapters)
    {
        const QString title = chapter.title.trimmed().isEmpty()
            ? QString("Глава %1").arg(chapter.index + 1)
            : chapter.title.trimmed();
        const QByteArray titleUtf8 = title.toUtf8();

        sqlite3_bind_int(chapterStmt, 1, bookId);
        sqlite3_bind_int(chapterStmt, 2, chapter.index);
        sqlite3_bind_text(chapterStmt, 3, titleUtf8.constData(), titleUtf8.size(), SQLITE_TRANSIENT);
        sqlite3_bind_int(chapterStmt, 4, startOffset);

        if (sqlite3_step(chapterStmt) != SQLITE_DONE)
        {
            if (errorOut) *errorOut = sqliteErr(db);
            sqlite3_finalize(chapterStmt);
            return false;
        }
        sqlite3_reset(chapterStmt);
        sqlite3_clear_bindings(chapterStmt);

        startOffset += chapter.text.size() + 2;
    }

    sqlite3_finalize(chapterStmt);
    return true;
}

Database::~Database()
{
    if (db) { sqlite3_close(db); db = nullptr; }
}

Database& Database::instance()
{
    static Database inst;
    return inst;
}

bool Database::openOrCreate(const QString& dbPath)
{
    if (db) return true;

    const QByteArray path = dbPath.toUtf8();
    if (sqlite3_open(path.constData(), &db) != SQLITE_OK)
    {
        sqlite3_close(db);
        db = nullptr;
        return false;
    }

    QString err;
    return ensureSchema(&err);
}

bool Database::exec(const QString& sql, QString* errorOut) const
{
    char* msg = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    const int rc = sqlite3_exec(db, utf8.constData(), nullptr, nullptr, &msg);
    if (rc != SQLITE_OK)
    {
        if (errorOut) *errorOut = QString::fromUtf8(msg ? msg : "sqlite error");
        sqlite3_free(msg);
        return false;
    }
    return true;
}

bool Database::ensureSchema(QString* errorOut)
{
    if (schemaNeedsReset(db) && !resetSchema(db, errorOut))
        return false;

    const QString schema = R"SQL(
PRAGMA journal_mode=WAL;
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS books (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  file_data    BLOB NOT NULL,
  title        TEXT NOT NULL,
  author       TEXT NOT NULL,
  translator   TEXT NOT NULL DEFAULT '',
  annotation   TEXT NOT NULL DEFAULT '',
  genre        TEXT NOT NULL DEFAULT '',
  cover        BLOB,
  total_pages  INTEGER NOT NULL DEFAULT 0,
  last_page    INTEGER NOT NULL DEFAULT 0,
  created_at   INTEGER NOT NULL,
  last_read_at INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS chapters (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  book_id      INTEGER NOT NULL,
  idx          INTEGER NOT NULL,
  title        TEXT NOT NULL,
  start_offset INTEGER NOT NULL,
  FOREIGN KEY(book_id) REFERENCES books(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS book_images (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  book_id      INTEGER NOT NULL,
  image_id     TEXT NOT NULL,
  content_type TEXT NOT NULL,
  data         BLOB NOT NULL,
  UNIQUE(book_id, image_id),
  FOREIGN KEY(book_id) REFERENCES books(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS reader_settings (
  book_id     INTEGER PRIMARY KEY,
  font_family TEXT NOT NULL,
  font_size   INTEGER NOT NULL,
  align       INTEGER NOT NULL,
  line_height REAL NOT NULL,
  FOREIGN KEY(book_id) REFERENCES books(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS bookmarks (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  book_id    INTEGER NOT NULL,
  page       INTEGER NOT NULL,
  created_at INTEGER NOT NULL,
  FOREIGN KEY(book_id) REFERENCES books(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS notes (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  book_id    INTEGER NOT NULL,
  page       INTEGER NOT NULL,
  text       TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  FOREIGN KEY(book_id) REFERENCES books(id) ON DELETE CASCADE
);
)SQL";

    if (!exec(schema, errorOut))
        return false;

    return true;
}

int Database::addOrUpdateBookFromMeta(const Fb2Meta& meta, QString* errorOut)
{
    if (!openOrCreate())
    {
        if (errorOut) *errorOut = "Не удалось открыть БД.";
        return 0;
    }

    const qint64 createdAt = QDateTime::currentSecsSinceEpoch();

    const char* sql =
        "INSERT INTO books(file_data,title,author,translator,annotation,genre,cover,created_at) "
        "VALUES(?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return 0;
    }

    const QByteArray titleUtf8 = meta.title.toUtf8();
    const QByteArray authorUtf8 = meta.author.toUtf8();
    const QByteArray translatorUtf8 = meta.translator.toUtf8();
    const QByteArray annotationUtf8 = meta.annotation.toUtf8();
    const QByteArray genreUtf8 = meta.genre.toUtf8();

    if (meta.rawFileData.isEmpty())
    {
        if (errorOut) *errorOut = "Пустой файл книги.";
        sqlite3_finalize(stmt);
        return 0;
    }
    else
        sqlite3_bind_blob(stmt, 1, meta.rawFileData.constData(), meta.rawFileData.size(), SQLITE_TRANSIENT);

    sqlite3_bind_text(stmt, 2, titleUtf8.constData(), titleUtf8.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, authorUtf8.constData(), authorUtf8.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, translatorUtf8.constData(), translatorUtf8.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, annotationUtf8.constData(), annotationUtf8.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, genreUtf8.constData(), genreUtf8.size(), SQLITE_TRANSIENT);

    if (meta.cover.isEmpty())
        sqlite3_bind_null(stmt, 7);
    else
        sqlite3_bind_blob(stmt, 7, meta.cover.constData(), meta.cover.size(), SQLITE_TRANSIENT);

    sqlite3_bind_int64(stmt, 8, createdAt);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);

    const int bookId = (int)sqlite3_last_insert_rowid(db);

    // Сохраняем картинки
    if (!meta.binaries.isEmpty())
    {
        sqlite3_stmt* imgStmt = nullptr;
        if (sqlite3_prepare_v2(db,
            "INSERT OR REPLACE INTO book_images(book_id,image_id,content_type,data) VALUES(?,?,?,?);",
            -1, &imgStmt, nullptr) == SQLITE_OK)
        {
            for (auto it = meta.binaries.constBegin(); it != meta.binaries.constEnd(); ++it)
            {
                const QByteArray idUtf8 = it.key().toUtf8();
                const QByteArray typeUtf8 = meta.binaryTypes.value(it.key(), "image/jpeg").toUtf8();
                const QByteArray& data = it.value();

                sqlite3_bind_int(imgStmt, 1, bookId);
                sqlite3_bind_text(imgStmt, 2, idUtf8.constData(), idUtf8.size(), SQLITE_TRANSIENT);
                sqlite3_bind_text(imgStmt, 3, typeUtf8.constData(), typeUtf8.size(), SQLITE_TRANSIENT);
                sqlite3_bind_blob(imgStmt, 4, data.constData(), data.size(), SQLITE_TRANSIENT);

                sqlite3_step(imgStmt);
                sqlite3_reset(imgStmt);
            }
            sqlite3_finalize(imgStmt);
        }
    }

    // Настройки по умолчанию
    ReaderSettings def;
    if (!getSettings(bookId, &def, nullptr))
        upsertSettings(bookId, def, nullptr);

    return bookId;
}

QByteArray Database::getBookCover(int bookId, QString* errorOut) const
{
    QByteArray result;
    if (!db) return result;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT cover FROM books WHERE id=?;", -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return result;
    }
    sqlite3_bind_int(stmt, 1, bookId);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const void* data = sqlite3_column_blob(stmt, 0);
        const int   size = sqlite3_column_bytes(stmt, 0);
        if (data && size > 0)
            result = QByteArray(static_cast<const char*>(data), size);
    }
    sqlite3_finalize(stmt);
    return result;
}

QByteArray Database::getBookFileData(int bookId, QString* errorOut) const
{
    QByteArray result;
    if (!db) return result;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT file_data FROM books WHERE id=?;", -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return result;
    }
    sqlite3_bind_int(stmt, 1, bookId);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const void* data = sqlite3_column_blob(stmt, 0);
        const int   size = sqlite3_column_bytes(stmt, 0);
        if (data && size > 0)
            result = QByteArray(static_cast<const char*>(data), size);
    }
    sqlite3_finalize(stmt);
    return result;
}

QByteArray Database::getBookImage(int bookId, const QString& imageId, QString* errorOut) const
{
    QByteArray result;
    if (!db) return result;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db,
        "SELECT data FROM book_images WHERE book_id=? AND image_id=?;",
        -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return result;
    }
    const QByteArray idUtf8 = imageId.toUtf8();
    sqlite3_bind_int(stmt, 1, bookId);
    sqlite3_bind_text(stmt, 2, idUtf8.constData(), idUtf8.size(), SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const void* data = sqlite3_column_blob(stmt, 0);
        const int   size = sqlite3_column_bytes(stmt, 0);
        if (data && size > 0)
            result = QByteArray(static_cast<const char*>(data), size);
    }
    sqlite3_finalize(stmt);
    return result;
}

bool Database::ensureTextCache(int bookId, QString* errorOut)
{
    if (!openOrCreate())
    {
        if (errorOut) *errorOut = "Не удалось открыть БД.";
        return false;
    }

    const QString cacheDir = QDir::current().filePath("caches");
    QString err;

    if (!Fb2TextParser::isCached(bookId, cacheDir))
    {
        const QByteArray fileData = getBookFileData(bookId, &err);
        if (fileData.isEmpty())
        {
            if (errorOut) *errorOut = err.isEmpty() ? "Файл книги не найден в БД." : err;
            return false;
        }

        if (Fb2TextParser::parseAndCache(bookId, fileData, cacheDir, &err).isEmpty())
        {
            if (errorOut) *errorOut = err;
            return false;
        }
    }

    const QVector<ParsedChapter> chapters = Fb2TextParser::loadFromCache(bookId, cacheDir, &err);
    if (chapters.isEmpty())
    {
        if (errorOut) *errorOut = err.isEmpty() ? "Не удалось загрузить текст книги." : err;
        return false;
    }

    return replaceChapters(db, bookId, chapters, errorOut);
}

bool Database::getParsedText(int bookId, QString* out, QString* errorOut)
{
    if (!out)
        return false;

    out->clear();

    const QString cacheDir = QDir::current().filePath("caches");
    QString err;
    if (!ensureTextCache(bookId, &err))
    {
        if (errorOut) *errorOut = err;
        return false;
    }

    const QVector<ParsedChapter> chapters = Fb2TextParser::loadFromCache(bookId, cacheDir, &err);
    if (chapters.isEmpty())
    {
        if (errorOut) *errorOut = err.isEmpty() ? "Не удалось загрузить текст книги." : err;
        return false;
    }

    for (const auto& chapter : chapters)
    {
        if (!out->isEmpty())
            *out += "\n\n";
        *out += chapter.text;
    }

    return !out->isEmpty();
}

QVector<DbBook> Database::listBooks(QString* errorOut) const
{
    QVector<DbBook> result;
    if (!db) return result;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id,title,author,translator,annotation,genre,total_pages,last_page "
        "FROM books ORDER BY created_at DESC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        DbBook b;
        b.id = sqlite3_column_int(stmt, 0);
        b.title = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 1));
        b.author = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 2));
        b.translator = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 3));
        b.annotation = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 4));
        b.genre = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 5));
        b.totalPages = sqlite3_column_int(stmt, 6);
        b.lastPage = sqlite3_column_int(stmt, 7);
        result.push_back(b);
    }
    sqlite3_finalize(stmt);
    return result;
}

QVector<DbBook> Database::listRecentlyAdded(int limit, QString* errorOut) const
{
    QVector<DbBook> result;
    if (!db) return result;

    const QString sql = QString(
        "SELECT id,title,author,translator,annotation,genre,total_pages,last_page "
        "FROM books ORDER BY created_at DESC %1;")
        .arg(limit > 0 ? QString("LIMIT %1").arg(limit) : "");

    sqlite3_stmt* stmt = nullptr;
    const QByteArray sqlUtf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db, sqlUtf8.constData(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        DbBook b;
        b.id = sqlite3_column_int(stmt, 0);
        b.title = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 1));
        b.author = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 2));
        b.translator = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 3));
        b.annotation = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 4));
        b.genre = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 5));
        b.totalPages = sqlite3_column_int(stmt, 6);
        b.lastPage = sqlite3_column_int(stmt, 7);
        result.push_back(b);
    }
    sqlite3_finalize(stmt);
    return result;
}

QVector<DbBook> Database::listRecentlyRead(int limit, QString* errorOut) const
{
    QVector<DbBook> result;
    if (!db) return result;

    const QString sql = QString(
        "SELECT id,title,author,translator,annotation,genre,total_pages,last_page "
        "FROM books WHERE last_read_at > 0 ORDER BY last_read_at DESC %1;")
        .arg(limit > 0 ? QString("LIMIT %1").arg(limit) : "");

    sqlite3_stmt* stmt = nullptr;
    const QByteArray sqlUtf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db, sqlUtf8.constData(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        DbBook b;
        b.id = sqlite3_column_int(stmt, 0);
        b.title = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 1));
        b.author = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 2));
        b.translator = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 3));
        b.annotation = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 4));
        b.genre = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 5));
        b.totalPages = sqlite3_column_int(stmt, 6);
        b.lastPage = sqlite3_column_int(stmt, 7);
        result.push_back(b);
    }
    sqlite3_finalize(stmt);
    return result;
}

bool Database::getBook(int bookId, DbBook* out, QString* errorOut) const
{
    if (!db || !out) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id,title,author,translator,annotation,genre,total_pages,last_page "
        "FROM books WHERE id=?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return false;
    }

    sqlite3_bind_int(stmt, 1, bookId);
    const bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    if (found)
    {
        out->id = sqlite3_column_int(stmt, 0);
        out->title = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 1));
        out->author = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 2));
        out->translator = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 3));
        out->annotation = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 4));
        out->genre = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 5));
        out->totalPages = sqlite3_column_int(stmt, 6);
        out->lastPage = sqlite3_column_int(stmt, 7);
    }
    sqlite3_finalize(stmt);
    if (!found && errorOut) *errorOut = "Книга не найдена.";
    return found;
}

QVector<DbChapter> Database::listChapters(int bookId, QString* errorOut) const
{
    QVector<DbChapter> result;
    if (!db) return result;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id,book_id,idx,title,start_offset "
        "FROM chapters WHERE book_id=? ORDER BY idx ASC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return result;
    }
    sqlite3_bind_int(stmt, 1, bookId);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        DbChapter ch;
        ch.id = sqlite3_column_int(stmt, 0);
        ch.bookId = sqlite3_column_int(stmt, 1);
        ch.index = sqlite3_column_int(stmt, 2);
        ch.title = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 3));
        ch.startOffset = sqlite3_column_int(stmt, 4);
        result.push_back(ch);
    }
    sqlite3_finalize(stmt);
    return result;
}

bool Database::getSettings(int bookId, ReaderSettings* out, QString* errorOut) const
{
    if (!db || !out) return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db,
        "SELECT font_family,font_size,align,line_height FROM reader_settings WHERE book_id=?;",
        -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return false;
    }
    sqlite3_bind_int(stmt, 1, bookId);
    const bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    if (found)
    {
        out->fontFamily = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 0));
        out->fontSize = sqlite3_column_int(stmt, 1);
        out->align = sqlite3_column_int(stmt, 2);
        out->lineHeight = sqlite3_column_double(stmt, 3);
    }
    sqlite3_finalize(stmt);
    return found;
}

bool Database::upsertSettings(int bookId, const ReaderSettings& s, QString* errorOut)
{
    if (!openOrCreate()) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO reader_settings(book_id,font_family,font_size,align,line_height) "
        "VALUES(?,?,?,?,?) "
        "ON CONFLICT(book_id) DO UPDATE SET "
        "font_family=excluded.font_family, font_size=excluded.font_size, "
        "align=excluded.align, line_height=excluded.line_height;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return false;
    }

    const QByteArray ffUtf8 = s.fontFamily.toUtf8();
    sqlite3_bind_int(stmt, 1, bookId);
    sqlite3_bind_text(stmt, 2, ffUtf8.constData(), ffUtf8.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, s.fontSize);
    sqlite3_bind_int(stmt, 4, s.align);
    sqlite3_bind_double(stmt, 5, s.lineHeight);

    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && errorOut) *errorOut = sqliteErr(db);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::updateLastPage(int bookId, int lastPage, QString* errorOut)
{
    if (!openOrCreate()) return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db,
        "UPDATE books SET last_page=?, last_read_at=? WHERE id=?;",
        -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return false;
    }
    sqlite3_bind_int(stmt, 1, lastPage);
    sqlite3_bind_int64(stmt, 2, QDateTime::currentSecsSinceEpoch());
    sqlite3_bind_int(stmt, 3, bookId);
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::updateTotalPages(int bookId, int totalPages, QString* errorOut)
{
    if (!openOrCreate()) return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db,
        "UPDATE books SET total_pages=? WHERE id=?;",
        -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return false;
    }
    sqlite3_bind_int(stmt, 1, totalPages);
    sqlite3_bind_int(stmt, 2, bookId);
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

