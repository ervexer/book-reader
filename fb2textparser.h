#pragma once

#include <QString>
#include <QVector>
#include <QByteArray>

struct ParsedChapter
{
    int index = 0;
    QString title;
    QString text;
};

class Fb2TextParser
{
public:
    static QVector<ParsedChapter> parseFile(
        int bookId,
        const QByteArray& fb2Data,
        QString* errorOut = nullptr
    );
};