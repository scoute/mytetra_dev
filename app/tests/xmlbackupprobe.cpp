// Ротация и проверка бэкапов mytetra.xml (см. 513).
// Чистая файловая логика, без AppContext.
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTemporaryDir>

#include "models/tree/KnowTreeModel.h"

static int fails=0;

static void check(bool ok, const char *name)
{
    qDebug() << (ok ? "PASS" : "FAIL") << name;

    if(!ok)
        ++fails;
}

static const char *BASE_XML=
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<root><format version=\"1\" subversion=\"1\" />"
    "<content><node id=\"n1\" name=\"B\">"
    "<recordtable>"
    "<record dir=\"d1\" id=\"r1\" name=\"A\" file=\"text.html\" />"
    "<record dir=\"d2\" id=\"r2\" name=\"B\" file=\"text.html\" />"
    "</recordtable></node>"
    "<node id=\"n2\" name=\"C\"></node>"
    "</content></root>\n";

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    check(f.open(QIODevice::WriteOnly), "BACKUP-OPEN");
    f.write(data);
    f.close();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tmp;
    check(tmp.isValid(), "BACKUP-TMPDIR");

    const QString xml=tmp.filePath(QStringLiteral("mytetra.xml"));
    writeFile(xml, QByteArray(BASE_XML));

    // Валидный файл: сканируется, ветки и записи считаются
    int branches=0;
    int records=0;
    check(KnowTreeModel::scanBackupFile(xml, &branches, &records), "BACKUP-SCAN-OK");
    check(branches==2 && records==2, "BACKUP-SCAN-COUNTS");

    // Битый файл: не предлагается
    const QString bad=tmp.filePath(QStringLiteral("mytetra.xml"));
    writeFile(bad, QByteArray("<root><node><unclosed"));
    check(!KnowTreeModel::scanBackupFile(bad, &branches, &records), "BACKUP-SCAN-BROKEN");

    // Нет файла: пусто, а не падение
    check(!KnowTreeModel::scanBackupFile(tmp.filePath(QStringLiteral("nope.xml")), nullptr, nullptr),
          "BACKUP-SCAN-MISSING");

    // Восстановить валидный для ротации
    writeFile(xml, QByteArray(BASE_XML));

    // Ротация со строго растущим временем: детерминировано,
    // без лотереи гранулярности mtime
    auto setMtime = [](const QString &path, qint64 msecs)
    {
        QFile f(path);
        f.open(QIODevice::ReadWrite);
        f.setFileTime(QDateTime::fromMSecsSinceEpoch(msecs, Qt::UTC), QFile::FileModificationTime);
        f.close();
    };

    const qint64 base0=1700000000000;

    for(int k=0; k<5; ++k)
    {
        setMtime(xml, base0+k*5000);
        KnowTreeModel::rotateBackups(xml);
    }

    int kept=0;
    for(int i=1; i<=3; ++i)
        if(QFile::exists(xml+QStringLiteral(".bak.")+QString::number(i)))
            ++kept;
    check(kept==3, "BACKUP-ROTATE-COUNT");
    check(!QFile::exists(xml+QStringLiteral(".bak.4")), "BACKUP-ROTATE-MAX");

    // Пропуск при идентичном бэкапе: время выставлено равным вручную
    {
        const QString x2dir=tmp.filePath(QStringLiteral("skipdir"));
        QDir().mkpath(x2dir);

        const QString x2=x2dir+QStringLiteral("/mytetra.xml");
        writeFile(x2, QByteArray(BASE_XML));
        setMtime(x2, base0+9000);
        KnowTreeModel::rotateBackups(x2);

        QFile b1(x2+QStringLiteral(".bak.1"));
        check(b1.exists(), "BACKUP-SKIP-CREATED");
        setMtime(x2+QStringLiteral(".bak.1"), base0+9000);

        KnowTreeModel::rotateBackups(x2);
        check(!QFile::exists(x2+QStringLiteral(".bak.2")), "BACKUP-SKIP-NOOP");
    }

    // Кандидаты находятся и читаются
    QList<KnowTreeModel::XmlBackupInfo> found=KnowTreeModel::usableBackups(xml);
    check(found.size()==3, "BACKUP-FOUND-COUNT");
    check(!found.isEmpty() && found.first().branches==2 && found.first().records==2,
          "BACKUP-FOUND-COUNTS");

    if(fails==0)
        qDebug() << "PROBE-OK xmlbackup";

    return 0;
}
