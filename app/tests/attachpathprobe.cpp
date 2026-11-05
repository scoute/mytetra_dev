// Поиск файла аттача по всем схемам имен (см. 509).
// Чистая файловая логика: AppContext не нужен, только QCoreApplication.
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include "models/attachTable/Attach.h"

static int fails=0;

static void check(bool ok, const char *name)
{
    qDebug() << (ok ? "PASS" : "FAIL") << name;

    if(!ok)
        ++fails;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tmp;
    check(tmp.isValid(), "PATH-TMPDIR");

    const QString dirName=tmp.path();
    const QString id=QStringLiteral("abc123testid");
    const QString visible=QStringLiteral("test.pdf");

    QStringList candidates=Attach::candidateFileNames(QStringLiteral("file"), id, visible);
    check(candidates.size()>=2, "PATH-MULTI-SCHEME");

    // Файл лежит только под вторым именем: находится все равно
    {
        QFile f(dirName+QStringLiteral("/")+candidates.at(1));
        check(f.open(QIODevice::WriteOnly), "PATH-FIXTURE");
        f.write("data");
        f.close();
    }

    QString found=Attach::findExistingFile(dirName, QStringLiteral("file"), id, visible);
    check(found==dirName+QStringLiteral("/")+candidates.at(1), "PATH-NONFIRST-SCHEME");

    // Файл под двумя именами: побеждает первое (порядок как в чтении)
    {
        QFile f(dirName+QStringLiteral("/")+candidates.at(0));
        check(f.open(QIODevice::WriteOnly), "PATH-FIXTURE-FIRST");
        f.write("data");
        f.close();
    }

    found=Attach::findExistingFile(dirName, QStringLiteral("file"), id, visible);
    check(found==dirName+QStringLiteral("/")+candidates.at(0), "PATH-FIRST-WINS");

    // Ничего нет: пусто, а не падение
    check(Attach::findExistingFile(dirName, QStringLiteral("file"),
                                   QStringLiteral("no-such-id"),
                                   QStringLiteral("no-such.bin")).isEmpty(),
          "PATH-MISSING-EMPTY");

    if(fails==0)
        qDebug() << "PROBE-OK attachpath";

    return 0;
}
