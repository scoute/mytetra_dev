#ifndef SYNCHTTP_H
#define SYNCHTTP_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

// Разбор одного HTTP/1.1 запроса sync/1 (тело — JSON).
// Вынесен из SyncServer без сокетов и маршрутов: чистая логика, которую
// легко покрыть тестами. Главное правило: неполный запрос — это NeedMore,
// а НЕ запрос с пустым телом (иначе TCP может разрезать заголовки, и
// push-note исполнился бы с пустым body).
namespace SyncHttp
{

enum class Status
{
    NeedMore, // данных мало, ждём следующий readyRead
    Ready,    // запрос целиком разобран
    Broken    // битый/слишком большой — 400 и disconnect
};

struct ParseResult
{
    Status status=Status::NeedMore;
    QString methodPath;      // "POST /push-note"
    QJsonObject body;        // пустой, если Content-Length == 0
    bool sendContinue=false; // ответить "100 Continue" (Expect: 100-continue)
    int consumed=0;          // байт запроса, включая заголовки и тело
};

// Максимальный размер заголовков и тела: непроверенный Content-Length
// иначе съедает память хаба.
const int maxHeaderBytes=64*1024;
const int maxBodyBytes=256*1024*1024;

ParseResult parse(const QByteArray &raw);

} // namespace SyncHttp

#endif // SYNCHTTP_H
