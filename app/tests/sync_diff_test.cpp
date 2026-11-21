#include <QString>
#include <QStringList>

#include <cstdio>

#include "SyncDiff.h"

// Дешёвый git-diff для превью: видимый текст (без HTML), картинки-
// заглушки, декодирование сущностей и корректная обработка невалидных
// code point'ов (раньше fromUcs4 получал мусор).
//
//   make -C app/tests run

static int failures=0;

#define CHECK(cond) do { \
    if(!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        ++failures; \
    } \
} while(0)


int main(void)
{
    using namespace SyncDiff;

    // HTML -> видимые строки, блочные теги в переносы
    {
        const QStringList lines=visibleLines(
            QStringLiteral("<style>p{color:red}</style>"
                           "<p>Hello &amp; world</p><p>Second</p>"));
        CHECK(lines.size()==2);
        CHECK(lines.value(0)==QStringLiteral("Hello & world"));
        CHECK(lines.value(1)==QStringLiteral("Second"));
    }

    // <img> -> заглушка с именем файла, сам тег не остаётся
    {
        const QStringList lines=visibleLines(
            QStringLiteral("<p>text</p><img src=\"img/pic.png\">"));
        CHECK(lines.join(QStringLiteral("|")).contains(QStringLiteral("[картинка: pic.png]")));
        CHECK(!lines.join(QStringLiteral("|")).contains(QStringLiteral("<img")));
    }

    // Числовые/именованные сущности и невалидный кодпоинт
    {
        CHECK(decodeEntities(QStringLiteral("A&#66;&#x43;"))==QStringLiteral("ABC"));
        CHECK(decodeEntities(QStringLiteral("&lt;tag&gt;"))==QStringLiteral("<tag>"));
        // вне Unicode / суррогат — оставляем как текст, не роняем
        CHECK(decodeEntities(QStringLiteral("&#x110000;"))
              ==QStringLiteral("&#x110000;"));
        CHECK(decodeEntities(QStringLiteral("&#xD800;"))
              ==QStringLiteral("&#xD800;"));
    }

    // LCS: общий префикс/суффикс и +/- в середине
    {
        const QList<Line> d=diffLines(
            QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")},
            QStringList{QStringLiteral("a"), QStringLiteral("x"), QStringLiteral("c")});
        CHECK(d.size()==4);
        CHECK(d.value(0).kind==Line::Equal && d.value(0).text==QStringLiteral("a"));
        CHECK(d.value(1).kind==Line::Removed && d.value(1).text==QStringLiteral("b"));
        CHECK(d.value(2).kind==Line::Added && d.value(2).text==QStringLiteral("x"));
        CHECK(d.value(3).kind==Line::Equal && d.value(3).text==QStringLiteral("c"));
        CHECK(summary(d)==QStringLiteral("+1 -1"));
    }

    // Одинаковые тексты — только Equal
    {
        const QList<Line> d=diffLines(QStringList{QStringLiteral("same")},
                                      QStringList{QStringLiteral("same")});
        CHECK(d.size()==1);
        CHECK(d.first().kind==Line::Equal);
    }

    if(failures==0)
        std::printf("sync_diff_test OK\n");

    return failures==0 ? 0 : 1;
}
