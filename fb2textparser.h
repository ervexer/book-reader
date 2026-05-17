#pragma once

#include <QString>
#include <QVector>
#include <QByteArray>

// Маркер разрыва главы — вставляется в начало текста каждой главы (кроме первой).
// ReaderWindow использует его для принудительного перехода на новую страницу.
extern const QString CHAPTER_BREAK_MARKER;

struct ParsedChapter
{
    int     index = 0;
    QString title;
    QString text;
    QVector<QString> imageIds;
};

class Fb2TextParser
{
public:
    static bool    isCached(int bookId, const QString& cacheDir);
    static void    clearCache(const QString& cacheDir);

    // Парсит FB2 из памяти и сохраняет JSON-кэш. Возвращает путь к файлу или пустую строку при ошибке.
    static QString parseAndCache(int bookId, const QByteArray& fb2Data,
        const QString& cacheDir, QString* errorOut = nullptr);

    // Загружает главы из JSON-кэша.
    static QVector<ParsedChapter> loadFromCache(int bookId,
        const QString& cacheDir,
        QString* errorOut = nullptr);
};