// fb2parser.h

#pragma once

#include <QString>
#include <QVector>

struct Fb2Meta
{
    QString title;
    QString author;
    QString translator;
    QString annotation;
    QString genre;
};

struct ParsedChapter
{
    int index = 0;
    QString title;
    QString text;
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