#include "fb2textparser.h"

#include <QXmlStreamReader>
#include <QBuffer>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

static QString cacheFilePath(int bookId, const QString& cacheDir)
{
    return QDir(cacheDir).filePath(QString("%1.json").arg(bookId));
}

bool Fb2TextParser::isCached(int bookId, const QString& cacheDir)
{
    return QFile::exists(cacheFilePath(bookId, cacheDir));
}

void Fb2TextParser::clearCache(const QString& cacheDir)
{
    QDir dir(cacheDir);
    const auto files = dir.entryList({ "*.json" }, QDir::Files);
    for (const auto& f : files)
        dir.remove(f);
}

QString Fb2TextParser::parseAndCache(
    int bookId,
    const QByteArray& fb2Data,
    const QString& cacheDir,
    QString* errorOut)
{
    QDir().mkpath(cacheDir);

    QBuffer buf;
    buf.setData(fb2Data);
    buf.open(QIODevice::ReadOnly);
    QXmlStreamReader xml(&buf);

    QVector<ParsedChapter> chapters;
    ParsedChapter* current = nullptr;

    bool inBody = false;
    bool inSection = false;
    bool inTitle = false;
    bool inP = false;
    QString currentText;
    QString lastTitleText;
    int chapterIndex = 0;

    auto flushParagraph = [&](const QString& text) {
        if (!current || text.trimmed().isEmpty()) return;
        if (!current->text.isEmpty()) current->text += "\n\n";
        current->text += text.trimmed();
        };

    while (!xml.atEnd())
    {
        xml.readNext();

        if (xml.isStartElement())
        {
            const QString name = xml.name().toString();

            if (name == "body") { inBody = true; }
            else if (inBody && name == "section")
            {
                inSection = true;
                chapters.append(ParsedChapter());
                current = &chapters.last();
                current->index = chapterIndex++;
            }
            else if (inSection && name == "title") { inTitle = true; lastTitleText.clear(); }
            else if (inTitle && name == "p") { inP = true; currentText.clear(); }
            else if (inSection && !inTitle && name == "p") { inP = true; currentText.clear(); }
            else if (inSection && name == "image")
            {
                const QString href = xml.attributes().value("l:href").toString();
                if (!href.isEmpty() && current)
                {
                    const QString id = href.startsWith('#') ? href.mid(1) : href;
                    current->imageIds.append(id);
                    flushParagraph(QString("[[IMAGE:%1]]").arg(id));
                }
            }
        }
        else if (xml.isCharacters() && !xml.isWhitespace())
        {
            if (inP) currentText += xml.text().toString();
        }
        else if (xml.isEndElement())
        {
            const QString name = xml.name().toString();

            if (name == "p" && inP)
            {
                inP = false;
                if (inTitle)
                    lastTitleText = currentText.trimmed();
                else
                    flushParagraph(currentText);
            }
            else if (name == "title" && inTitle)
            {
                inTitle = false;
                if (current && !lastTitleText.isEmpty())
                {
                    current->title = lastTitleText;
                    // вставляем заголовок в текст главы
                    if (!current->text.isEmpty()) current->text.prepend("\n\n");
                    current->text.prepend(lastTitleText);
                }
            }
            else if (name == "section") { inSection = false; current = nullptr; }
            else if (name == "body") { inBody = false; }
        }
    }

    if (xml.hasError())
    {
        if (errorOut) *errorOut = "Ошибка XML: " + xml.errorString();
        return {};
    }

    // Если глав нет — одна глава со всем текстом уже собрана как есть
    if (chapters.isEmpty())
    {
        ParsedChapter ch;
        ch.index = 0;
        ch.title = "Без названия";
        chapters.append(ch);
    }

    // Сериализуем в JSON
    QJsonArray arr;
    for (const auto& ch : chapters)
    {
        QJsonObject obj;
        obj["index"] = ch.index;
        obj["title"] = ch.title;
        obj["text"] = ch.text;
        QJsonArray ids;
        for (const auto& id : ch.imageIds) ids.append(id);
        obj["imageIds"] = ids;
        arr.append(obj);
    }

    const QString outPath = cacheFilePath(bookId, cacheDir);
    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly))
    {
        if (errorOut) *errorOut = "Не удалось записать кэш: " + outPath;
        return {};
    }
    outFile.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    return outPath;
}

QVector<ParsedChapter> Fb2TextParser::loadFromCache(
    int bookId,
    const QString& cacheDir,
    QString* errorOut)
{
    QVector<ParsedChapter> result;
    QFile f(cacheFilePath(bookId, cacheDir));
    if (!f.open(QIODevice::ReadOnly))
    {
        if (errorOut) *errorOut = "Кэш не найден.";
        return result;
    }

    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const auto& val : arr)
    {
        const QJsonObject obj = val.toObject();
        ParsedChapter ch;
        ch.index = obj["index"].toInt();
        ch.title = obj["title"].toString();
        ch.text = obj["text"].toString();
        for (const auto& id : obj["imageIds"].toArray())
            ch.imageIds.append(id.toString());
        result.append(ch);
    }
    return result;
}