#include <QString>

#include <cstdio>

#include "SyncValidation.h"

// Валидация путей от пира: без неё push-note с dir="../../.." пишет
// вне базы (и «зеркальная уборка» уносит в корзину чужой каталог).
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
    using namespace SyncValidation;

    // --- имена файлов ---
    CHECK(isSafeFileName(QStringLiteral("note.html")));
    CHECK(isSafeFileName(QStringLiteral("a b.txt")));
    CHECK(isSafeFileName(QString::fromUtf8("заметка.html")));
    CHECK(isSafeFileName(QStringLiteral("file-2026_01.tar.gz")));

    CHECK(!isSafeFileName(QString()));
    CHECK(!isSafeFileName(QStringLiteral(".")));
    CHECK(!isSafeFileName(QStringLiteral("..")));
    CHECK(!isSafeFileName(QStringLiteral("../x")));
    CHECK(!isSafeFileName(QStringLiteral("a/b")));
    CHECK(!isSafeFileName(QStringLiteral("a\\b")));
    CHECK(!isSafeFileName(QStringLiteral("/etc/passwd")));
    CHECK(!isSafeFileName(QStringLiteral("C:\\Windows\\x")));

    QString withNul=QStringLiteral("a");
    withNul+=QChar(0);
    withNul+=QStringLiteral("b");
    CHECK(!isSafeFileName(withNul));

    // --- каталог записи ---
    CHECK(isSafeRecordDir(QStringLiteral("2026")));
    CHECK(isSafeRecordDir(QStringLiteral("2026/05/01")));
    CHECK(isSafeRecordDir(QString::fromUtf8("архив/2026")));

    CHECK(!isSafeRecordDir(QString()));
    CHECK(!isSafeRecordDir(QStringLiteral(".")));
    CHECK(!isSafeRecordDir(QStringLiteral("..")));
    CHECK(!isSafeRecordDir(QStringLiteral("../x")));
    CHECK(!isSafeRecordDir(QStringLiteral("a/../b")));
    CHECK(!isSafeRecordDir(QStringLiteral("a/..")));
    CHECK(!isSafeRecordDir(QStringLiteral("/abs")));
    CHECK(!isSafeRecordDir(QStringLiteral("a//../b")));
    CHECK(!isSafeRecordDir(QStringLiteral("a\\b")));

    // --- путь остался внутри базы ---
    CHECK(isInsideDir(QStringLiteral("/base"), QStringLiteral("/base/a/b")));
    CHECK(isInsideDir(QStringLiteral("/base"), QStringLiteral("/base")));
    CHECK(!isInsideDir(QStringLiteral("/base"), QStringLiteral("/base2/a")));
    CHECK(!isInsideDir(QStringLiteral("/base"), QStringLiteral("/base/../etc")));
    CHECK(!isInsideDir(QStringLiteral("/base"), QStringLiteral("/etc/passwd")));

    if(failures==0)
        std::printf("sync_validation_test OK\n");

    return failures==0 ? 0 : 1;
}
