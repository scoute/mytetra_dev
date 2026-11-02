// Перенос метки аттача: абзац метки и дописка в текст закрытой
// заметки. Только файлы и QString, без AppContext и дерева:
// вставка в чужую запись и удаление из своей проверяются ревью
// и ручным QA, здесь фиксируется формат и файловые операции.
#include <QCoreApplication>
#include <QDebug>
#include <QString>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include "libraries/wyedit/formatters/ReferenceFormatter.h"

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

    // Абзац метки: ссылка плюс [EXT] имя
    check(ReferenceFormatter::badgeParagraphHtml(
              QStringLiteral("abc_test.pdf"),
              QStringLiteral("test.pdf"))
          ==QStringLiteral("<p><a href=\"attach://abc_test.pdf\">[PDF] test.pdf</a></p>"),
          "MOVE-PARAGRAPH-BASIC");

    // Без расширения — метка [FILE]
    check(ReferenceFormatter::badgeParagraphHtml(
              QStringLiteral("abc_README"),
              QStringLiteral("README"))
          ==QStringLiteral("<p><a href=\"attach://abc_README\">[FILE] README</a></p>"),
          "MOVE-PARAGRAPH-NOEXT");

    // Экранирование в href и тексте
    {
        const QString html=ReferenceFormatter::badgeParagraphHtml(
            QStringLiteral("a&b.pdf"),
            QStringLiteral("a\"b.pdf"));
        check(html.contains(QStringLiteral("attach://a&amp;b.pdf")) &&
              html.contains(QStringLiteral("[PDF] a&quot;b.pdf")),
              "MOVE-PARAGRAPH-ESCAPED");
    }

    check(ReferenceFormatter::badgeParagraphHtml(QString(), QStringLiteral("x")).isEmpty(),
          "MOVE-PARAGRAPH-EMPTY-INNER");
    check(ReferenceFormatter::badgeParagraphHtml(QStringLiteral("i"), QString()).isEmpty(),
          "MOVE-PARAGRAPH-EMPTY-DISPLAY");

    QTemporaryDir tmp;
    check(tmp.isValid(), "MOVE-TMPDIR");

    // Файла нет: минимальный каркас с меткой
    QString error;
    const QString fresh=tmp.filePath(QStringLiteral("fresh.html"));
    check(ReferenceFormatter::appendAttachBadgeToTextFile(
              fresh, QStringLiteral("abc_t.txt"), QStringLiteral("t.txt"), &error),
          "MOVE-APPEND-CREATE");
    {
        QFile f(fresh);
        f.open(QIODevice::ReadOnly);
        const QString content=QString::fromUtf8(f.readAll());
        check(content.contains(QStringLiteral("attach://abc_t.txt")) &&
              content.contains(QStringLiteral("<html>")),
              "MOVE-APPEND-CREATE-CONTENT");
    }

    // Существующий файл: вставка перед </body>
    const QString exist=tmp.filePath(QStringLiteral("exist.html"));
    {
        QFile f(exist);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write("<html><body><p>old</p></body></html>");
    }
    error.clear();
    check(ReferenceFormatter::appendAttachBadgeToTextFile(
              exist, QStringLiteral("abc_n.pdf"), QStringLiteral("n.pdf"), &error),
          "MOVE-APPEND-EXISTING");
    {
        QFile f(exist);
        f.open(QIODevice::ReadOnly);
        const QString content=QString::fromUtf8(f.readAll());
        check(content.indexOf(QStringLiteral("attach://abc_n.pdf")) <
              content.indexOf(QStringLiteral("</body>")) &&
              content.contains(QStringLiteral("<p>old</p>")),
              "MOVE-APPEND-BEFORE-BODY");
    }

    // Нет каталога: отказ с ошибкой
    error.clear();
    check(!ReferenceFormatter::appendAttachBadgeToTextFile(
              tmp.filePath(QStringLiteral("nodir/f.html")),
              QStringLiteral("i"), QStringLiteral("f.txt"), &error),
          "MOVE-APPEND-NODIR-FAILS");
    check(!error.isEmpty(), "MOVE-APPEND-NODIR-ERROR");

    qDebug() << (fails==0 ? "ATTACHMOVEPROBE-OK" : "ATTACHMOVEPROBE-BAD");
    return fails;
}
