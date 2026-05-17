#pragma once
#include <QString>
#include <QByteArray>
#include <QHash>

struct Fb2Meta
{
    QString title;
    QString author;
    QString translator;
    QString annotation;
    QString genre;
    QByteArray cover;
    QByteArray rawFileData;
    QHash<QString, QByteArray> binaries;
    QHash<QString, QString>    binaryTypes;
};

class Fb2MetaParser
{
public:
    static bool parseFile(const QString& filePath, Fb2Meta* out, QString* errorOut = nullptr);
};
