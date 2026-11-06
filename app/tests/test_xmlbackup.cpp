// Ротация и проверка бэкапов mytetra.xml на QTest, линия v2 (см. 504).
// Порт xmlbackupprobe: чистая файловая логика, без AppContext.
// Порт xmlbackupprobe: чистая файловая логика, без AppContext.
// Время файлов выставляется явно — детерминировано,
// без лотереи гранулярности mtime.
#include <QtTest>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTemporaryDir>

#include "models/tree/KnowTreeModel.h"

class TestXmlBackup : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);

    void scanValid(void);
    void scanBroken(void);
    void scanMissing(void);
    void rotateKeepsThree(void);
    void rotateSkipsIdentical(void);
    void usableBackupsFound(void);

private:

    QTemporaryDir m_tmp;
    QString m_xml;

    void writeFile(const QString &path, const QByteArray &data);
    static void setMtime(const QString &path, qint64 msecs);
};

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

static const qint64 BASE0=1700000000000;

void TestXmlBackup::writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), "backup file must be writable");
    f.write(data);
    f.close();
}

void TestXmlBackup::setMtime(const QString &path, qint64 msecs)
{
    QFile f(path);
    f.open(QIODevice::ReadWrite);
    f.setFileTime(QDateTime::fromMSecsSinceEpoch(msecs, Qt::UTC), QFile::FileModificationTime);
    f.close();
}

void TestXmlBackup::initTestCase(void)
{
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");
    m_xml=m_tmp.filePath(QStringLiteral("mytetra.xml"));
    writeFile(m_xml, QByteArray(BASE_XML));
}

void TestXmlBackup::scanValid(void)
{
    int branches=0;
    int records=0;
    QVERIFY2(KnowTreeModel::scanBackupFile(m_xml, &branches, &records),
             "valid backup must scan");
    QCOMPARE(branches, 2);
    QCOMPARE(records, 2);
}

void TestXmlBackup::scanBroken(void)
{
    const QString bad=m_tmp.filePath(QStringLiteral("broken.xml"));
    writeFile(bad, QByteArray("<root><node><unclosed"));
    int branches=0;
    int records=0;
    QVERIFY2(!KnowTreeModel::scanBackupFile(bad, &branches, &records),
             "broken file must not scan");
}

void TestXmlBackup::scanMissing(void)
{
    QVERIFY2(!KnowTreeModel::scanBackupFile(m_tmp.filePath(QStringLiteral("nope.xml")),
                                            nullptr, nullptr),
             "missing file must not scan");
}

void TestXmlBackup::rotateKeepsThree(void)
{
    writeFile(m_xml, QByteArray(BASE_XML));

    for(int k=0; k<5; ++k)
    {
        setMtime(m_xml, BASE0+k*5000);
        KnowTreeModel::rotateBackups(m_xml);
    }

    int kept=0;
    for(int i=1; i<=3; ++i)
        if(QFile::exists(m_xml+QStringLiteral(".bak.")+QString::number(i)))
            ++kept;
    QCOMPARE(kept, 3);
    QVERIFY2(!QFile::exists(m_xml+QStringLiteral(".bak.4")),
             "rotation must keep at most three backups");
}

void TestXmlBackup::rotateSkipsIdentical(void)
{
    const QString dir=m_tmp.filePath(QStringLiteral("skipdir"));
    QVERIFY2(QDir().mkpath(dir), "skipdir must be created");

    const QString xml=dir+QStringLiteral("/mytetra.xml");
    writeFile(xml, QByteArray(BASE_XML));
    setMtime(xml, BASE0+9000);
    KnowTreeModel::rotateBackups(xml);

    QVERIFY2(QFile::exists(xml+QStringLiteral(".bak.1")),
             "first backup must be created");
    setMtime(xml+QStringLiteral(".bak.1"), BASE0+9000);

    KnowTreeModel::rotateBackups(xml);
    QVERIFY2(!QFile::exists(xml+QStringLiteral(".bak.2")),
             "identical backup must be skipped");
}

void TestXmlBackup::usableBackupsFound(void)
{
    writeFile(m_xml, QByteArray(BASE_XML));

    for(int k=0; k<3; ++k)
    {
        setMtime(m_xml, BASE0+k*7000);
        KnowTreeModel::rotateBackups(m_xml);
    }

    QList<KnowTreeModel::XmlBackupInfo> found=KnowTreeModel::usableBackups(m_xml);
    QCOMPARE(found.size(), 3);
    QVERIFY2(!found.isEmpty(), "usable backups must be found");
    QCOMPARE(found.first().branches, 2);
    QCOMPARE(found.first().records, 2);
}

QTEST_APPLESS_MAIN(TestXmlBackup)

#include "test_xmlbackup.moc"
