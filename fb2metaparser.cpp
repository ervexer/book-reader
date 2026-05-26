#include "fb2metaparser.h"

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

bool Fb2MetaParser::parseFile(const QString& filePath, Fb2Meta* out, QString* errorOut)
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

    bool inTitleInfo = false;

    bool inBookTitle = false;
    bool inGenre = false;

    bool inAuthor = false;
    bool inTranslator = false;

    bool inFirstName = false;
    bool inLastName = false;
    bool inMiddleName = false;

    bool inAnnotation = false;
    bool inParagraph = false;

    QString currentText;

    QString authorFirst;
    QString authorLast;
    QString authorMiddle;

    QString translatorFirst;
    QString translatorLast;
    QString translatorMiddle;

    QString annotationText;

    while (!xml.atEnd())
    {
        xml.readNext();

        // ================= START ELEMENT =================
        if (xml.isStartElement())
        {
            QString name = xml.name().toString();

            if (name == "body")
                break;

            if (name == "title-info")
            {
                inTitleInfo = true;
                continue;
            }

            if (!inTitleInfo)
                continue;

            if (name == "book-title")
            {
                inBookTitle = true;
                currentText.clear();
            }
            else if (name == "genre")
            {
                inGenre = true;
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
            else if (name == "p" && inAnnotation)
            {
                inParagraph = true;
                currentText.clear();
            }
        }

        // ================= TEXT =================
        else if (xml.isCharacters() && !xml.isWhitespace())
        {
            currentText += xml.text().toString();
        }

        // ================= END ELEMENT =================
        else if (xml.isEndElement())
        {
            QString name = xml.name().toString();

            if (name == "title-info")
            {
                inTitleInfo = false;
            }
            else if (name == "book-title")
            {
                inBookTitle = false;
                out->title = currentText.trimmed();
            }
            else if (name == "genre")
            {
                inGenre = false;
                out->genre = currentText.trimmed();
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

                if (out->author.isEmpty())
                {
                    out->author = joinName(
                        authorFirst,
                        authorMiddle,
                        authorLast
                    );
                }
            }
            else if (name == "translator")
            {
                inTranslator = false;

                if (out->translator.isEmpty())
                {
                    out->translator = joinName(
                        translatorFirst,
                        translatorMiddle,
                        translatorLast
                    );
                }
            }
            else if (name == "p" && inParagraph)
            {
                inParagraph = false;

                if (!annotationText.isEmpty())
                    annotationText += "\n";

                annotationText += currentText.trimmed();
            }
            else if (name == "annotation")
            {
                inAnnotation = false;
                out->annotation = annotationText.trimmed();
            }

            currentText.clear();
        }
    }

    if (xml.hasError())
    {
        if (errorOut)
            *errorOut = xml.errorString();

        return false;
    }

    if (out->title.isEmpty())
        out->title = QFileInfo(filePath).baseName();

    if (out->author.isEmpty())
        out->author = "Неизвестный автор";

    return true;
}