#include <QJsonDocument>
#include <QJsonParseError>
#include <QList>
#include <QStringList>

#include "SyncHttp.h"


namespace SyncHttp
{

ParseResult parse(const QByteArray &raw)
{
    ParseResult result;

    // Стартовая строка: "METHOD /path HTTP/1.x"
    const int lineEnd=raw.indexOf("\r\n");

    if(lineEnd<0)
    {
        if(raw.size()>maxHeaderBytes)
            result.status=Status::Broken;

        return result;
    }

    const QList<QByteArray> parts=QByteArray(raw.constData(), lineEnd).split(' ');

    if(parts.size()<2)
    {
        result.status=Status::Broken;
        return result;
    }

    const QString method=QString::fromLatin1(parts.at(0)).toUpper();
    const QString path=QString::fromLatin1(parts.at(1));
    result.methodPath=method+QStringLiteral(" ")+path;

    // Заголовки ещё не пришли целиком — ждём. Раньше здесь получался
    // "готовый запрос" с пустым телом.
    const int headEnd=raw.indexOf("\r\n\r\n");

    if(headEnd<0)
    {
        if(raw.size()>maxHeaderBytes)
            result.status=Status::Broken;

        return result;
    }

    int contentLength=0;
    const QString head=QString::fromLatin1(raw.constData(), headEnd);

    for(const QString &line : head.split(QStringLiteral("\r\n")))
    {
        if(line.startsWith(QStringLiteral("Content-Length:"),
                           Qt::CaseInsensitive))
        {
            bool ok=false;
            contentLength=line.section(QLatin1Char(':'), 1).trimmed()
                              .toInt(&ok);

            if(!ok || contentLength<0 || contentLength>maxBodyBytes)
            {
                result.status=Status::Broken;
                return result;
            }

            break;
        }
    }

    const bool expectContinue=head.toLower()
        .contains(QStringLiteral("expect: 100-continue"));

    const int bodyStart=headEnd+4;
    const int bodyHave=raw.size()-bodyStart;

    if(bodyHave<contentLength)
    {
        // Клиент (QNetworkAccessManager) ждёт промежуточного подтверждения,
        // иначе тело не отправит вовсе. Отвечаем сразу, тело ждём дальше.
        result.status=Status::NeedMore;
        result.sendContinue=expectContinue && bodyHave==0;
        return result;
    }

    if(contentLength<=0)
    {
        result.status=Status::Ready;
        result.body=QJsonObject();
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument doc=QJsonDocument::fromJson(
        QByteArray(raw.constData()+bodyStart, contentLength), &parseError);

    if(parseError.error!=QJsonParseError::NoError || !doc.isObject())
    {
        result.status=Status::Broken;
        return result;
    }

    result.body=doc.object();
    result.status=Status::Ready;
    return result;
}

} // namespace SyncHttp
