#include "fb2metaparser.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QXmlStreamReader>

static QString joinName(const QString& first, const QString& middle, const QString& last)
{
    QStringList parts;
    if (!last.trimmed().isEmpty())   parts << last.trimmed();
    if (!first.trimmed().isEmpty())  parts << first.trimmed();
    if (!middle.trimmed().isEmpty()) parts << middle.trimmed();
    return parts.join(' ');
}

static QString imageHref(const QXmlStreamAttributes& attributes)
{
    QString href = attributes.value("l:href").toString();
    if (href.isEmpty())
        href = attributes.value("href").toString();
    if (href.isEmpty())
        href = attributes.value("http://www.w3.org/1999/xlink", "href").toString();
    return href;
}

bool Fb2MetaParser::parseFile(const QString& filePath, Fb2Meta* out, QString* errorOut)
{
    if (!out) return false;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (errorOut) *errorOut = "Не удалось открыть файл.";
        return false;
    }

    out->rawFileData = f.readAll();
    f.seek(0);

    QXmlStreamReader xml(&f);

    // --- состояние ---

    // Нас интересует только первый <title-info>, игнорируем <src-title-info>
    bool inTitleInfo = false;
    bool titleInfoDone = false; // первый title-info уже прочитан
    bool inSrcTitleInfo = false; // игнорируем

    bool inAuthor = false;
    bool inTranslator = false;
    bool inAnnotation = false;
    bool inCoverpage = false;
    bool inBinary = false;

    bool inBookTitle = false;
    bool inGenre = false;
    bool inFirst = false;
    bool inLast = false;
    bool inMiddle = false;
    bool inP = false;

    QString currentText;
    QString authorFirst, authorLast, authorMiddle;
    QString translatorFirst, translatorLast, translatorMiddle;
    QString annotationText;
    QString coverRef;
    QString binaryId, binaryContentType;

    // Глубина вложенности — чтобы корректно закрывать <annotation>
    // (внутри может быть несколько <p>, <emphasis> и т.д.)
    int annotationDepth = 0;

    while (!xml.atEnd())
    {
        xml.readNext();

        // ======= START ELEMENT =======
        if (xml.isStartElement())
        {
            const QString name = xml.name().toString();

            // Как только встречаем <body> — метаданные закончились
            if (name == "body") break;

            // Входим в первый title-info
            if (name == "title-info" && !titleInfoDone)
            {
                inTitleInfo = true;
                continue;
            }

            // src-title-info и document-info — пропускаем
            if (name == "src-title-info") { inSrcTitleInfo = true; continue; }
            if (inSrcTitleInfo) continue;

            // Всё ниже — только внутри title-info
            if (!inTitleInfo)
            {
                // binary может быть в любом месте файла (обычно в конце)
                if (name == "binary")
                {
                    inBinary = true;
                    binaryId = xml.attributes().value("id").toString();
                    binaryContentType = xml.attributes().value("content-type").toString();
                    currentText.clear();
                }
                continue;
            }

            if (name == "book-title") { inBookTitle = true; currentText.clear(); }
            else if (name == "genre" && out->genre.isEmpty()) { inGenre = true; currentText.clear(); }
            else if (name == "author" && !inAuthor && !inTranslator)
            {
                inAuthor = true;
                authorFirst.clear(); authorLast.clear(); authorMiddle.clear();
            }
            else if (name == "translator")
            {
                inTranslator = true;
                translatorFirst.clear(); translatorLast.clear(); translatorMiddle.clear();
            }
            else if (name == "annotation")
            {
                inAnnotation = true;
                annotationDepth = 1;
                annotationText.clear();
            }
            else if (name == "coverpage") { inCoverpage = true; }
            else if (name == "image" && inCoverpage)
            {
                const QString href = imageHref(xml.attributes());
                if (!href.isEmpty()) coverRef = href;
            }
            else if ((inAuthor || inTranslator) && name == "first-name") { inFirst = true;  currentText.clear(); }
            else if ((inAuthor || inTranslator) && name == "last-name") { inLast = true;   currentText.clear(); }
            else if ((inAuthor || inTranslator) && name == "middle-name") { inMiddle = true; currentText.clear(); }
            else if (inAnnotation && name == "p") { inP = true; currentText.clear(); }
            else if (inAnnotation) { annotationDepth++; }
        }

        // ======= CHARACTERS =======
        else if (xml.isCharacters() && !xml.isWhitespace())
        {
            if (inBookTitle || inFirst || inLast || inMiddle || inGenre || inP || inBinary)
                currentText += xml.text().toString();
        }

        // ======= END ELEMENT =======
        else if (xml.isEndElement())
        {
            const QString name = xml.name().toString();

            if (name == "src-title-info") { inSrcTitleInfo = false; continue; }
            if (inSrcTitleInfo) continue;

            if (name == "binary")
            {
                inBinary = false;
                if (!binaryId.isEmpty())
                {
                    const QByteArray decoded = QByteArray::fromBase64(
                        currentText.toLatin1(), QByteArray::IgnoreBase64DecodingErrors
                    );
                    if (!decoded.isEmpty())
                    {
                        out->binaries.insert(binaryId, decoded);
                        if (!out->binaryTypes.contains(binaryId))
                            out->binaryTypes.insert(binaryId, binaryContentType);
                    }
                }
                continue;
            }

            if (!inTitleInfo) continue;

            if (name == "title-info")
            {
                inTitleInfo = false;
                titleInfoDone = true;
                // Не break — ещё нужно собрать binary из конца файла
                continue;
            }

            if (name == "book-title") { inBookTitle = false; out->title = currentText.trimmed(); }
            else if (name == "genre") { inGenre = false; }
            else if (name == "first-name")
            {
                inFirst = false;
                (inAuthor ? authorFirst : translatorFirst) = currentText.trimmed();
            }
            else if (name == "last-name")
            {
                inLast = false;
                (inAuthor ? authorLast : translatorLast) = currentText.trimmed();
            }
            else if (name == "middle-name")
            {
                inMiddle = false;
                (inAuthor ? authorMiddle : translatorMiddle) = currentText.trimmed();
            }
            else if (name == "author" && inAuthor)
            {
                inAuthor = false;
                // Берём только первого автора (если несколько)
                if (out->author.isEmpty())
                    out->author = joinName(authorFirst, authorMiddle, authorLast);
            }
            else if (name == "translator" && inTranslator)
            {
                inTranslator = false;
                if (out->translator.isEmpty())
                    out->translator = joinName(translatorFirst, translatorMiddle, translatorLast);
            }
            else if (name == "p" && inP)
            {
                inP = false;
                if (!annotationText.isEmpty()) annotationText += "\n";
                annotationText += currentText.trimmed();
            }
            else if (name == "annotation")
            {
                inAnnotation = false;
                out->annotation = annotationText.trimmed();
            }
            else if (name == "coverpage") { inCoverpage = false; }
        }
    }

    // Собираем binary по всему файлу: обложка в FB2 часто объявлена в title-info,
    // а сами байты лежат в конце после body.
    if (out->cover.isEmpty() && !coverRef.isEmpty())
    {
        const QString coverId = coverRef.startsWith('#') ? coverRef.mid(1) : coverRef;

        QBuffer binaryBuffer;
        binaryBuffer.setData(out->rawFileData);
        binaryBuffer.open(QIODevice::ReadOnly);
        QXmlStreamReader binaryXml(&binaryBuffer);

        while (!binaryXml.atEnd())
        {
            binaryXml.readNext();
            if (!binaryXml.isStartElement() || binaryXml.name().toString() != "binary")
                continue;

            const QString binaryId = binaryXml.attributes().value("id").toString();
            const QString contentType = binaryXml.attributes().value("content-type").toString();
            QString encoded;

            while (!binaryXml.atEnd())
            {
                binaryXml.readNext();
                if (binaryXml.isCharacters())
                    encoded += binaryXml.text().toString();
                else if (binaryXml.isEndElement() && binaryXml.name().toString() == "binary")
                    break;
            }

            const QByteArray decoded = QByteArray::fromBase64(
                encoded.toLatin1(), QByteArray::IgnoreBase64DecodingErrors
            );
            if (!binaryId.isEmpty() && !decoded.isEmpty())
            {
                out->binaries.insert(binaryId, decoded);
                out->binaryTypes.insert(binaryId, contentType.isEmpty() ? "image/jpeg" : contentType);
            }
        }

        out->cover = out->binaries.value(coverId);
    }

    if (out->title.isEmpty())
        out->title = QFileInfo(filePath).baseName();
    if (out->author.trimmed().isEmpty())
        out->author = "Неизвестный автор";

    return true;
}
