// DiskHelper на QTest (см. 570).
// Перемещение файлов в корзину между томами: QFile::rename
// между томами не работает (Temp на C:, база на Z: под Wine),
// тогда файл копируется и исходник удаляется. Тест гоняет
// контракт moveFileCrossVolume и сквозные removeFileToTrash /
// removeDirectoryToTrash на временной базе, кросс-томность -
// парой /dev/shm (tmpfs) + /tmp (ext4), со скипом если тома совпали.
#include <QtTest>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include <sys/stat.h>

#include "libraries/helpers/DiskHelper.h"
#include "libraries/AppContext.h"
#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "libraries/TrashMonitoring.h"
#include "libraries/WalkHistory.h"
#include "libraries/VisitHistory.h"
#include "libraries/ActionLogger.h"
#include "libraries/ShortcutManager.h"
#include "libraries/PeriodicCheckBase.h"
#include "libraries/PeriodicSyncro.h"
#include "libraries/InternalClipboard.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppFiles.h"
#include "models/dataBaseConfig/DataBaseConfig.h"

extern AppConfig mytetraConfig;

class TestDiskHelper : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void moveSameVolume(void);
    void moveAcrossVolumes(void);
    void removeFileToTrashEndToEnd(void);
    void removeDirectoryToTrashEndToEnd(void);

    // Корзины нет (свежий portable): создается на месте, файлы уезжают
    void missingTrashIsCreatedOnRemoveFile(void);
    void missingTrashIsCreatedOnRemoveDirectory(void);

private:

    QTemporaryDir m_tmp;
    QString m_trashDir;
    QSettings *m_settings=nullptr;

    static bool writeFile(const QString &path, const QByteArray &data);
    static dev_t deviceOf(const QString &path);
};

void TestDiskHelper::initTestCase(void)
{
    static FixedParameters *fp=new FixedParameters;
    static GlobalParameters *gp=new GlobalParameters;
    static AppFiles *af=new AppFiles;
    static DataBaseConfig *dbc=new DataBaseConfig;
    static TrashMonitoring *tm=new TrashMonitoring;
    static WalkHistory *wh=new WalkHistory;
    static VisitHistory *vh=new VisitHistory;
    static ActionLogger *al=new ActionLogger;
    static ShortcutManager *sm=new ShortcutManager;
    static PeriodicCheckBase *pcb=new PeriodicCheckBase;
    static PeriodicSyncro *ps=new PeriodicSyncro;
    static InternalClipboard *ic=new InternalClipboard;
    AppContext::instance().init(fp, gp, &mytetraConfig, af, dbc, tm,
                                wh, vh, al, sm, pcb, ps,
                                ic, nullptr, &gp->cryptHolder());

    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    m_settings=new QSettings(m_tmp.filePath(QStringLiteral("test_diskhelper.ini")),
                             QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;

    m_trashDir=m_tmp.filePath(QStringLiteral("trash"));
    QVERIFY2(QDir().mkpath(m_trashDir), "trash must be created");
    m_settings->setValue(QStringLiteral("trashdir"), m_trashDir);
}

void TestDiskHelper::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

bool TestDiskHelper::writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);

    if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    const bool ok=(f.write(data)==data.size());
    f.close();

    return ok;
}

dev_t TestDiskHelper::deviceOf(const QString &path)
{
    struct stat info;

    if(::stat(path.toLocal8Bit().constData(), &info)!=0)
        return 0;

    return info.st_dev;
}

void TestDiskHelper::moveSameVolume(void)
{
    // Один том: быстрый путь через rename
    const QString src=m_tmp.filePath(QStringLiteral("same_src.txt"));
    const QString dst=m_tmp.filePath(QStringLiteral("same_dst.txt"));
    QVERIFY2(writeFile(src, QByteArray("same-volume")), "src must be written");

    QVERIFY2(DiskHelper::moveFileCrossVolume(src, dst), "move must succeed");
    QVERIFY2(!QFile::exists(src), "src must be gone");

    QFile result(dst);
    QVERIFY2(result.open(QIODevice::ReadOnly), "dst must be readable");
    QCOMPARE(result.readAll(), QByteArray("same-volume"));
}

void TestDiskHelper::moveAcrossVolumes(void)
{
    // Пара tmpfs+ext4: настоящий кросс-томный rename (как C:->Z: под Wine).
    // Тома совпали (одна ФС, нет /dev/shm) - скип, а не падение
    const QString shmDir=QStringLiteral("/dev/shm");
    QVERIFY2(QDir(shmDir).exists(), "/dev/shm must exist");

    const QString probe=m_tmp.filePath(QStringLiteral("probe.txt"));
    QVERIFY2(writeFile(probe, QByteArray("x")), "probe must be written");

    if(deviceOf(shmDir)==0 || deviceOf(shmDir)==deviceOf(probe))
        QSKIP("same filesystem, no cross-volume pair available");

    const QString src=shmDir+QStringLiteral("/mytetra_cross_src.txt");
    const QString dst=m_tmp.filePath(QStringLiteral("cross_dst.txt"));
    QVERIFY2(writeFile(src, QByteArray("cross-volume")), "shm src must be written");
    QFile::remove(dst);

    QVERIFY2(DiskHelper::moveFileCrossVolume(src, dst), "cross-volume move must succeed");
    QVERIFY2(!QFile::exists(src), "shm src must be gone");

    QFile result(dst);
    QVERIFY2(result.open(QIODevice::ReadOnly), "dst must be readable");
    QCOMPARE(result.readAll(), QByteArray("cross-volume"));
}

void TestDiskHelper::removeFileToTrashEndToEnd(void)
{
    // Сквозной путь формулы: временный файл уезжает в корзину
    const QString src=m_tmp.filePath(QStringLiteral("mytetra_tex_probe.txt"));
    QVERIFY2(writeFile(src, QByteArray("formula-source")), "src must be written");

    QVERIFY2(DiskHelper::removeFileToTrash(src), "remove to trash must succeed");
    QVERIFY2(!QFile::exists(src), "src must be gone");

    const QStringList trashFiles=QDir(m_trashDir).entryList(QStringList() << QStringLiteral("*_mytetra_tex_probe.txt"),
                                                            QDir::Files);
    QCOMPARE(trashFiles.size(), 1);

    QFile result(m_trashDir+QStringLiteral("/")+trashFiles.first());
    QVERIFY2(result.open(QIODevice::ReadOnly), "trash copy must be readable");
    QCOMPARE(result.readAll(), QByteArray("formula-source"));
}

void TestDiskHelper::removeDirectoryToTrashEndToEnd(void)
{
    // Каталог целиком: файлы с уникальными именами в корзине, каталог удален
    const QString srcDir=m_tmp.filePath(QStringLiteral("notedir"));
    QVERIFY2(QDir().mkpath(srcDir), "srcdir must be created");
    QVERIFY2(writeFile(srcDir+QStringLiteral("/a.txt"), QByteArray("aaa")), "a must be written");
    QVERIFY2(writeFile(srcDir+QStringLiteral("/b.txt"), QByteArray("bbb")), "b must be written");

    DiskHelper::removeDirectoryToTrash(srcDir);
    QVERIFY2(!QDir(srcDir).exists(), "srcdir must be gone");

    const QStringList trashA=QDir(m_trashDir).entryList(QStringList() << QStringLiteral("*_a.txt"),
                                                         QDir::Files);
    const QStringList trashB=QDir(m_trashDir).entryList(QStringList() << QStringLiteral("*_b.txt"),
                                                         QDir::Files);
    QCOMPARE(trashA.size(), 1);
    QCOMPARE(trashB.size(), 1);
}

void TestDiskHelper::missingTrashIsCreatedOnRemoveFile(void)
{
    // Свежий portable без корзины: создается сама, файл уезжает
    const QString freshTrash=m_tmp.filePath(QStringLiteral("fresh_trash_file"));
    QVERIFY2(!QDir(freshTrash).exists(), "fresh trash must not exist yet");
    m_settings->setValue(QStringLiteral("trashdir"), freshTrash);

    const QString src=m_tmp.filePath(QStringLiteral("orphan_src.txt"));
    QVERIFY2(writeFile(src, QByteArray("orphan")), "src must be written");

    QVERIFY2(DiskHelper::removeFileToTrash(src), "remove must succeed with autocreated trash");
    QVERIFY2(QDir(freshTrash).exists(), "trash must be autocreated");
    QVERIFY2(!QFile::exists(src), "src must be gone");
    QCOMPARE(QDir(freshTrash).entryList(QStringList() << QStringLiteral("*_orphan_src.txt"),
                                         QDir::Files).size(), 1);

    m_settings->setValue(QStringLiteral("trashdir"), m_trashDir);
}

void TestDiskHelper::missingTrashIsCreatedOnRemoveDirectory(void)
{
    // То же для каталога целиком
    const QString freshTrash=m_tmp.filePath(QStringLiteral("fresh_trash_dir"));
    QVERIFY2(!QDir(freshTrash).exists(), "fresh trash must not exist yet");
    m_settings->setValue(QStringLiteral("trashdir"), freshTrash);

    const QString srcDir=m_tmp.filePath(QStringLiteral("orphandir"));
    QVERIFY2(QDir().mkpath(srcDir), "srcdir must be created");
    QVERIFY2(writeFile(srcDir+QStringLiteral("/c.txt"), QByteArray("ccc")), "c must be written");

    DiskHelper::removeDirectoryToTrash(srcDir);
    QVERIFY2(QDir(freshTrash).exists(), "trash must be autocreated");
    QVERIFY2(!QDir(srcDir).exists(), "srcdir must be gone");
    QCOMPARE(QDir(freshTrash).entryList(QStringList() << QStringLiteral("*_c.txt"),
                                         QDir::Files).size(), 1);

    m_settings->setValue(QStringLiteral("trashdir"), m_trashDir);
}

QTEST_APPLESS_MAIN(TestDiskHelper)

#include "test_diskhelper.moc"
