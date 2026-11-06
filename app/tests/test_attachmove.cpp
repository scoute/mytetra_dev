// Перенос метки аттача на QTest (см. 493, линия v2).
// Порт attachmoveprobe: абзац метки и дописка в текст закрытой
// заметки. Только файлы и QString, без AppContext и дерева.
#include <QtTest>
#include <QString>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include "libraries/wyedit/formatters/ReferenceFormatter.h"

class TestAttachMove : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);

    void paragraphBasic(void);
    void paragraphNoExt(void);
    void paragraphEscaped(void);
    void paragraphEmpty(void);
    void appendCreate(void);
    void appendExisting(void);
    void appendNoDir(void);

private:

    QTemporaryDir m_tmp;
};

void TestAttachMove::initTestCase(void)
{
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");
}

void TestAttachMove::paragraphBasic(void)
{
    QCOMPARE(ReferenceFormatter::badgeParagraphHtml(
                 QStringLiteral("abc_test.pdf"),
                 QStringLiteral("test.pdf")),
             QStringLiteral("<p><a href=\"attach://abc_test.pdf\">[PDF] test.pdf</a></p>"));
}

void TestAttachMove::paragraphNoExt(void)
{
    // Без расширения — метка [FILE]
    QCOMPARE(ReferenceFormatter::badgeParagraphHtml(
                 QStringLiteral("abc_README"),
                 QStringLiteral("README")),
             QStringLiteral("<p><a href=\"attach://abc_README\">[FILE] README</a></p>"));
}

void TestAttachMove::paragraphEscaped(void)
{
    // Экранирование в href и тексте
    const QString html=ReferenceFormatter::badgeParagraphHtml(
        QStringLiteral("a&b.pdf"),
        QStringLiteral("a\"b.pdf"));
    QVERIFY2(html.contains(QStringLiteral("attach://a&amp;b.pdf")),
             "href must be escaped");
    QVERIFY2(html.contains(QStringLiteral("[PDF] a&quot;b.pdf")),
             "text must be escaped");
}

void TestAttachMove::paragraphEmpty(void)
{
    QVERIFY2(ReferenceFormatter::badgeParagraphHtml(QString(), QStringLiteral("x")).isEmpty(),
             "empty inner must give empty");
    QVERIFY2(ReferenceFormatter::badgeParagraphHtml(QStringLiteral("i"), QString()).isEmpty(),
             "empty display must give empty");
}

void TestAttachMove::appendCreate(void)
{
    // Файла нет: минимальный каркас с меткой
    QString error;
    const QString fresh=m_tmp.filePath(QStringLiteral("fresh.html"));
    QVERIFY2(ReferenceFormatter::appendAttachBadgeToTextFile(
                 fresh, QStringLiteral("abc_t.txt"), QStringLiteral("t.txt"), &error),
             "append must create file");

    QFile f(fresh);
    QVERIFY2(f.open(QIODevice::ReadOnly), "created file must be readable");
    const QString content=QString::fromUtf8(f.readAll());
    QVERIFY2(content.contains(QStringLiteral("attach://abc_t.txt")),
             "badge must be present");
    QVERIFY2(content.contains(QStringLiteral("<html>")),
             "skeleton must be html");
}

void TestAttachMove::appendExisting(void)
{
    // Существующий файл: вставка перед </body>
    const QString exist=m_tmp.filePath(QStringLiteral("exist.html"));
    {
        QFile f(exist);
        QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Truncate),
                 "fixture must be writable");
        f.write("<html><body><p>old</p></body></html>");
    }

    QString error;
    QVERIFY2(ReferenceFormatter::appendAttachBadgeToTextFile(
                 exist, QStringLiteral("abc_n.pdf"), QStringLiteral("n.pdf"), &error),
             "append must succeed");

    QFile f(exist);
    QVERIFY2(f.open(QIODevice::ReadOnly), "file must be readable");
    const QString content=QString::fromUtf8(f.readAll());
    QVERIFY2(content.indexOf(QStringLiteral("attach://abc_n.pdf")) <
             content.indexOf(QStringLiteral("</body>")) &&
             content.contains(QStringLiteral("<p>old</p>")),
             "badge must precede body close, old text kept");
}

void TestAttachMove::appendNoDir(void)
{
    // Нет каталога: отказ с ошибкой
    QString error;
    QVERIFY2(!ReferenceFormatter::appendAttachBadgeToTextFile(
                 m_tmp.filePath(QStringLiteral("nodir/f.html")),
                 QStringLiteral("i"), QStringLiteral("f.txt"), &error),
             "append without dir must fail");
    QVERIFY2(!error.isEmpty(), "error must be set");
}

QTEST_APPLESS_MAIN(TestAttachMove)

#include "test_attachmove.moc"
