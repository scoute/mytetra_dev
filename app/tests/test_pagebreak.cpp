// Вставка разрыва страницы на QTest (см. 552).
// Живой MetaEditor: кнопка тулбара вставляет маркер в пустой абзац,
// маркер несет page-break-after и переживает html-раундтрип,
// stripPageBreaksForPrint чистит визуал перед печатью, оставляя разрыв.
// Чужие page-break без маркера не трогаются.
// Нужна рабочая директория: раннер стейджит фикстуры из репо,
// workdir резолвится из applicationFilePath.
#include <QtTest>
#include <QApplication>
#include <QWidget>
#include <QSettings>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QAction>
#include <QDir>
#include <QTemporaryDir>

#include "views/record/MetaEditor.h"
#include "libraries/wyedit/EditorTextArea.h"
#include "libraries/wyedit/formatters/TypefaceFormatter.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppConfigStorage.h"
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
#include "models/appConfig/AppFiles.h"
#include "models/dataBaseConfig/DataBaseConfig.h"

extern AppConfig mytetraConfig;
extern ShortcutManager shortcutManager;

class TestPageBreak : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void insertStripsAndRoundtrips(void);

private:

    QTemporaryDir m_tmp;

    struct EditorKit
    {
        QWidget *win=nullptr;
        MetaEditor *ed=nullptr;
    };

    EditorKit makeEditor(void);
    static void dropEditor(EditorKit &kit);
};

void TestPageBreak::initTestCase(void)
{
    static FixedParameters *fp=new FixedParameters;
    static GlobalParameters *gp=new GlobalParameters;
    static AppFiles *af=new AppFiles;
    static DataBaseConfig *dbc=new DataBaseConfig;
    static TrashMonitoring *tm=new TrashMonitoring;
    static WalkHistory *wh=new WalkHistory;
    static VisitHistory *vh=new VisitHistory;
    static ActionLogger *al=new ActionLogger;
    static PeriodicCheckBase *pcb=new PeriodicCheckBase;
    static PeriodicSyncro *ps=new PeriodicSyncro;
    gp->setMainProgramFile(QCoreApplication::applicationFilePath());
    QVERIFY2(gp->initHeadless(), "headless workdir must init");

    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");
    mytetraConfig.m_conf=new QSettings(m_tmp.filePath(QStringLiteral("test_pagebreak.ini")),
                                       QSettings::IniFormat, this);

    AppContext::instance().init(fp, gp, &mytetraConfig, af, dbc, tm,
                                wh, vh, al, &shortcutManager, pcb, ps,
                                nullptr, nullptr, &gp->cryptHolder());

    mytetraConfig.init();
    shortcutManager.init();
}

void TestPageBreak::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

TestPageBreak::EditorKit TestPageBreak::makeEditor(void)
{
    EditorKit kit;
    kit.win=new QWidget;
    kit.win->show();
    kit.ed=new MetaEditor(kit.win);
    kit.ed->show();
    QApplication::processEvents();
    return kit;
}

void TestPageBreak::dropEditor(EditorKit &kit)
{
    delete kit.win;
    kit.win=nullptr;
    kit.ed=nullptr;
}

void TestPageBreak::insertStripsAndRoundtrips(void)
{
    EditorKit k=makeEditor();

    k.ed->setTextarea(QStringLiteral("<p>First</p><p>Last</p>"));
    QApplication::processEvents();

    // Пустой абзац между First и Last - как Enter в живом редакторе
    // (именно пустой блок с текстом "", а не <br/> из сохраненки:
    // сериализованный <br/> дает символ U+2028, туда вставка не ходит -
    // тот же контракт что у горизонтальной линии)
    QTextDocument *doc=k.ed->textArea->document();
    QTextCursor splitCursor(doc->begin());
    splitCursor.movePosition(QTextCursor::EndOfBlock);
    splitCursor.insertBlock();
    QApplication::processEvents();

    int emptyPos=-1;
    for(QTextBlock b=doc->begin(); b!=doc->end(); b=b.next())
        if(b.text().isEmpty())
        {
            emptyPos=b.position();
            break;
        }
    QVERIFY2(emptyPos>=0, "empty paragraph must exist");

    QTextCursor cursor(doc);
    cursor.setPosition(emptyPos);
    k.ed->textArea->setTextCursor(cursor);

    // Вставка штатной кнопкой тулбара: вся цепочка action->slot
    QAction *insertAction=k.ed->findChild<QAction *>(QStringLiteral("editor_tb_insertPageBreak"));
    QVERIFY2(insertAction!=nullptr, "pagebreak toolbar action must exist");
    insertAction->trigger();
    QApplication::processEvents();

    // Маркер в документе: блок с page-break и ножницами
    bool markerFound=false;
    for(QTextBlock b=doc->begin(); b!=doc->end(); b=b.next())
        if(b.blockFormat().pageBreakPolicy()!=QTextFormat::PageBreak_Auto &&
           b.text().contains(TypefaceFormatter::pageBreakMark()))
            markerFound=true;
    QVERIFY2(markerFound, "pagebreak marker block must be inserted");

    // Маркер переживает html-раундтрип (сохранение заметки)
    QString html=doc->toHtml();
    QVERIFY2(html.contains(QStringLiteral("page-break")), "page-break must survive html roundtrip");

    QScopedPointer<QTextDocument> reloaded(new QTextDocument);
    reloaded->setHtml(html);
    bool markerSurvived=false;
    for(QTextBlock b=reloaded->begin(); b!=reloaded->end(); b=b.next())
        if(b.blockFormat().pageBreakPolicy()!=QTextFormat::PageBreak_Auto &&
           b.text().contains(TypefaceFormatter::pageBreakMark()))
            markerSurvived=true;
    QVERIFY2(markerSurvived, "pagebreak marker must survive reload");

    // Чистка перед печатью: текст и фон уходят, разрыв остается
    QScopedPointer<QTextDocument> printDoc(new QTextDocument);
    printDoc->setHtml(html);
    TypefaceFormatter::stripPageBreaksForPrint(printDoc.data());

    bool cleanBreakFound=false;
    for(QTextBlock b=printDoc->begin(); b!=printDoc->end(); b=b.next())
        if(b.blockFormat().pageBreakPolicy()!=QTextFormat::PageBreak_Auto)
        {
            QVERIFY2(b.text().isEmpty(), "stripped marker must be empty");
            cleanBreakFound=true;
        }
    QVERIFY2(cleanBreakFound, "clean break must remain after strip");
    QVERIFY2(!printDoc->toHtml().contains(TypefaceFormatter::pageBreakMark()),
             "marker char must be gone after strip");

    // Окружение цело: First и Last на месте
    QVERIFY2(printDoc->toPlainText().contains(QStringLiteral("First")), "First must survive strip");
    QVERIFY2(printDoc->toPlainText().contains(QStringLiteral("Last")), "Last must survive strip");

    // Чужой page-break без маркера не трогается
    QScopedPointer<QTextDocument> foreignDoc(new QTextDocument);
    foreignDoc->setHtml(QStringLiteral("<p>Hand</p><p style=\"page-break-after: always;\">made</p>"));
    TypefaceFormatter::stripPageBreaksForPrint(foreignDoc.data());
    QVERIFY2(foreignDoc->toPlainText().contains(QStringLiteral("made")),
             "foreign page-break content must be kept");

    dropEditor(k);
}

QTEST_MAIN(TestPageBreak)

#include "test_pagebreak.moc"
