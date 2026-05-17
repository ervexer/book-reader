/*
/////////////////////////////////////////FB2PARSER.CPP

#include "fb2parser.h"

#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QHash>

static QString normalizeSpaces(QString s)
{
    s.replace('\r', '\n');
    s.replace(QChar(0x00A0), ' ');
    while (s.contains("\n\n\n"))
        s.replace("\n\n\n", "\n\n");
    return s.trimmed();
}

static QString joinAuthor(const QString& first, const QString& last, const QString& middle)
{
    QStringList parts;
    if (!last.trimmed().isEmpty())   parts << last.trimmed();
    if (!first.trimmed().isEmpty())  parts << first.trimmed();
    if (!middle.trimmed().isEmpty()) parts << middle.trimmed();
    return parts.join(' ');
}

bool Fb2Parser::parseFile(const QString& filePath, Fb2ParsedBook* out, QString* errorOut)
{
    if (!out)
        return false;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (errorOut) *errorOut = "Не удалось открыть файл книги.";
        return false;
    }

    // Сохраняем сырые байты файла для хранения в БД
    out->rawFileData = f.readAll();
    f.seek(0); // возвращаемся в начало для парсинга XML

    QXmlStreamReader xml(&f);

    QString bookTitle;
    QString authorFirst, authorLast, authorMiddle;

    // Все бинарные данные (картинки) из <binary id="...">BASE64</binary>
    QHash<QString, QByteArray> binaries;

    // id обложки из <coverpage><image l:href="#id"/></coverpage>
    QString coverRef;
    bool inCoverpage = false; // флаг что мы внутри <coverpage>

    // Накопленный текст книги
    QString plain;
    int chapterIndex = 0;
    QVector<ParsedChapter> chapters;

    // Добавляет абзац в общий текст через двойной перенос
    auto appendParagraph = [&](const QString& paragraph) {
        const QString trimmed = paragraph.trimmed();
        if (trimmed.isEmpty()) return;
        if (!plain.isEmpty()) plain += "\n\n";
        plain += trimmed;
        };

    // Текущий накапливаемый текст внутри тега
    QString currentText;
    QString lastTitleParagraph;

    // --- Флаги состояния парсера ---
    bool inBookTitle = false;
    bool inAuthor = false;
    bool inFirst = false;
    bool inLast = false;
    bool inMiddle = false;
    bool inBinary = false;
    bool inBody = false;
    bool inSection = false;
    bool inTitle = false;
    bool inP = false;

    QString binaryId;
    QString binaryContentType;

    while (!xml.atEnd())
    {
        xml.readNext();

        if (xml.isStartElement())
        {
            const QString name = xml.name().toString();

            if (name == "book-title")
            {
                inBookTitle = true;
                currentText.clear();
            }
            else if (name == "author")
            {
                inAuthor = true;
                authorFirst.clear();
                authorLast.clear();
                authorMiddle.clear();
            }
            else if (inAuthor && name == "first-name") { inFirst = true; currentText.clear(); }
            else if (inAuthor && name == "last-name") { inLast = true; currentText.clear(); }
            else if (inAuthor && name == "middle-name") { inMiddle = true; currentText.clear(); }
            else if (name == "coverpage")
            {
                inCoverpage = true;
            }
            else if (name == "image")
            {
                const QString href = xml.attributes().value("l:href").toString();
                if (!href.isEmpty())
                {
                    if (inCoverpage)
                    {
                        // Обложка — только из <coverpage>
                        coverRef = href;
                    }
                    else if (inBody)
                    {
                        // Картинка внутри текста — вставляем маркер
                        // чтобы ридер мог заменить его на реальное изображение
                        const QString imageId = href.startsWith('#') ? href.mid(1) : href;
                        appendParagraph(QString("[[IMAGE:%1]]").arg(imageId));
                    }
                }
            }
            else if (name == "binary")
            {
                inBinary = true;
                binaryId = xml.attributes().value("id").toString();
                binaryContentType = xml.attributes().value("content-type").toString();
                currentText.clear();
            }
            else if (name == "body")
            {
                inBody = true;
            }
            else if (inBody && name == "section")
            {
                inSection = true;
            }
            else if (inSection && name == "title")
            {
                inTitle = true;
                lastTitleParagraph.clear();
            }
            else if (inTitle && name == "p")
            {
                inP = true;
                currentText.clear();
            }
            else if (inSection && !inTitle && name == "p")
            {
                inP = true;
                currentText.clear();
            }
        }
        else if (xml.isCharacters() && !xml.isWhitespace())
        {
            if (inBookTitle || inFirst || inLast || inMiddle || inP || inBinary)
                currentText += xml.text().toString();
        }
        else if (xml.isEndElement())
        {
            const QString name = xml.name().toString();

            if (name == "book-title")
            {
                inBookTitle = false;
                bookTitle = currentText.trimmed();
            }
            else if (name == "first-name") { inFirst = false; authorFirst = currentText.trimmed(); }
            else if (name == "last-name") { inLast = false; authorLast = currentText.trimmed(); }
            else if (name == "middle-name") { inMiddle = false; authorMiddle = currentText.trimmed(); }
            else if (name == "author")
            {
                inAuthor = false;
            }
            else if (name == "coverpage")
            {
                inCoverpage = false;
            }
            else if (name == "binary")
            {
                inBinary = false;
                if (!binaryId.isEmpty())
                {
                    const QByteArray decoded = QByteArray::fromBase64(currentText.toUtf8());
                    if (!decoded.isEmpty())
                    {
                        // Сохраняем вместе с типом чтобы ридер знал jpeg это или png
                        binaries.insert(binaryId, decoded);

                        // Сохраняем content-type отдельно
                        if (!out->binaryTypes.contains(binaryId))
                            out->binaryTypes.insert(binaryId, binaryContentType);
                    }
                }
            }
            else if (name == "p" && inP)
            {
                inP = false;
                if (inTitle)
                    lastTitleParagraph = currentText.trimmed();
                else
                    appendParagraph(currentText);
            }
            else if (name == "title" && inTitle)
            {
                inTitle = false;

                const QString chapterTitle = lastTitleParagraph.trimmed();
                if (!chapterTitle.isEmpty())
                {
                    ParsedChapter ch;
                    ch.index = chapterIndex++;
                    ch.title = chapterTitle;
                    ch.startOffset = plain.size();
                    chapters.push_back(ch);

                    // Вставляем заголовок главы в текст
                    const QString header = QString("Глава %1. %2").arg(chapterIndex).arg(chapterTitle);
                    appendParagraph(header);
                }
            }
            else if (name == "section") { inSection = false; }
            else if (name == "body") { inBody = false; }
        }
    }

    if (xml.hasError())
    {
        if (errorOut) *errorOut = "Ошибка разбора FB2 (XML): " + xml.errorString();
        return false;
    }

    // --- Сборка результата ---

    out->title = bookTitle.isEmpty() ? QFileInfo(filePath).baseName() : bookTitle;

    out->author = joinAuthor(authorFirst, authorLast, authorMiddle);
    if (out->author.trimmed().isEmpty())
        out->author = "Неизвестный автор";

    // Обложка — ищем по id из coverRef (убираем '#')
    if (!coverRef.isEmpty())
    {
        const QString coverId = coverRef.startsWith('#') ? coverRef.mid(1) : coverRef;
        out->cover = binaries.value(coverId);
    }

    // Все картинки книги для ридера
    out->binaries = binaries;

    out->plainText = normalizeSpaces(plain);
    if (out->plainText.isEmpty())
        out->plainText = "(Пустой текст книги)";

    out->chapters = chapters;
    if (out->chapters.isEmpty())
    {
        ParsedChapter ch;
        ch.index = 0;
        ch.title = "Без названия";
        ch.startOffset = 0;
        out->chapters.push_back(ch);
    }

    return true;
}

/////////////////////////////FB2PARSER.H
#pragma once

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QHash>  // добавить этот include

struct ParsedChapter
{
    int index = 0;
    QString title;
    int startOffset = 0;
};

struct Fb2ParsedBook
{
    QString title;
    QString author;
    QByteArray cover;
    QString plainText;
    QVector<ParsedChapter> chapters;

    // Новые поля:
    QByteArray rawFileData;                  // сырые байты FB2 файла
    QHash<QString, QByteArray> binaries;     // все картинки: id -> байты
    QHash<QString, QString> binaryTypes;     // тип картинки: id -> "image/jpeg"
};

class Fb2Parser
{
public:
    static bool parseFile(const QString& filePath, Fb2ParsedBook* out, QString* errorOut = nullptr);
};

/////////////////////////////DATABASE.CPP
#include "database.h"
#include "fb2parser.h"
#include <SQLite/sqlite3.h>

#include <QFileInfo>
#include <QDateTime>
#include <QFile>

static QString sqliteErr(sqlite3* db)
{
    const char* message = db ? sqlite3_errmsg(db) : "no db";
    return QString::fromUtf8(message ? message : "unknown sqlite error");
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

bool Database::openOrCreate(const QString& dbPath)
{
    if (db)
        return true;

    const QByteArray pathUtf8 = QFileInfo(dbPath).absoluteFilePath().toUtf8();
    if (sqlite3_open(pathUtf8.constData(), &db) != SQLITE_OK)
    {
        sqlite3_close(db);
        db = nullptr;
        return false;
    }

    QString schemaError;
    if (!ensureSchema(&schemaError))
        return false;

    return true;
}

bool Database::exec(const QString& sql, QString* errorOut) const
{
    char* sqliteErrorMessage = nullptr;
    const QByteArray queryUtf8 = sql.toUtf8();
    const int resultCode = sqlite3_exec(db, queryUtf8.constData(), nullptr, nullptr, &sqliteErrorMessage);
    if (resultCode != SQLITE_OK)
    {
        if (errorOut)
            *errorOut = QString::fromUtf8(sqliteErrorMessage ? sqliteErrorMessage : "sqlite error");
        sqlite3_free(sqliteErrorMessage);
        return false;
    }
    return true;
}

bool Database::ensureSchema(QString* errorOut)
{
    const QString schema = R"SQL(
PRAGMA journal_mode=WAL;
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS books (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  file_path    TEXT NOT NULL UNIQUE,
  file_data    BLOB,
  title        TEXT NOT NULL,
  author       TEXT NOT NULL,
  cover        BLOB,
  parsed_text  TEXT NOT NULL,
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

    return exec(schema, errorOut);
}

int Database::addOrUpdateBookFromFb2File(const QString& filePath, QString* errorOut)
{
    if (!openOrCreate())
    {
        if (errorOut) *errorOut = "Не удалось открыть базу данных.";
        return 0;
    }

    // Парсим FB2
    Fb2ParsedBook parsedBook;
    QString parseError;
    if (!Fb2Parser::parseFile(filePath, &parsedBook, &parseError))
    {
        if (errorOut) *errorOut = parseError;
        return 0;
    }

    const QByteArray filePathUtf8 = QFileInfo(filePath).absoluteFilePath().toUtf8();
    const QByteArray titleUtf8 = parsedBook.title.toUtf8();
    const QByteArray authorUtf8 = parsedBook.author.toUtf8();
    const QByteArray plainTextUtf8 = parsedBook.plainText.toUtf8();
    const qint64     createdAt = QDateTime::currentSecsSinceEpoch();

    // UPSERT книги (включая file_data — сырые байты файла)
    sqlite3_stmt* insertStatement = nullptr;
    const char* upsertSql =
        "INSERT INTO books(file_path,file_data,title,author,cover,parsed_text,total_pages,last_page,created_at) "
        "VALUES(?,?,?,?,?,?,0,0,?) "
        "ON CONFLICT(file_path) DO UPDATE SET "
        "file_data=excluded.file_data, "
        "title=excluded.title, "
        "author=excluded.author, "
        "cover=excluded.cover, "
        "parsed_text=excluded.parsed_text;";

    if (sqlite3_prepare_v2(db, upsertSql, -1, &insertStatement, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return 0;
    }

    sqlite3_bind_text(insertStatement, 1, filePathUtf8.constData(), (int)filePathUtf8.size(), SQLITE_TRANSIENT);

    // file_data — сырые байты файла
    if (parsedBook.rawFileData.isEmpty())
        sqlite3_bind_null(insertStatement, 2);
    else
        sqlite3_bind_blob(insertStatement, 2, parsedBook.rawFileData.constData(), (int)parsedBook.rawFileData.size(), SQLITE_TRANSIENT);

    sqlite3_bind_text(insertStatement, 3, titleUtf8.constData(), (int)titleUtf8.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(insertStatement, 4, authorUtf8.constData(), (int)authorUtf8.size(), SQLITE_TRANSIENT);

    // cover — обложка
    if (parsedBook.cover.isEmpty())
        sqlite3_bind_null(insertStatement, 5);
    else
        sqlite3_bind_blob(insertStatement, 5, parsedBook.cover.constData(), (int)parsedBook.cover.size(), SQLITE_TRANSIENT);

    sqlite3_bind_text(insertStatement, 6, plainTextUtf8.constData(), (int)plainTextUtf8.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int64(insertStatement, 7, createdAt);

    if (sqlite3_step(insertStatement) != SQLITE_DONE)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        sqlite3_finalize(insertStatement);
        return 0;
    }
    sqlite3_finalize(insertStatement);

    // Получаем id книги
    int bookId = 0;
    {
        sqlite3_stmt* selectIdStatement = nullptr;
        const char* selectIdSql = "SELECT id FROM books WHERE file_path=?;";
        if (sqlite3_prepare_v2(db, selectIdSql, -1, &selectIdStatement, nullptr) != SQLITE_OK)
        {
            if (errorOut) *errorOut = sqliteErr(db);
            return 0;
        }
        sqlite3_bind_text(selectIdStatement, 1, filePathUtf8.constData(), (int)filePathUtf8.size(), SQLITE_TRANSIENT);
        if (sqlite3_step(selectIdStatement) == SQLITE_ROW)
            bookId = sqlite3_column_int(selectIdStatement, 0);
        sqlite3_finalize(selectIdStatement);
    }

    if (!bookId)
    {
        if (errorOut) *errorOut = "Не удалось получить id книги из базы.";
        return 0;
    }

    // Перезаписываем главы
    {
        sqlite3_stmt* deleteStatement = nullptr;
        if (sqlite3_prepare_v2(db, "DELETE FROM chapters WHERE book_id=?;", -1, &deleteStatement, nullptr) != SQLITE_OK)
        {
            if (errorOut) *errorOut = sqliteErr(db);
            return bookId;
        }
        sqlite3_bind_int(deleteStatement, 1, bookId);
        sqlite3_step(deleteStatement);
        sqlite3_finalize(deleteStatement);

        sqlite3_stmt* insertChapter = nullptr;
        if (sqlite3_prepare_v2(db,
            "INSERT INTO chapters(book_id,idx,title,start_offset) VALUES(?,?,?,?);",
            -1, &insertChapter, nullptr) != SQLITE_OK)
        {
            if (errorOut) *errorOut = sqliteErr(db);
            return bookId;
        }

        for (const auto& chapter : parsedBook.chapters)
        {
            const QByteArray chapterTitleUtf8 = chapter.title.toUtf8();
            sqlite3_bind_int(insertChapter, 1, bookId);
            sqlite3_bind_int(insertChapter, 2, chapter.index);
            sqlite3_bind_text(insertChapter, 3, chapterTitleUtf8.constData(), (int)chapterTitleUtf8.size(), SQLITE_TRANSIENT);
            sqlite3_bind_int(insertChapter, 4, chapter.startOffset);
            if (sqlite3_step(insertChapter) != SQLITE_DONE)
            {
                if (errorOut) *errorOut = sqliteErr(db);
                sqlite3_finalize(insertChapter);
                return bookId;
            }
            sqlite3_reset(insertChapter);
        }
        sqlite3_finalize(insertChapter);
    }

    // Сохраняем все картинки книги в таблицу book_images
    {
        // Сначала удаляем старые картинки
        sqlite3_stmt* deleteImages = nullptr;
        if (sqlite3_prepare_v2(db, "DELETE FROM book_images WHERE book_id=?;", -1, &deleteImages, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_int(deleteImages, 1, bookId);
            sqlite3_step(deleteImages);
            sqlite3_finalize(deleteImages);
        }

        sqlite3_stmt* insertImage = nullptr;
        if (sqlite3_prepare_v2(db,
            "INSERT OR REPLACE INTO book_images(book_id,image_id,content_type,data) VALUES(?,?,?,?);",
            -1, &insertImage, nullptr) == SQLITE_OK)
        {
            for (auto it = parsedBook.binaries.constBegin(); it != parsedBook.binaries.constEnd(); ++it)
            {
                const QByteArray imageIdUtf8 = it.key().toUtf8();
                const QString    contentType = parsedBook.binaryTypes.value(it.key(), "image/jpeg");
                const QByteArray contentTypeUtf8 = contentType.toUtf8();
                const QByteArray& imageData = it.value();

                sqlite3_bind_int(insertImage, 1, bookId);
                sqlite3_bind_text(insertImage, 2, imageIdUtf8.constData(), (int)imageIdUtf8.size(), SQLITE_TRANSIENT);
                sqlite3_bind_text(insertImage, 3, contentTypeUtf8.constData(), (int)contentTypeUtf8.size(), SQLITE_TRANSIENT);
                sqlite3_bind_blob(insertImage, 4, imageData.constData(), (int)imageData.size(), SQLITE_TRANSIENT);

                sqlite3_step(insertImage);
                sqlite3_reset(insertImage);
            }
            sqlite3_finalize(insertImage);
        }
    }

    // Создаём настройки по умолчанию если их нет
    ReaderSettings defaultSettings;
    QString settingsError;
    if (!getSettings(bookId, &defaultSettings, &settingsError))
        upsertSettings(bookId, defaultSettings, nullptr);

    return bookId;
}

// Загрузить картинку книги по её id (например "cover.jpg")
QByteArray Database::getBookImage(int bookId, const QString& imageId, QString* errorOut) const
{
    QByteArray result;
    if (!db) return result;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT data FROM book_images WHERE book_id=? AND image_id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return result;
    }

    const QByteArray imageIdUtf8 = imageId.toUtf8();
    sqlite3_bind_int(stmt, 1, bookId);
    sqlite3_bind_text(stmt, 2, imageIdUtf8.constData(), (int)imageIdUtf8.size(), SQLITE_TRANSIENT);

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

// Загрузить сырые байты файла книги из БД
QByteArray Database::getBookFileData(int bookId, QString* errorOut) const
{
    QByteArray result;
    if (!db) return result;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT file_data FROM books WHERE id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
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

QVector<DbBook> Database::listBooks(QString* errorOut) const
{
    QVector<DbBook> result;
    if (!db) return result;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id,file_path,title,author,total_pages,last_page "
        "FROM books ORDER BY created_at DESC;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        DbBook book;
        book.id = sqlite3_column_int(stmt, 0);
        book.filePath = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 1));
        book.title = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 2));
        book.author = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 3));
        book.totalPages = sqlite3_column_int(stmt, 4);
        book.lastPage = sqlite3_column_int(stmt, 5);
        result.push_back(book);
    }
    sqlite3_finalize(stmt);
    return result;
}

QVector<DbBook> Database::listRecentlyAdded(int limit, QString* errorOut) const
{
    QVector<DbBook> result;
    if (!db) return result;

    const QString queryStr = QString(
        "SELECT id,file_path,title,author,total_pages,last_page "
        "FROM books ORDER BY created_at DESC %1;")
        .arg(limit > 0 ? QString("LIMIT %1").arg(limit) : "");

    sqlite3_stmt* stmt = nullptr;
    const QByteArray queryUtf8 = queryStr.toUtf8();
    if (sqlite3_prepare_v2(db, queryUtf8.constData(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        DbBook book;
        book.id = sqlite3_column_int(stmt, 0);
        book.filePath = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 1));
        book.title = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 2));
        book.author = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 3));
        book.totalPages = sqlite3_column_int(stmt, 4);
        book.lastPage = sqlite3_column_int(stmt, 5);
        result.push_back(book);
    }
    sqlite3_finalize(stmt);
    return result;
}

QVector<DbBook> Database::listRecentlyRead(int limit, QString* errorOut) const
{
    QVector<DbBook> result;
    if (!db) return result;

    const QString queryStr = QString(
        "SELECT id,file_path,title,author,total_pages,last_page "
        "FROM books WHERE last_read_at > 0 "
        "ORDER BY last_read_at DESC %1;")
        .arg(limit > 0 ? QString("LIMIT %1").arg(limit) : "");

    sqlite3_stmt* stmt = nullptr;
    const QByteArray queryUtf8 = queryStr.toUtf8();
    if (sqlite3_prepare_v2(db, queryUtf8.constData(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        DbBook book;
        book.id = sqlite3_column_int(stmt, 0);
        book.filePath = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 1));
        book.title = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 2));
        book.author = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 3));
        book.totalPages = sqlite3_column_int(stmt, 4);
        book.lastPage = sqlite3_column_int(stmt, 5);
        result.push_back(book);
    }
    sqlite3_finalize(stmt);
    return result;
}

bool Database::getBook(int bookId, DbBook* out, QString* errorOut) const
{
    if (!db || !out) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id,file_path,title,author,total_pages,last_page "
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
        out->filePath = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 1));
        out->title = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 2));
        out->author = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 3));
        out->totalPages = sqlite3_column_int(stmt, 4);
        out->lastPage = sqlite3_column_int(stmt, 5);
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
        DbChapter chapter;
        chapter.id = sqlite3_column_int(stmt, 0);
        chapter.bookId = sqlite3_column_int(stmt, 1);
        chapter.index = sqlite3_column_int(stmt, 2);
        chapter.title = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 3));
        chapter.startOffset = sqlite3_column_int(stmt, 4);
        result.push_back(chapter);
    }
    sqlite3_finalize(stmt);
    return result;
}

bool Database::getParsedText(int bookId, QString* out, QString* errorOut) const
{
    if (!db || !out) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT parsed_text FROM books WHERE id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return false;
    }
    sqlite3_bind_int(stmt, 1, bookId);
    const bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    if (found)
        *out = QString::fromUtf8((const char*)sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    if (!found && errorOut) *errorOut = "Текст книги не найден.";
    return found;
}

bool Database::getSettings(int bookId, ReaderSettings* out, QString* errorOut) const
{
    if (!db || !out) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT font_family,font_size,align,line_height "
        "FROM reader_settings WHERE book_id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
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
    if (!found && errorOut) *errorOut = "Настройки не найдены.";
    return found;
}

bool Database::upsertSettings(int bookId, const ReaderSettings& settings, QString* errorOut)
{
    if (!openOrCreate())
    {
        if (errorOut) *errorOut = "Не удалось открыть базу данных.";
        return false;
    }

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

    const QByteArray fontFamilyUtf8 = settings.fontFamily.toUtf8();
    sqlite3_bind_int(stmt, 1, bookId);
    sqlite3_bind_text(stmt, 2, fontFamilyUtf8.constData(), (int)fontFamilyUtf8.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, settings.fontSize);
    sqlite3_bind_int(stmt, 4, settings.align);
    sqlite3_bind_double(stmt, 5, settings.lineHeight);

    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && errorOut) *errorOut = sqliteErr(db);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::updateLastPage(int bookId, int lastPage, QString* errorOut)
{
    if (!openOrCreate())
    {
        if (errorOut) *errorOut = "Не удалось открыть базу данных.";
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE books SET last_page=?, last_read_at=? WHERE id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return false;
    }
    sqlite3_bind_int(stmt, 1, lastPage);
    sqlite3_bind_int64(stmt, 2, QDateTime::currentSecsSinceEpoch());
    sqlite3_bind_int(stmt, 3, bookId);
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && errorOut) *errorOut = sqliteErr(db);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::updateTotalPages(int bookId, int totalPages, QString* errorOut)
{
    if (!openOrCreate())
    {
        if (errorOut) *errorOut = "Не удалось открыть базу данных.";
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE books SET total_pages=? WHERE id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (errorOut) *errorOut = sqliteErr(db);
        return false;
    }
    sqlite3_bind_int(stmt, 1, totalPages);
    sqlite3_bind_int(stmt, 2, bookId);
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && errorOut) *errorOut = sqliteErr(db);
    sqlite3_finalize(stmt);
    return ok;
}

////////////////////////////////////////DATABASE.H
#pragma once

#include <QString>
#include <QByteArray>
#include <QVector>

struct sqlite3;

struct DbBook
{
    int id = 0;
    QString filePath;
    QString title;
    QString author;
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

    int addOrUpdateBookFromFb2File(const QString& filePath, QString* errorOut = nullptr);

    QVector<DbBook> listBooks(QString* errorOut = nullptr) const;
    QVector<DbBook> listRecentlyAdded(int limit = 0, QString* errorOut = nullptr) const;
    QVector<DbBook> listRecentlyRead(int limit = 0, QString* errorOut = nullptr) const;
    bool getBook(int bookId, DbBook* out, QString* errorOut = nullptr) const;
    QVector<DbChapter> listChapters(int bookId, QString* errorOut = nullptr) const;

    bool getParsedText(int bookId, QString* out, QString* errorOut = nullptr) const;

    // Новые методы:
    QByteArray getBookImage(int bookId, const QString& imageId, QString* errorOut = nullptr) const;
    QByteArray getBookFileData(int bookId, QString* errorOut = nullptr) const;

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

private:
    sqlite3* db = nullptr;
};

*/