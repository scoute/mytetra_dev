// Поиск файла аттача по всем схемам имен на QTest (см. 509, 520).
// Порт attachpathprobe: чистая файловая логика,
// AppContext не нужен, только QCoreApplication.
#include <QtTest>
#include <QFile>
#include <QDir>
#include <QStringList>
#include <QTemporaryDir>

#include "models/attachTable/Attach.h"
#include "models/attachTable/AttachTableData.h"

class TestAttachPath : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);

    void multiScheme(void);
    void nonFirstSchemeWins(void);
    void firstWins(void);
    void missingIsEmpty(void);

    void storedLinkAbsoluteMode(void);
    void storedLinkRelativeMode(void);
    void sameFsTargetBasic(void);
    void sameFsTargetMissing(void);
    void storedLinkConvertRoundtrip(void);
    void dupFindFolderLink(void);
    void dupFindFileLink(void);
    void dupFindFileSource(void);
    void storedLinkDotDot(void);
    void resolveStoredLink(void);

private:

    QTemporaryDir m_tmp;
    QString m_dir;
    QString m_id;
    QString m_visible;
    QStringList m_candidates;
};

void TestAttachPath::initTestCase(void)
{
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    m_dir=m_tmp.path();
    m_id=QStringLiteral("abc123testid");
    m_visible=QStringLiteral("test.pdf");

    m_candidates=Attach::candidateFileNames(QStringLiteral("file"), m_id, m_visible);
    QVERIFY2(m_candidates.size()>=2, "at least two naming schemes must exist");
}

void TestAttachPath::multiScheme(void)
{
    QVERIFY2(m_candidates.size()>=2, "multi-scheme candidates must be listed");
}

void TestAttachPath::nonFirstSchemeWins(void)
{
    // Файл лежит только под вторым именем: находится все равно
    QFile f(m_dir+QStringLiteral("/")+m_candidates.at(1));
    QVERIFY2(f.open(QIODevice::WriteOnly), "fixture must be writable");
    f.write("data");
    f.close();

    QCOMPARE(Attach::findExistingFile(m_dir, QStringLiteral("file"), m_id, m_visible),
             m_dir+QStringLiteral("/")+m_candidates.at(1));
}

void TestAttachPath::firstWins(void)
{
    // Файл под двумя именами: побеждает первое (порядок как в чтении)
    QFile f(m_dir+QStringLiteral("/")+m_candidates.at(0));
    QVERIFY2(f.open(QIODevice::WriteOnly), "fixture must be writable");
    f.write("data");
    f.close();

    QCOMPARE(Attach::findExistingFile(m_dir, QStringLiteral("file"), m_id, m_visible),
             m_dir+QStringLiteral("/")+m_candidates.at(0));
}

void TestAttachPath::missingIsEmpty(void)
{
    // Ничего нет: пусто, а не падение
    QVERIFY2(Attach::findExistingFile(m_dir, QStringLiteral("file"),
                                      QStringLiteral("no-such-id"),
                                      QStringLiteral("no-such.bin")).isEmpty(),
             "missing file must give empty path");
}

void TestAttachPath::storedLinkAbsoluteMode(void)
{
    // Абсолютный режим (AttachLinkAbsolute=0): как было, так и хранится
    const QString target=m_dir+QStringLiteral("/docs");
    QCOMPARE(Attach::makeStoredLink(m_dir+QStringLiteral("/note1"), target, 0),
             target);
}

void TestAttachPath::storedLinkRelativeMode(void)
{
    // Относительный режим (AttachLinkRelative=1): папка внутри
    // каталога записи — короткое имя
    QCOMPARE(Attach::makeStoredLink(m_dir+QStringLiteral("/note1"),
                                    m_dir+QStringLiteral("/note1/docs"), 1),
             QStringLiteral("docs"));
}

void TestAttachPath::storedLinkDotDot(void)
{
    // Папка снаружи: относительный путь с .., все равно чинится
    // от каталога записи
    QCOMPARE(Attach::makeStoredLink(m_dir+QStringLiteral("/note1"),
                                    m_dir+QStringLiteral("/shared/docs"), 1),
             QStringLiteral("../shared/docs"));
}

void TestAttachPath::resolveStoredLink(void)
{
    // Абсолютный хранимый — как есть
    const QString target=m_dir+QStringLiteral("/docs");
    QCOMPARE(Attach::resolveStoredLink(m_dir+QStringLiteral("/note1"), target),
             target);

    // Относительный — от каталога записи
    QCOMPARE(Attach::resolveStoredLink(m_dir+QStringLiteral("/note1"),
                                       QStringLiteral("docs")),
             m_dir+QStringLiteral("/note1/docs"));

    // С точками — чистится
    QCOMPARE(Attach::resolveStoredLink(m_dir+QStringLiteral("/note1"),
                                       QStringLiteral("../shared/docs")),
             m_dir+QStringLiteral("/shared/docs"));

    // Пустые входы — пустой выход, без падений
    QVERIFY2(Attach::resolveStoredLink(m_dir+QStringLiteral("/note1"),
                                       QString()).isEmpty(),
             "empty stored link must stay empty");
    QCOMPARE(Attach::resolveStoredLink(QString(), QStringLiteral("docs")),
             QStringLiteral("docs"));
}

void TestAttachPath::sameFsTargetBasic(void)
{
    // Один и тот же каталог разными написаниями: .., слэш в хвосте
    const QString target=m_dir+QStringLiteral("/docs");
    QVERIFY2(QDir().mkpath(target), "fixture dir must be created");
    QVERIFY2(Attach::sameFsTarget(target, target+QStringLiteral("/")),
             "trailing slash must match");
    QVERIFY2(Attach::sameFsTarget(target,
                                  m_dir+QStringLiteral("/sub/../docs")),
             "dotdot must match");
    QVERIFY2(!Attach::sameFsTarget(target, m_dir+QStringLiteral("/other")),
             "different dir must not match");
    QVERIFY2(!Attach::sameFsTarget(QString(), target),
             "empty path must not match");
}

void TestAttachPath::sameFsTargetMissing(void)
{
    // Цели нет на диске: сравнение чищеных абсолютных как строк
    const QString missingA=m_dir+QStringLiteral("/gone/sub/../dir");
    const QString missingB=m_dir+QStringLiteral("/gone/dir");
    QVERIFY2(Attach::sameFsTarget(missingA, missingB),
             "missing targets compare as cleaned strings");
    QVERIFY2(!Attach::sameFsTarget(missingA, m_dir+QStringLiteral("/gone/other")),
             "different missing targets must not match");
}

void TestAttachPath::storedLinkConvertRoundtrip(void)
{
    // Ядро конвертера настроек: резолв в абсолютный и обратно
    // в относительный дает исходный хранимый путь
    const QString recordDir=m_dir+QStringLiteral("/note1");
    const QString stored=QStringLiteral("../shared/docs");
    const QString absolutePath=Attach::resolveStoredLink(recordDir, stored);
    QCOMPARE(Attach::makeStoredLink(recordDir, absolutePath, 1), stored);
}

void TestAttachPath::dupFindFolderLink(void)
{
    // Поиск дубля папки в таблице без записи (lite): хранимый
    // абсолютный путь находится по .. и слэшу, чужой — нет
    const QString target=m_dir+QStringLiteral("/dupdir");
    QVERIFY2(QDir().mkpath(target), "fixture dir must be created");

    AttachTableData table(nullptr);
    Attach link(QStringLiteral("link"), &table);
    link.setField(QStringLiteral("id"), QStringLiteral("row1"));
    link.setField(QStringLiteral("fileName"), QStringLiteral("dupdir"));
    link.setField(QStringLiteral("link"), target);
    table.addAttach(link);

    QCOMPARE(table.findLinkRowByPath(target), QStringLiteral("row1"));
    QCOMPARE(table.findLinkRowByPath(target+QStringLiteral("/")),
             QStringLiteral("row1"));
    QCOMPARE(table.findLinkRowByPath(m_dir+QStringLiteral("/sub/../dupdir")),
             QStringLiteral("row1"));
    QVERIFY2(table.findLinkRowByPath(m_dir+QStringLiteral("/elsewhere")).isEmpty(),
             "other dir must not match");

    // Строка файла дублем папки не считается
    Attach file(QStringLiteral("file"), &table);
    file.setField(QStringLiteral("id"), QStringLiteral("row2"));
    file.setField(QStringLiteral("fileName"), QStringLiteral("f.txt"));
    QVERIFY2(table.findLinkRowByPath(target)==QStringLiteral("row1"),
             "file row must not shadow link row");
}

void TestAttachPath::dupFindFileSource(void)
{
    // Поиск дубля файла по исходному пути: .. чинится,
    // пустой source и чужие пути не матчатся
    const QString srcFile=m_dir+QStringLiteral("/orig.pdf");
    {
        QFile f(srcFile);
        QVERIFY2(f.open(QIODevice::WriteOnly), "fixture file must be writable");
        f.write("data");
        f.close();
    }

    AttachTableData table(nullptr);
    Attach file(QStringLiteral("file"), &table);
    file.setField(QStringLiteral("id"), QStringLiteral("row1"));
    file.setField(QStringLiteral("fileName"), QStringLiteral("orig.pdf"));
    file.setField(QStringLiteral("source"), srcFile);
    table.addAttach(file);

    Attach nosrc(QStringLiteral("file"), &table);
    nosrc.setField(QStringLiteral("id"), QStringLiteral("row2"));
    nosrc.setField(QStringLiteral("fileName"), QStringLiteral("nosrc.txt"));
    table.addAttach(nosrc);

    QCOMPARE(table.findFileAttachRowBySource(srcFile), QStringLiteral("row1"));
    QCOMPARE(table.findFileAttachRowBySource(m_dir+QStringLiteral("/sub/../orig.pdf")),
             QStringLiteral("row1"));
    QVERIFY2(table.findFileAttachRowBySource(m_dir+QStringLiteral("/other.pdf")).isEmpty(),
             "other source must not match");
    QVERIFY2(table.findFileAttachRowBySource(QString()).isEmpty(),
             "empty source must not match");
}

void TestAttachPath::dupFindFileLink(void)
{
    // Линк может указывать и на файл (пикер Add link): такой
    // находится тем же поиском, файлы-копии не мешают
    const QString targetFile=m_dir+QStringLiteral("/linked.pdf");
    {
        QFile f(targetFile);
        QVERIFY2(f.open(QIODevice::WriteOnly), "fixture file must be writable");
        f.write("data");
        f.close();
    }
    const QString copyFile=m_dir+QStringLiteral("/copy.pdf");
    {
        QFile f(copyFile);
        QVERIFY2(f.open(QIODevice::WriteOnly), "fixture file must be writable");
        f.write("data");
        f.close();
    }

    AttachTableData table(nullptr);
    Attach link(QStringLiteral("link"), &table);
    link.setField(QStringLiteral("id"), QStringLiteral("row9"));
    link.setField(QStringLiteral("fileName"), QStringLiteral("linked.pdf"));
    link.setField(QStringLiteral("link"), targetFile);
    table.addAttach(link);

    Attach file(QStringLiteral("file"), &table);
    file.setField(QStringLiteral("id"), QStringLiteral("row8"));
    file.setField(QStringLiteral("fileName"), QStringLiteral("linked.pdf"));
    file.setField(QStringLiteral("source"), copyFile);
    table.addAttach(file);

    QCOMPARE(table.findLinkRowByPath(targetFile), QStringLiteral("row9"));
    QCOMPARE(table.findLinkRowByPath(m_dir+QStringLiteral("/sub/../linked.pdf")),
             QStringLiteral("row9"));
    QVERIFY2(table.findLinkRowByPath(m_dir+QStringLiteral("/nowhere.pdf")).isEmpty(),
             "missing target must not match");
    QVERIFY2(table.findLinkRowByPath(copyFile).isEmpty(),
             "file copy must not match link search");
}

QTEST_APPLESS_MAIN(TestAttachPath)

#include "test_attachpath.moc"
