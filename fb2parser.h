// fb2parser.h

#pragma once

#include <QString>
#include <QByteArray>
#include <QVector>

struct Fb2Meta
{
    QString title;
    QString author;
    QString translator;
    QString annotation;
    QString genre;
    QByteArray cover; // обложка (декодированные байты картинки), может быть пустой
};

struct ParsedChapter
{
    int index = 0;
    QString title;
    QString text; // HTML-фрагмент главы (абзацы, курсив/жирный, подзаголовки)
};

struct ParsedBook
{
    Fb2Meta meta;
    QVector<ParsedChapter> chapters;
};

class Fb2Parser
{
public:
    static bool parseFile(
        const QString& filePath,
        ParsedBook* out,
        QString* errorOut = nullptr
    );
};
