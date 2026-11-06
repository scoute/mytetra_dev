// Кандидаты имен аттача на QTest, линия v2 (см. 492).
// v2-контракт: три схемы без дублей, порядок системное→гибрид→оригинал.
// findExistingFile в v2 нет (фича 509 — только v1): проверяется сам
// список кандидатов и его использование для поиска перебором.
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

    void threeSchemesNoDups(void);
    void exactCandidates(void);
    void fileFoundByScan(void);
    void missingIsEmpty(void);

private:

    QTemporaryDir m_tmp;
    QString m_dir;
    QStringList m_candidates;

    static bool existsByScan(const QString &dir, const QStringList &candidates,
                             QString *found);
};

bool TestAttachPath::existsByScan(const QString &dir, const QStringList &candidates,
                                  QString *found)
{
    // Порядок как в чтении: первый существующий побеждает
    for(const QString &name : candidates)
        if(QFile::exists(dir+QStringLiteral("/")+name))
        {
            if(found!=nullptr)
                *found=dir+QStringLiteral("/")+name;
            return true;
        }

    return false;
}

void TestAttachPath::initTestCase(void)
{
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    m_dir=m_tmp.path();
    m_candidates=Attach::candidateFileNames(QStringLiteral("file"),
                                            QStringLiteral("abc123testid"),
                                            QStringLiteral("test.pdf"));
}

void TestAttachPath::threeSchemesNoDups(void)
{
    // Три схемы без дублей: системное, гибридное, оригинальное
    QCOMPARE(m_candidates.size(), 3);
    QCOMPARE(QSet<QString>(m_candidates.begin(), m_candidates.end()).size(), 3);
    QVERIFY2(m_candidates.contains(QStringLiteral("test.pdf")),
             "original scheme must be listed");
}

void TestAttachPath::exactCandidates(void)
{
    QCOMPARE(m_candidates,
             QStringList() << QStringLiteral("abc123testid.pdf")
                           << QStringLiteral("abc123testid_test.pdf")
                           << QStringLiteral("test.pdf"));
}

void TestAttachPath::fileFoundByScan(void)
{
    // Файл лежит только под вторым именем: находится все равно
    {
        QFile f(m_dir+QStringLiteral("/")+m_candidates.at(1));
        QVERIFY2(f.open(QIODevice::WriteOnly), "fixture must be writable");
        f.write("data");
        f.close();
    }

    QString found;
    QVERIFY2(existsByScan(m_dir, m_candidates, &found), "second-name file must be found");
    QCOMPARE(found, m_dir+QStringLiteral("/")+m_candidates.at(1));

    // Файл под двумя именами: побеждает первое (порядок как в чтении)
    {
        QFile f(m_dir+QStringLiteral("/")+m_candidates.at(0));
        QVERIFY2(f.open(QIODevice::WriteOnly), "fixture must be writable");
        f.write("data");
        f.close();
    }

    QVERIFY2(existsByScan(m_dir, m_candidates, &found), "first-name file must be found");
    QCOMPARE(found, m_dir+QStringLiteral("/")+m_candidates.at(0));
}

void TestAttachPath::missingIsEmpty(void)
{
    // Ничего нет: пусто, а не падение
    const QStringList missing=Attach::candidateFileNames(QStringLiteral("file"),
                                                         QStringLiteral("no-such-id"),
                                                         QStringLiteral("no-such.bin"));
    QString found=QStringLiteral("SENTINEL");
    QVERIFY2(!existsByScan(m_dir, missing, &found), "missing file must not be found");
    QCOMPARE(found, QStringLiteral("SENTINEL"));
}

QTEST_APPLESS_MAIN(TestAttachPath)

#include "test_attachpath.moc"
