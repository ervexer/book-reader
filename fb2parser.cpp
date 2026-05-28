// fb2parser.cpp

#include "fb2parser.h"

#include <QFile>
#include <QFileInfo>
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

    // метаданные

    bool inTitleInfo = false;

    bool inAuthor = false;
    bool inTranslator = false;

    bool inFirstName = false;
    bool inLastName = false;
    bool inMiddleName = false;

    bool inAnnotation = false;

    QString currentText;

    QString authorFirst;
    QString authorLast;
    QString authorMiddle;

    QString translatorFirst;
    QString translatorLast;
    QString translatorMiddle;

    QString annotationText;

    // текст

    bool inBody = false;
    bool inSection = false;
    bool inTitle = false;
    bool inParagraph = false;

    QString titleText;

    ParsedChapter* currentChapter = nullptr;

    int chapterIndex = 0;

    auto appendParagraph = [&](const QString& text)
        {
            if (!currentChapter)
                return;

            QString trimmed = text.trimmed();

            if (trimmed.isEmpty())
                return;

            if (!currentChapter->text.isEmpty())
                currentChapter->text += "\n\n";

            currentChapter->text += trimmed;
        };

    while (!xml.atEnd())
    {
        xml.readNext();

        if (xml.isStartElement())
        {
            QString name = xml.name().toString();

            if (name == "title-info")
            {
                inTitleInfo = true;
            }

            if (inTitleInfo)
            {
                if (name == "book-title")
                {
                    currentText.clear();
                }
                else if (name == "genre")
                {
                    currentText.clear();
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
                else if (name == "first-name")
                {
                    inFirstName = true;
                    currentText.clear();
                }
                else if (name == "last-name")
                {
                    inLastName = true;
                    currentText.clear();
                }
                else if (name == "middle-name")
                {
                    inMiddleName = true;
                    currentText.clear();
                }
                else if (name == "annotation")
                {
                    inAnnotation = true;
                    annotationText.clear();
                }
            }

            if (name == "body")
            {
                inBody = true;
            }
            else if (inBody && name == "section")
            {
                inSection = true;

                ParsedChapter chapter;

                chapter.index = chapterIndex++;

                chapter.title =
                    QString("Глава %1")
                    .arg(chapter.index + 1);

                out->chapters.append(chapter);

                currentChapter =
                    &out->chapters.last();
            }
            else if (inSection && name == "title")
            {
                inTitle = true;
                titleText.clear();
            }
            else if (name == "p")
            {
                inParagraph = true;
                currentText.clear();
            }
        }

        else if (xml.isCharacters() && !xml.isWhitespace())
        {
            currentText += xml.text().toString();
        }

        else if (xml.isEndElement())
        {
            QString name = xml.name().toString();

            if (name == "title-info")
            {
                inTitleInfo = false;
            }
            else if (name == "book-title")
            {
                out->meta.title =
                    currentText.trimmed();
            }
            else if (name == "genre")
            {
                out->meta.genre =
                    currentText.trimmed();
            }
            else if (name == "first-name")
            {
                inFirstName = false;

                if (inAuthor)
                    authorFirst = currentText.trimmed();
                else if (inTranslator)
                    translatorFirst = currentText.trimmed();
            }
            else if (name == "last-name")
            {
                inLastName = false;

                if (inAuthor)
                    authorLast = currentText.trimmed();
                else if (inTranslator)
                    translatorLast = currentText.trimmed();
            }
            else if (name == "middle-name")
            {
                inMiddleName = false;

                if (inAuthor)
                    authorMiddle = currentText.trimmed();
                else if (inTranslator)
                    translatorMiddle = currentText.trimmed();
            }
            else if (name == "author")
            {
                inAuthor = false;

                if (out->meta.author.isEmpty())
                {
                    out->meta.author =
                        joinName(
                            authorFirst,
                            authorMiddle,
                            authorLast
                        );
                }
            }
            else if (name == "translator")
            {
                inTranslator = false;

                if (out->meta.translator.isEmpty())
                {
                    out->meta.translator =
                        joinName(
                            translatorFirst,
                            translatorMiddle,
                            translatorLast
                        );
                }
            }
            else if (name == "annotation")
            {
                inAnnotation = false;

                out->meta.annotation =
                    annotationText.trimmed();
            }

            else if (name == "p" && inParagraph)
            {
                inParagraph = false;

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
                else
                {
                    appendParagraph(currentText);
                }
            }
            else if (name == "title")
            {
                inTitle = false;

                if (currentChapter && !titleText.isEmpty())
                {
                    currentChapter->title = titleText;
                }
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

            currentText.clear();
        }
    }

    if (xml.hasError())
    {
        if (errorOut)
        {
            *errorOut =
                "Ошибка XML: "
                + xml.errorString();
        }

        return false;
    }

    if (out->meta.title.isEmpty())
    {
        out->meta.title =
            QFileInfo(filePath).baseName();
    }

    if (out->meta.author.isEmpty())
    {
        out->meta.author =
            "Неизвестный автор";
    }

    if (out->chapters.isEmpty())
    {
        ParsedChapter chapter;

        chapter.index = 0;
        chapter.title = "Без названия";
        chapter.text = "Текст книги не найден";

        out->chapters.append(chapter);
    }

    return true;
}