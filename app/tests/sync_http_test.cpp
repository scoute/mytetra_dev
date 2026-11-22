#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <cstdio>

#include "SyncHttp.h"

// Регрессии разбора HTTP-запроса sync/1:
//  - неполные заголовки НЕ исполняются как запрос с пустым телом;
//  - нечисловой/огромный Content-Length — Broken (не жрём память);
//  - Expect: 100-continue подтверждается только пока тела нет;
//  - полный JSON разбирается в body.
//
//   make -C app/tests run

static int failures=0;

#define CHECK(cond) do { \
    if(!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        ++failures; \
    } \
} while(0)


static QByteArray request(const QByteArray &body,
                          int contentLength=-1,
                          bool expectContinue=false)
{
    QByteArray out="POST /push-note HTTP/1.1\r\n";
    out+="Content-Type: application/json\r\n";

    const int length=contentLength>=0 ? contentLength : body.size();
    out+="Content-Length: "+QByteArray::number(length)+"\r\n";

    if(expectContinue)
        out+="Expect: 100-continue\r\n";

    out+="\r\n";
    out+=body;
    return out;
}


int main(void)
{
    using namespace SyncHttp;

    // Пусто / только стартовая строка — ждём, ничего не исполняем
    CHECK(parse(QByteArray()).status==Status::NeedMore);
    CHECK(parse("POST /push-note HTTP/1.1\r\n").status==Status::NeedMore);

    // Классический баг: строка запроса пришла, заголовки ещё нет
    CHECK(parse("POST /manifest HTTP/1.1\r\nHost: x\r\n").status
          ==Status::NeedMore);

    // Полный запрос без тела
    {
        const ParseResult r=parse(request(QByteArray(), 0));
        CHECK(r.status==Status::Ready);
        CHECK(r.methodPath==QStringLiteral("POST /push-note"));
        CHECK(r.body.isEmpty());
    }

    // Тело пришло частично — ждём, не разбираем
    {
        const QByteArray body="{\"id\":\"r1\"}";
        QByteArray raw=request(body);
        raw.chop(3); // не хватает хвоста
        CHECK(parse(raw).status==Status::NeedMore);
    }

    // Полный JSON
    {
        const QByteArray body="{\"id\":\"r1\",\"branch\":\"b1\"}";
        const ParseResult r=parse(request(body));
        CHECK(r.status==Status::Ready);
        CHECK(r.body.value(QStringLiteral("id")).toString()
              ==QStringLiteral("r1"));
        CHECK(r.body.value(QStringLiteral("branch")).toString()
              ==QStringLiteral("b1"));
    }

    // HTTP/1.1 keep-alive: два запроса в буфере, consumed указывает,
    // где заканчивается первый — остаток разбирается отдельно
    {
        const QByteArray first=request("{\"id\":\"r1\"}");
        const QByteArray second=request("{\"id\":\"r2\"}");
        const QByteArray both=first+second;
        const ParseResult r=parse(both);
        CHECK(r.status==Status::Ready);
        CHECK(r.consumed==first.size());
        CHECK(r.body.value(QStringLiteral("id")).toString()
              ==QStringLiteral("r1"));

        const ParseResult r2=parse(both.mid(r.consumed));
        CHECK(r2.status==Status::Ready);
        CHECK(r2.body.value(QStringLiteral("id")).toString()
              ==QStringLiteral("r2"));
    }

    // Битый JSON / отрицательная / огромная длина — Broken
    CHECK(parse(request("xxx", 3)).status==Status::Broken);

    {
        QByteArray raw="POST /x HTTP/1.1\r\nContent-Length: -1\r\n\r\n";
        CHECK(parse(raw).status==Status::Broken);
    }
    {
        QByteArray raw="POST /x HTTP/1.1\r\nContent-Length: 300000000\r\n\r\n";
        CHECK(parse(raw).status==Status::Broken);
    }
    {
        QByteArray raw="POST /x HTTP/1.1\r\nContent-Length: abc\r\n\r\n";
        CHECK(parse(raw).status==Status::Broken);
    }

    // Expect: 100-continue без тела — просим Continue; с частью тела — нет
    {
        const ParseResult r=parse(request(QByteArray(), 10, true));
        CHECK(r.status==Status::NeedMore);
        CHECK(r.sendContinue);
    }
    {
        const ParseResult r=parse(request("{", 10, true));
        CHECK(r.status==Status::NeedMore);
        CHECK(!r.sendContinue);
    }

    // Бесконечные заголовки ограничены
    {
        QByteArray raw="POST /x HTTP/1.1\r\n";
        for(int i=0; i<5000; ++i)
            raw+="X-Pad: 0123456789\r\n";
        CHECK(parse(raw).status==Status::Broken);
    }

    if(failures==0)
        std::printf("sync_http_test OK\n");

    return failures==0 ? 0 : 1;
}
