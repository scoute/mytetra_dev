#include <QMap>
#include <QString>

#include <cstdio>

#include "SyncTombstones.h"

// Правило покрытия: tombstone ветки действует на всё поддерево.
// Регрессия: заметки под удалённым предком раньше проверялись только по
// прямому branchId и вечно давали "no branch" со старыми пирами.
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
    using SyncTombstones::coveredByAncestor;

    const QMap<QString, QString> parents={
        {QStringLiteral("child"), QStringLiteral("root")},
        {QStringLiteral("grand"), QStringLiteral("child")},
        {QStringLiteral("other"), QStringLiteral("")},
    };

    // прямой tombstone
    CHECK(coveredByAncestor(QStringLiteral("child"), parents,
                            {{QStringLiteral("child"), 1}}));

    // tombstone предка покрывает внука
    CHECK(coveredByAncestor(QStringLiteral("grand"), parents,
                            {{QStringLiteral("root"), 1}}));
    CHECK(coveredByAncestor(QStringLiteral("grand"), parents,
                            {{QStringLiteral("child"), 1}}));

    // чужая ветка не покрыта
    CHECK(!coveredByAncestor(QStringLiteral("other"), parents,
                             {{QStringLiteral("root"), 1}}));

    // пустые карты
    CHECK(!coveredByAncestor(QStringLiteral("grand"), parents, {}));
    CHECK(!coveredByAncestor(QString(), parents,
                             {{QStringLiteral(""), 1}}));

    // цикл в parents не вешает
    const QMap<QString, QString> loop={
        {QStringLiteral("a"), QStringLiteral("b")},
        {QStringLiteral("b"), QStringLiteral("a")},
    };
    CHECK(!coveredByAncestor(QStringLiteral("a"), loop, {}));
    CHECK(coveredByAncestor(QStringLiteral("a"), loop,
                            {{QStringLiteral("b"), 1}}));

    if(failures==0)
        std::printf("sync_tombstones_test OK\n");

    return failures==0 ? 0 : 1;
}
