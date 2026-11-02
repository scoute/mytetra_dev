// Вырезание/копирование метки аттача и вставка в другую заметку.
// Живой MetaEditor, две поддельные директории записей с реальным файлом.
// Проверяется настоящий код: createMimeDataFromSelection кладет MIME,
// paste доделывает вставку сигналом attachBadgesPasted с парами href-путь.
#include <QApplication>
#include <QWidget>
#include <QDebug>
#include <QSettings>
#include <QTextCursor>
#include <QDir>
#include <QFile>
#include <QClipboard>
#include <QMimeData>

#include "views/record/MetaEditor.h"
#include "libraries/wyedit/EditorTextArea.h"
#include "libraries/wyedit/formatters/ReferenceFormatter.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppConfigStorage.h"
#include "models/attachTable/Attach.h"
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
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppFiles.h"
#include "models/dataBaseConfig/DataBaseConfig.h"

extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;
extern FixedParameters fixedParameters;
extern AppFiles mytetraFiles;
extern DataBaseConfig dataBaseConfig;
extern TrashMonitoring trashMonitoring;
extern WalkHistory walkHistory;
extern VisitHistory visitHistory;
extern ActionLogger actionLogger;
extern ShortcutManager shortcutManager;
extern PeriodicCheckBase periodicCheckBase;
extern PeriodicSyncro periodicSyncro;

static int fails=0;
static void check(bool ok, const char *name)
{
    qDebug() << (ok ? "PASS" : "FAIL") << name;
    if(!ok)
        ++fails;
}

static int badgeSignals=0;
static QByteArray lastBadgeMime;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    globalParameters.setMainProgramFile(QString::fromLocal8Bit(argv[0]));

    if(!globalParameters.initHeadless())
    {
        qDebug() << QStringLiteral("NOWORKDIR");
        return 1;
    }

    QFile::remove(QStringLiteral("/tmp/opencode/probeattach.ini"));
    mytetraConfig.m_conf=new QSettings(QStringLiteral("/tmp/opencode/probeattach.ini"),
                                       QSettings::IniFormat);

    AppContext::instance().init(&fixedParameters,
                                &globalParameters,
                                &mytetraConfig,
                                &mytetraFiles,
                                &dataBaseConfig,
                                &trashMonitoring,
                                &walkHistory,
                                &visitHistory,
                                &actionLogger,
                                &shortcutManager,
                                &periodicCheckBase,
                                &periodicSyncro,
                                nullptr,
                                nullptr,
                                &globalParameters.cryptHolder());

    mytetraConfig.init();
    shortcutManager.init();

    // Две поддельные записи с реальным файлом в первой
    QDir().mkpath(QStringLiteral("/tmp/opencode/attachrec1"));
    QDir().mkpath(QStringLiteral("/tmp/opencode/attachrec2"));
    QFile srcFile(QStringLiteral("/tmp/opencode/attachrec1/aaa_test.pdf"));
    srcFile.open(QIODevice::WriteOnly);
    srcFile.write("fakepdf");
    srcFile.close();

    QWidget win;
    win.show();

    MetaEditor *ed=new MetaEditor(&win);
    ed->show();
    QApplication::processEvents();

    QObject::connect(ed->textArea, &EditorTextArea::attachBadgesPasted,
                     [&](const QByteArray &mime) {
                         ++badgeSignals;
                         lastBadgeMime=mime;
                     });

    // Запись 1: метка на существующий файл
    check(ed->setWorkDirectory(QStringLiteral("/tmp/opencode/attachrec1")), "CUT-SETWORKDIR");
    ed->setTextarea(QStringLiteral("<p><a href=\"attach://aaa_test.pdf\">[PDF] aaa_test.pdf</a></p>"));
    QApplication::processEvents();

    // Выделить метку целиком и скопировать
    {
        QTextCursor c(ed->textArea->document());
        c.select(QTextCursor::Document);
        ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    ed->textArea->copy();
    QApplication::processEvents();

    // Прямой вызов переопределения: работает ли сам скан меток
    QMimeData *directMime=ed->textArea->createMimeDataFromSelection();
    check(directMime!=nullptr, "CUT-DIRECT-NONNULL");
    check(directMime!=nullptr &&
          directMime->hasFormat(QString::fromLatin1(ReferenceFormatter::attachMimeTypeName())),
          "CUT-DIRECT-MIME-VISIBLE");
    delete directMime;

    const QMimeData *cb=QApplication::clipboard()->mimeData();
    check(cb!=nullptr, "CUT-CLIPBOARD-EXISTS");
    check(cb->hasFormat(QString::fromLatin1(ReferenceFormatter::attachMimeTypeName())),
          "CUT-MIME-VISIBLE");

    const QStringList pairLines=QString::fromUtf8(
        cb->data(QString::fromLatin1(ReferenceFormatter::attachMimeTypeName()))
        ).split(QStringLiteral("\n"), Qt::SkipEmptyParts);
    check(pairLines.size()==2, "CUT-MIME-PAIRS");
    check(pairLines.value(0)==QStringLiteral("attach://aaa_test.pdf"), "CUT-MIME-HREF");
    check(pairLines.value(1)==QStringLiteral("/tmp/opencode/attachrec1/aaa_test.pdf"),
          "CUT-MIME-PATH");

    // Вставка в запись 2: HTML метки приезжает, сигнал доделки с парами
    badgeSignals=0;
    lastBadgeMime.clear();
    check(ed->setWorkDirectory(QStringLiteral("/tmp/opencode/attachrec2")), "PASTE-SETWORKDIR");
    ed->setTextarea(QStringLiteral("<p>empty</p>"));
    ed->textArea->paste();
    QApplication::processEvents();

    check(badgeSignals==1, "PASTE-SIGNAL-EMITTED");
    check(!lastBadgeMime.isEmpty(), "PASTE-MIME-NONEMPTY");
    check(ed->textArea->document()->toHtml().contains(QStringLiteral("attach://")),
          "PASTE-HTML-ARRIVED");

    // Вырезание: текст метки уходит, MIME в буфере как при копировании
    ed->setWorkDirectory(QStringLiteral("/tmp/opencode/attachrec1"));
    ed->setTextarea(QStringLiteral("<p><a href=\"attach://aaa_test.pdf\">[PDF] aaa_test.pdf</a></p>"));
    {
        QTextCursor c(ed->textArea->document());
        c.select(QTextCursor::Document);
        ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    badgeSignals=0;
    ed->textArea->cut();
    QApplication::processEvents();

    check(!ed->textArea->document()->toPlainText().contains(QStringLiteral("aaa_test")),
          "CUT-TEXT-REMOVED");
    const QMimeData *cb2=QApplication::clipboard()->mimeData();
    check(cb2->hasFormat(QString::fromLatin1(ReferenceFormatter::attachMimeTypeName())),
          "CUT-MIME-KEPT");

    // Имена аттачей по умолчанию оригинальные: пробелы в подчеркивания,
    // регистр и кириллица целы. Кандидаты покрывают все схемы,
    // гибрид служит дублем при коллизии оригинала
    check(AppContext::instance().appConfig()->storageSection()->get_attachNameMode()
          ==AppConfig::AttachNameOriginal, "NAME-DEFAULT-ORIGINAL");
    check(Attach::constructOriginalFileName(QStringLiteral("file"), QStringLiteral("someid"),
          QStringLiteral("My Document v2.PDF"))==QStringLiteral("My_Document_v2.PDF"),
          "NAME-SPACES-TO-UNDERSCORES");
    check(Attach::constructOriginalFileName(QStringLiteral("file"), QStringLiteral("someid"),
          QStringLiteral("Договор 12.pdf"))==QStringLiteral("Договор_12.pdf"),
          "NAME-CYRILLIC-KEPT");
    check(Attach::fileNameForMode(AppConfig::AttachNameHybrid, QStringLiteral("file"),
          QStringLiteral("abc123"), QStringLiteral("test.pdf"))
          ==QStringLiteral("abc123_test.pdf"), "NAME-HYBRID-FALLBACK");
    {
        const QStringList candidates=Attach::candidateFileNames(QStringLiteral("file"),
          QStringLiteral("abc123"), QStringLiteral("test.pdf"));
        check(candidates.contains(QStringLiteral("abc123_test.pdf")) &&
              candidates.contains(QStringLiteral("test.pdf")),
              "NAME-CANDIDATES-COVER-ALL");
    }

    qDebug() << (fails==0 ? "ATTACHCUTPROBE-OK" : "ATTACHCUTPROBE-BAD");
    return fails;
}
