// Вырезание/копирование метки аттача на QTest, линия v2 (см. 507).
// Порт attachcutprobe: живой MetaEditor, две поддельные директории
// записей с реальным файлом в QTemporaryDir. Проверяется настоящий код:
// createMimeDataFromSelection кладет MIME, paste доделывает вставку
// сигналом attachBadgesPasted с парами href-путь.
// Нужна рабочая директория: раннер стейджит фикстуры из репо,
// workdir резолвится из applicationFilePath.
#include <QtTest>
#include <QApplication>
#include <QWidget>
#include <QSettings>
#include <QTextCursor>
#include <QDir>
#include <QFile>
#include <QClipboard>
#include <QMimeData>
#include <QTemporaryDir>

#include "views/record/MetaEditor.h"
#include "libraries/wyedit/EditorTextArea.h"
#include "libraries/wyedit/formatters/ReferenceFormatter.h"
#include "models/appConfig/AppConfig.h"
#include "models/attachTable/Attach.h"
#include "libraries/GlobalParameters.h"
#include "libraries/ShortcutManager.h"

extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;
extern ShortcutManager shortcutManager;

class TestAttachCut : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void cutCopyMime(void);
    void paste(void);
    void cut(void);
    void attachNames(void);

private:

    QTemporaryDir m_tmp;
    QString m_rec1;
    QString m_rec2;

    struct EditorKit
    {
        QWidget *win=nullptr;
        MetaEditor *ed=nullptr;
    };

    EditorKit makeEditor(void);
    static void dropEditor(EditorKit &kit);
    void selectAll(EditorKit &kit);
};

void TestAttachCut::initTestCase(void)
{
    globalParameters.setMainProgramFile(QCoreApplication::applicationFilePath());
    QVERIFY2(globalParameters.initHeadless(), "headless workdir must init");

    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");
    mytetraConfig.m_conf=new QSettings(m_tmp.filePath(QStringLiteral("test_attachcut.ini")),
                                       QSettings::IniFormat, this);

    mytetraConfig.init();
    shortcutManager.init();

    // Две поддельные записи с реальным файлом в первой
    m_rec1=m_tmp.filePath(QStringLiteral("attachrec1"));
    m_rec2=m_tmp.filePath(QStringLiteral("attachrec2"));
    QVERIFY2(QDir().mkpath(m_rec1), "rec1 must be created");
    QVERIFY2(QDir().mkpath(m_rec2), "rec2 must be created");
    QFile srcFile(m_rec1+QStringLiteral("/aaa_test.pdf"));
    QVERIFY2(srcFile.open(QIODevice::WriteOnly), "fixture file must be writable");
    srcFile.write("fakepdf");
    srcFile.close();
}

void TestAttachCut::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

TestAttachCut::EditorKit TestAttachCut::makeEditor(void)
{
    EditorKit kit;
    kit.win=new QWidget;
    kit.win->show();
    kit.ed=new MetaEditor(kit.win);
    kit.ed->show();
    QApplication::processEvents();
    return kit;
}

void TestAttachCut::dropEditor(EditorKit &kit)
{
    delete kit.win;
    kit.win=nullptr;
    kit.ed=nullptr;
}

void TestAttachCut::selectAll(EditorKit &kit)
{
    QTextCursor c(kit.ed->textArea->document());
    c.select(QTextCursor::Document);
    kit.ed->textArea->setTextCursor(c);
    QApplication::processEvents();
}

void TestAttachCut::cutCopyMime(void)
{
    EditorKit k=makeEditor();

    // Запись 1: метка на существующий файл
    QVERIFY2(k.ed->setWorkDirectory(m_rec1), "rec1 must attach");
    k.ed->setTextarea(QStringLiteral("<p><a href=\"attach://aaa_test.pdf\">[PDF] aaa_test.pdf</a></p>"));
    QApplication::processEvents();

    // Выделить метку целиком и скопировать
    selectAll(k);
    k.ed->textArea->copy();
    QApplication::processEvents();

    // Прямой вызов переопределения: работает ли сам скан меток
    QMimeData *directMime=k.ed->textArea->createMimeDataFromSelection();
    QVERIFY2(directMime!=nullptr, "direct mime must exist");
    QVERIFY2(directMime->hasFormat(QString::fromLatin1(ReferenceFormatter::attachMimeTypeName())),
             "direct mime must carry attach format");
    delete directMime;

    const QMimeData *cb=QApplication::clipboard()->mimeData();
    QVERIFY2(cb!=nullptr, "clipboard must exist");
    QVERIFY2(cb->hasFormat(QString::fromLatin1(ReferenceFormatter::attachMimeTypeName())),
             "clipboard must carry attach format");

    const QStringList pairLines=QString::fromUtf8(
        cb->data(QString::fromLatin1(ReferenceFormatter::attachMimeTypeName()))
        ).split(QStringLiteral("\n"), Qt::SkipEmptyParts);
    QCOMPARE(pairLines.size(), 2);
    QCOMPARE(pairLines.value(0), QStringLiteral("attach://aaa_test.pdf"));
    QCOMPARE(pairLines.value(1), m_rec1+QStringLiteral("/aaa_test.pdf"));

    dropEditor(k);
}

void TestAttachCut::paste(void)
{
    EditorKit k=makeEditor();

    int badgeSignals=0;
    QByteArray lastBadgeMime;
    connect(k.ed->textArea, &EditorTextArea::attachBadgesPasted,
            [&](const QByteArray &mime) {
                ++badgeSignals;
                lastBadgeMime=mime;
            });

    // Вставка в запись 2: HTML метки приезжает, сигнал доделки с парами
    QVERIFY2(k.ed->setWorkDirectory(m_rec2), "rec2 must attach");
    k.ed->setTextarea(QStringLiteral("<p>empty</p>"));

    // Сначала положить метку в буфер: копируем из записи 1
    QVERIFY2(k.ed->setWorkDirectory(m_rec1), "rec1 must attach");
    k.ed->setTextarea(QStringLiteral("<p><a href=\"attach://aaa_test.pdf\">[PDF] aaa_test.pdf</a></p>"));
    QApplication::processEvents();
    selectAll(k);
    k.ed->textArea->copy();
    QApplication::processEvents();

    QVERIFY2(k.ed->setWorkDirectory(m_rec2), "rec2 must reattach");
    k.ed->setTextarea(QStringLiteral("<p>empty</p>"));
    k.ed->textArea->paste();
    QApplication::processEvents();

    QCOMPARE(badgeSignals, 1);
    QVERIFY2(!lastBadgeMime.isEmpty(), "paste mime must be nonempty");
    QVERIFY2(k.ed->textArea->document()->toHtml().contains(QStringLiteral("attach://")),
             "badge html must arrive");

    dropEditor(k);
}

void TestAttachCut::cut(void)
{
    EditorKit k=makeEditor();

    // Вырезание: текст метки уходит, MIME в буфере как при копировании
    QVERIFY2(k.ed->setWorkDirectory(m_rec1), "rec1 must attach");
    k.ed->setTextarea(QStringLiteral("<p><a href=\"attach://aaa_test.pdf\">[PDF] aaa_test.pdf</a></p>"));
    selectAll(k);
    k.ed->textArea->cut();
    QApplication::processEvents();

    QVERIFY2(!k.ed->textArea->document()->toPlainText().contains(QStringLiteral("aaa_test")),
             "cut must remove badge text");
    const QMimeData *cb2=QApplication::clipboard()->mimeData();
    QVERIFY2(cb2->hasFormat(QString::fromLatin1(ReferenceFormatter::attachMimeTypeName())),
             "cut mime must be kept");

    dropEditor(k);
}

void TestAttachCut::attachNames(void)
{
    // Имена аттачей по умолчанию системные (id.extension): короткие
    // ASCII-имена без пробелов и национальных символов. Кандидаты
    // покрывают все схемы, гибрид служит дублем при коллизии оригинала
    QCOMPARE(mytetraConfig.get_attachNameMode(),
             AppConfig::AttachNameSystem);
    QCOMPARE(Attach::constructOriginalFileName(QStringLiteral("file"), QStringLiteral("someid"),
             QStringLiteral("My Document v2.PDF")), QStringLiteral("My_Document_v2.PDF"));
    QCOMPARE(Attach::constructOriginalFileName(QStringLiteral("file"), QStringLiteral("someid"),
             QStringLiteral("Договор 12.pdf")), QStringLiteral("Договор_12.pdf"));
    QCOMPARE(Attach::fileNameForMode(AppConfig::AttachNameHybrid, QStringLiteral("file"),
             QStringLiteral("abc123"), QStringLiteral("test.pdf")),
             QStringLiteral("abc123_test.pdf"));
    {
        const QStringList candidates=Attach::candidateFileNames(QStringLiteral("file"),
             QStringLiteral("abc123"), QStringLiteral("test.pdf"));
        QVERIFY2(candidates.contains(QStringLiteral("abc123_test.pdf")) &&
                 candidates.contains(QStringLiteral("test.pdf")),
                 "candidates must cover all schemes");
    }
}

QTEST_MAIN(TestAttachCut)

#include "test_attachcut.moc"
