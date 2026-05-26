#include "fb2textparser.h"

#include <QBuffer>
#include <QXmlStreamReader>

QVector<ParsedChapter> Fb2TextParser::parseFile(int bookId, const QByteArray& fb2Data, QString* errorOut)
{
    Q_UNUSED(bookId);

    QBuffer buffer;
    buffer.setData(fb2Data);
    buffer.open(QIODevice::ReadOnly);

    QXmlStreamReader xml(&buffer);

    QVector<ParsedChapter> chapters;

    ParsedChapter* currentChapter = nullptr;

    bool inBody = false;
    bool inSection = false;
    bool inTitle = false;
    bool inParagraph = false;

    QString currentText;
    QString titleText;

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

            if (name == "body")
            {
                inBody = true;
            }
            else if (inBody && name == "section")
            {
                inSection = true;

                ParsedChapter chapter;
                chapter.index = chapterIndex++;
                chapter.title = QString("Глава %1")
                    .arg(chapter.index + 1);

                chapters.append(chapter);
                currentChapter = &chapters.last();
            }
            else if (inSection && name == "title")
            {
                inTitle = true;
                titleText.clear();
            }
            else if (inSection && name == "p")
            {
                inParagraph = true;
                currentText.clear();
            }
        }
        else if (xml.isCharacters() && !xml.isWhitespace())
        {
            if (inParagraph)
                currentText += xml.text().toString();
        }
        else if (xml.isEndElement())
        {
            QString name = xml.name().toString();

            if (name == "p" && inParagraph)
            {
                inParagraph = false;

                if (inTitle)
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
        }
    }

    if (xml.hasError())
    {
        if (errorOut)
            *errorOut = "Ошибка XML: " + xml.errorString();

        return {};
    }

    if (chapters.isEmpty())
    {
        ParsedChapter chapter;

        chapter.index = 0;
        chapter.title = "Без названия";
        chapter.text = "Текст книги не найден";

        chapters.append(chapter);
    }

    return chapters;
}