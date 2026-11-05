// Поиск файла аттача по всем схемам имен на QTest (см. 509, 520).
// Порт attachpathprobe: чистая файловая логика,
// AppContext не нужен, только QCoreApplication.
#include <QtTest>
#include <QFile>
#include <QDir>
#include <QStringList>
#include <QTemporaryDir>

#include "models/attachTable/Attach.h"

class TestAttachPath : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);

    void multiScheme(void);
    void nonFirstSchemeWins(void);
    void firstWins(void);
    void missingIsEmpty(void);

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

QTEST_APPLESS_MAIN(TestAttachPath)

#include "test_attachpath.moc"
