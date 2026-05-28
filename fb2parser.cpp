// fb2parser.cpp

#include "fb2parser.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStringList>
#include <QXmlStreamReader>

static QString joinName(const QString& first, const QString& middle, const QString& last)
{
    QStringList parts;

    if (!last.trimmed().isEmpty())
        parts << last.trimmed();

    if (!first.trimmed().isEmpty())
        parts << first.trimmed();

    if (!middle.trimmed().isEmpty())
        parts << middle.trimmed();

    return parts.join(' ');
}

// Экранируем текст книги перед вставкой в HTML, иначе символы '<', '>', '&'
// сломают разметку (и потенциально позволят инъекцию из содержимого файла).
static QString escapeHtml(const QString& text)
{
    QString out = text;
    out.replace('&', "&amp;");
    out.replace('<', "&lt;");
    out.replace('>', "&gt;");
    return out;
}

// FB2 хранит ссылку на картинку в атрибуте xlink:href / l:href.
// При включённой обработке пространств имён локальное имя атрибута — "href".
static QString hrefAttr(const QXmlStreamReader& xml)
{
    const auto attrs = xml.attributes();
    for (const auto& a : attrs)
    {
        if (a.name() == QLatin1String("href"))
            return a.value().toString();
    }
    return QString();
}

// Тип текущего HTML-блока, который мы собираем внутри главы.
enum class Block
{
    None,
    Paragraph, // <p>
    Subtitle,  // <subtitle>
    Verse      // <v>
};

bool Fb2Parser::parseFile(const QString& filePath, ParsedBook* out, QString* errorOut)
{
    if (!out)
        return false;

    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorOut)
            *errorOut = "Не удалось открыть файл";

        return false;
    }

    QXmlStreamReader xml(&file);

    // ===== метаданные =====
    bool inTitleInfo = false;

    bool inAuthor = false;
    bool inTranslator = false;

    bool inFirstName = false;
    bool inLastName = false;
    bool inMiddleName = false;

    bool inAnnotation = false;

    QString currentText; // текст внутри простого тега (мета / заголовок / binary)

    QString authorFirst, authorLast, authorMiddle;
    QString translatorFirst, translatorLast, translatorMiddle;
    QString annotationText;

    // ===== обложка / картинки =====
    bool inCoverpage = false;
    bool inBinary = false;
    QString binaryId;
    QString coverRef;
    QHash<QString, QByteArray> binaries;

    // ===== текст =====
    bool inBody = false;
    bool inSection = false;
    bool inTitle = false;

    Block block = Block::None; // текущий собираемый блок текста главы
    QString blockHtml;         // накопленный inline-HTML текущего блока
    bool blockHasText = false; // были ли непустые символы в блоке

    QString titleText;

    ParsedChapter* currentChapter = nullptr;
    int chapterIndex = 0;

    auto addBlock = [&](const QString& html)
        {
            if (currentChapter)
                currentChapter->text += html;
        };

    while (!xml.atEnd())
    {
        xml.readNext();

        if (xml.isStartElement())
        {
            const QString name = xml.name().toString();

            if (name == "title-info")
                inTitleInfo = true;

            if (inTitleInfo)
            {
                if (name == "book-title" || name == "genre"
                    || name == "first-name" || name == "last-name"
                    || name == "middle-name")
                {
                    currentText.clear();

                    if (name == "first-name") inFirstName = true;
                    else if (name == "last-name") inLastName = true;
                    else if (name == "middle-name") inMiddleName = true;
                }
                else if (name == "author")
                {
                    inAuthor = true;
                    authorFirst.clear();
                    authorLast.clear();
                    authorMiddle.clear();
                }
                else if (name == "translator")
                {
                    inTranslator = true;
                    translatorFirst.clear();
                    translatorLast.clear();
                    translatorMiddle.clear();
                }
                else if (name == "annotation")
                {
                    inAnnotation = true;
                    annotationText.clear();
                }
            }

            // ----- обложка -----
            if (name == "coverpage")
            {
                inCoverpage = true;
            }
            else if (name == "image")
            {
                if (inCoverpage && coverRef.isEmpty())
                    coverRef = hrefAttr(xml);
            }
            else if (name == "binary")
            {
                inBinary = true;
                binaryId = xml.attributes().value("id").toString();
                currentText.clear();
            }

            // ----- текст книги -----
            if (name == "body")
            {
                inBody = true;
            }
            else if (inBody && name == "section")
            {
                inSection = true;

                ParsedChapter chapter;
                chapter.index = chapterIndex++;
                chapter.title = QString("Глава %1").arg(chapter.index + 1);

                out->chapters.append(chapter);
                currentChapter = &out->chapters.last();
            }
            else if (inSection && name == "title")
            {
                inTitle = true;
                titleText.clear();
            }
            else if (name == "p")
            {
                currentText.clear();

                if (!inTitle && !inAnnotation && currentChapter)
                {
                    block = Block::Paragraph;
                    blockHtml.clear();
                    blockHasText = false;
                }
            }
            else if (name == "subtitle" && currentChapter && !inTitle)
            {
                block = Block::Subtitle;
                blockHtml.clear();
                blockHasText = false;
            }
            else if (name == "v" && currentChapter)
            {
                block = Block::Verse;
                blockHtml.clear();
                blockHasText = false;
            }
            else if (name == "empty-line" && currentChapter)
            {
                addBlock("<p>&nbsp;</p>");
            }
            else if (name == "emphasis")
            {
                if (block != Block::None) blockHtml += "<i>";
            }
            else if (name == "strong")
            {
                if (block != Block::None) blockHtml += "<b>";
            }
        }

        else if (xml.isCharacters() && !xml.isWhitespace())
        {
            const QString t = xml.text().toString();
            currentText += t;

            if (block != Block::None)
            {
                blockHtml += escapeHtml(t);
                if (!t.trimmed().isEmpty())
                    blockHasText = true;
            }
        }

        else if (xml.isEndElement())
        {
            const QString name = xml.name().toString();

            if (name == "title-info")
            {
                inTitleInfo = false;
            }
            else if (name == "book-title")
            {
                out->meta.title = currentText.trimmed();
            }
            else if (name == "genre")
            {
                out->meta.genre = currentText.trimmed();
            }
            else if (name == "first-name")
            {
                inFirstName = false;
                if (inAuthor) authorFirst = currentText.trimmed();
                else if (inTranslator) translatorFirst = currentText.trimmed();
            }
            else if (name == "last-name")
            {
                inLastName = false;
                if (inAuthor) authorLast = currentText.trimmed();
                else if (inTranslator) translatorLast = currentText.trimmed();
            }
            else if (name == "middle-name")
            {
                inMiddleName = false;
                if (inAuthor) authorMiddle = currentText.trimmed();
                else if (inTranslator) translatorMiddle = currentText.trimmed();
            }
            else if (name == "author")
            {
                inAuthor = false;
                if (out->meta.author.isEmpty())
                    out->meta.author = joinName(authorFirst, authorMiddle, authorLast);
            }
            else if (name == "translator")
            {
                inTranslator = false;
                if (out->meta.translator.isEmpty())
                    out->meta.translator = joinName(translatorFirst, translatorMiddle, translatorLast);
            }
            else if (name == "annotation")
            {
                inAnnotation = false;
                out->meta.annotation = annotationText.trimmed();
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
                    const QByteArray decoded =
                        QByteArray::fromBase64(currentText.toUtf8());
                    if (!decoded.isEmpty())
                        binaries.insert(binaryId, decoded);
                }
            }
            else if (name == "emphasis")
            {
                if (block != Block::None) blockHtml += "</i>";
            }
            else if (name == "strong")
            {
                if (block != Block::None) blockHtml += "</b>";
            }
            else if (name == "p")
            {
                if (inAnnotation)
                {
                    if (!annotationText.isEmpty())
                        annotationText += "\n";
                    annotationText += currentText.trimmed();
                }
                else if (inTitle)
                {
                    titleText = currentText.trimmed();
                }
                else if (block == Block::Paragraph)
                {
                    if (blockHasText)
                        addBlock("<p>" + blockHtml + "</p>");
                    block = Block::None;
                }
            }
            else if (name == "subtitle")
            {
                if (block == Block::Subtitle)
                {
                    if (blockHasText)
                        addBlock("<p align=\"center\"><b>" + blockHtml + "</b></p>");
                    block = Block::None;
                }
            }
            else if (name == "v")
            {
                if (block == Block::Verse)
                {
                    addBlock("<p>" + blockHtml + "</p>");
                    block = Block::None;
                }
            }
            else if (name == "title")
            {
                inTitle = false;
                if (currentChapter && !titleText.isEmpty())
                    currentChapter->title = titleText;
            }
            else if (name == "section")
            {
                inSection = false;
                currentChapter = nullptr;
            }
            else if (name == "body")
            {
                inBody = false;
            }

            // Не сбрасываем currentText при закрытии inline-тегов внутри абзаца,
            // иначе потеряем текст вокруг <emphasis>/<strong>.
            if (name != "emphasis" && name != "strong")
                currentText.clear();
        }
    }

    if (xml.hasError())
    {
        if (errorOut)
            *errorOut = "Ошибка XML: " + xml.errorString();

        return false;
    }

    // Обложка: id из coverRef (убираем ведущий '#')
    if (!coverRef.isEmpty())
    {
        const QString coverId =
            coverRef.startsWith('#') ? coverRef.mid(1) : coverRef;
        out->meta.cover = binaries.value(coverId);
    }

    if (out->meta.title.isEmpty())
        out->meta.title = QFileInfo(filePath).baseName();

    if (out->meta.author.isEmpty())
        out->meta.author = "Неизвестный автор";

    // Вложенные <section> создают главу-обёртку без собственного текста.
    // Удаляем такие пустые главы и перенумеровываем оставшиеся.
    for (int i = out->chapters.size() - 1; i >= 0; --i)
    {
        if (out->chapters[i].text.trimmed().isEmpty())
            out->chapters.removeAt(i);
    }
    for (int i = 0; i < out->chapters.size(); ++i)
        out->chapters[i].index = i;

    if (out->chapters.isEmpty())
    {
        ParsedChapter chapter;
        chapter.index = 0;
        chapter.title = "Без названия";
        chapter.text = "<p>Текст книги не найден</p>";
        out->chapters.append(chapter);
    }

    return true;
}
