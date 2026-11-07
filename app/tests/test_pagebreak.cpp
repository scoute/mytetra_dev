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

    // Бэкспейс в начале абзаца за маркером сносит весь маркер
    // (текст, фон и page-break), а не ест символы и не трогает
    // пустые строки до разрыва. Курсор встает на точку стыка
    void backspaceAfterMarkerRemovesIt(void);

    // Delete в конце абзаца перед маркером - симметрично
    void deleteBeforeMarkerRemovesIt(void);

    // Бэкспейс внутри текста маркера сносит весь маркер целиком:
    // огрызок с фоном и разрывом оставлять нельзя
    void backspaceInsideMarkerRemovesIt(void);

    // Зомби-маркер (текст уже съеден, пустой блок с фоном и разрывом):
    // бэкспейс за ним убирает блок полностью
    void backspaceAfterZombieRemovesIt(void);

    // Выделение с запасом до и после маркера: удаляется одним махом
    // без полосок (это и раньше работало, регрессия)
    void selectionAcrossMarkerRemovesIt(void);

    // Выделение начинается внутри маркера: маркер уходит целиком,
    // огрызков с фоном не остается
    void selectionFromInsideMarkerRemovesIt(void);

    // Чистка зомби перед печатью: пустой блок с фоном и разрывом,
    // но без ножниц. Разрыв остается, серая полоса уходит
    void stripCleansZombie(void);

    // Enter внутри маркера: маркер цел, под ним новый обычный абзац
    // без фона и разрыва. Половинок с разрывом не плодится
    void enterInsideMarkerMakesCleanBlock(void);

    // Ctrl+X с заходом в маркер: режется весь маркер целиком
    void cutAcrossMarkerRemovesIt(void);

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


static QString dumpBlocks(QTextDocument *doc)
{
    QStringList lines;

    for(QTextBlock b=doc->begin(); b.isValid(); b=b.next())
        lines << QStringLiteral("[break=%1 bg=%2 text=%3]")
                 .arg(b.blockFormat().pageBreakPolicy())
                 .arg(b.blockFormat().background().color().name())
                 .arg(b.text());

    return lines.join(QStringLiteral(" | "));
}


static int markerBlockCount(QTextDocument *doc)
{
    int n=0;

    for(QTextBlock b=doc->begin(); b.isValid(); b=b.next())
        if(b.blockFormat().pageBreakPolicy()!=QTextFormat::PageBreak_Auto)
            ++n;

    return n;
}


// Документ First / пустой / МАРКЕР / Last через штатную кнопку:
// курсор после вставки стоит в начале Last
static QTextBlock insertMarkerBetween(QObject *tc, MetaEditor *ed)
{
    Q_UNUSED(tc);
    QTextDocument *doc=ed->textArea->document();

    QTextCursor split(doc->begin());
    split.movePosition(QTextCursor::EndOfBlock);
    split.insertBlock();
    QApplication::processEvents();

    int emptyPos=-1;
    for(QTextBlock b=doc->begin(); b.isValid(); b=b.next())
        if(b.text().isEmpty())
        {
            emptyPos=b.position();
            break;
        }
    Q_ASSERT(emptyPos>=0);

    QTextCursor cursor(doc);
    cursor.setPosition(emptyPos);
    ed->textArea->setTextCursor(cursor);

    QAction *insertAction=ed->findChild<QAction *>(QStringLiteral("editor_tb_insertPageBreak"));
    Q_ASSERT(insertAction!=nullptr);
    insertAction->trigger();
    QApplication::processEvents();

    for(QTextBlock b=doc->begin(); b.isValid(); b=b.next())
        if(b.blockFormat().pageBreakPolicy()!=QTextFormat::PageBreak_Auto &&
           b.text().contains(TypefaceFormatter::pageBreakMark()))
            return b;

    Q_ASSERT(false);
    return QTextBlock();
}


void TestPageBreak::backspaceAfterMarkerRemovesIt(void)
{
    EditorKit k=makeEditor();
    k.ed->setTextarea(QStringLiteral("<p>First</p><p>Last</p>"));
    QApplication::processEvents();

    insertMarkerBetween(this, k.ed);

    QTextDocument *doc=k.ed->textArea->document();
    qDebug() << "BEFORE backspace:" << dumpBlocks(doc);

    // Курсор в начало Last (как после вставки): бэкспейс должен снести
    // маркер целиком, курсор - на конец First
    QTextCursor cursor(doc);
    QTextBlock last=doc->lastBlock();
    cursor.setPosition(last.position());
    k.ed->textArea->setTextCursor(cursor);

    QTest::keyClick(k.ed->textArea, Qt::Key_Backspace);
    QApplication::processEvents();

    qDebug() << "AFTER backspace:" << dumpBlocks(doc);

    QCOMPARE(markerBlockCount(doc), 0);
    QVERIFY2(!doc->toPlainText().contains(TypefaceFormatter::pageBreakMark()),
             "marker text must be gone");
    QVERIFY2(doc->toPlainText().contains(QStringLiteral("First")), "First must survive");
    QVERIFY2(doc->toPlainText().contains(QStringLiteral("Last")), "Last must survive");

    // Курсор на точке стыка (конец First), а не на середине страницы
    QTextBlock first=doc->begin();
    QCOMPARE(k.ed->textArea->textCursor().position(),
             first.position()+first.text().length());

    dropEditor(k);
}


void TestPageBreak::deleteBeforeMarkerRemovesIt(void)
{
    EditorKit k=makeEditor();
    k.ed->setTextarea(QStringLiteral("<p>First</p><p>Last</p>"));
    QApplication::processEvents();

    insertMarkerBetween(this, k.ed);

    QTextDocument *doc=k.ed->textArea->document();

    // Курсор в конец First: Delete должен снести маркер целиком
    QTextCursor cursor(doc->begin());
    cursor.movePosition(QTextCursor::EndOfBlock);
    k.ed->textArea->setTextCursor(cursor);

    QTest::keyClick(k.ed->textArea, Qt::Key_Delete);
    QApplication::processEvents();

    qDebug() << "AFTER delete:" << dumpBlocks(doc);

    QCOMPARE(markerBlockCount(doc), 0);
    QVERIFY2(!doc->toPlainText().contains(TypefaceFormatter::pageBreakMark()),
             "marker text must be gone");

    dropEditor(k);
}


void TestPageBreak::backspaceInsideMarkerRemovesIt(void)
{
    EditorKit k=makeEditor();
    k.ed->setTextarea(QStringLiteral("<p>First</p><p>Last</p>"));
    QApplication::processEvents();

    QTextBlock marker=insertMarkerBetween(this, k.ed);

    QTextDocument *doc=k.ed->textArea->document();

    // Курсор в середину текста маркера: бэкспейс сносит весь маркер,
    // а не один символ (иначе остается полоска с разрывом)
    QTextCursor cursor(doc);
    cursor.setPosition(marker.position()+2);
    k.ed->textArea->setTextCursor(cursor);

    QTest::keyClick(k.ed->textArea, Qt::Key_Backspace);
    QApplication::processEvents();

    qDebug() << "AFTER inner backspace:" << dumpBlocks(doc);

    QCOMPARE(markerBlockCount(doc), 0);
    QVERIFY2(!doc->toPlainText().contains(TypefaceFormatter::pageBreakMark()),
             "marker text must be gone");

    dropEditor(k);
}


void TestPageBreak::backspaceAfterZombieRemovesIt(void)
{
    EditorKit k=makeEditor();
    k.ed->setTextarea(QStringLiteral("<p>First</p><p>Last</p>"));
    QApplication::processEvents();

    QTextBlock marker=insertMarkerBetween(this, k.ed);

    QTextDocument *doc=k.ed->textArea->document();

    // Зомби: текст маркера съеден (как у пользователя), остался пустой
    // блок с фоном и page-break - светлая полоска ни на что не реагирует
    QTextCursor eat(doc);
    eat.setPosition(marker.position());
    eat.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    eat.removeSelectedText();
    QApplication::processEvents();

    qDebug() << "ZOMBIE state:" << dumpBlocks(doc);

    // Курсор в начало Last: бэкспейс должен убрать зомби-блок полностью
    QTextCursor cursor(doc);
    cursor.setPosition(doc->lastBlock().position());
    k.ed->textArea->setTextCursor(cursor);

    QTest::keyClick(k.ed->textArea, Qt::Key_Backspace);
    QApplication::processEvents();

    qDebug() << "AFTER zombie backspace:" << dumpBlocks(doc);

    QCOMPARE(markerBlockCount(doc), 0);

    dropEditor(k);
}


void TestPageBreak::selectionAcrossMarkerRemovesIt(void)
{
    EditorKit k=makeEditor();
    k.ed->setTextarea(QStringLiteral("<p>First</p><p>Last</p>"));
    QApplication::processEvents();

    insertMarkerBetween(this, k.ed);

    QTextDocument *doc=k.ed->textArea->document();

    // Выделение с запасом: середина First - середина Last
    QTextCursor cursor(doc);
    cursor.setPosition(doc->begin().position()+2);
    cursor.setPosition(doc->lastBlock().position()+2, QTextCursor::KeepAnchor);
    k.ed->textArea->setTextCursor(cursor);

    QTest::keyClick(k.ed->textArea, Qt::Key_Backspace);
    QApplication::processEvents();

    qDebug() << "AFTER wide selection:" << dumpBlocks(doc);

    QCOMPARE(markerBlockCount(doc), 0);
    QVERIFY2(!doc->toPlainText().contains(TypefaceFormatter::pageBreakMark()),
             "marker text must be gone");

    dropEditor(k);
}


void TestPageBreak::selectionFromInsideMarkerRemovesIt(void)
{
    EditorKit k=makeEditor();
    k.ed->setTextarea(QStringLiteral("<p>First</p><p>Last</p>"));
    QApplication::processEvents();

    QTextBlock marker=insertMarkerBetween(this, k.ed);

    QTextDocument *doc=k.ed->textArea->document();

    // Выделение начинается внутри маркера, кончается в Last:
    // маркер уходит целиком
    QTextCursor cursor(doc);
    cursor.setPosition(marker.position()+3);
    cursor.setPosition(doc->lastBlock().position()+2, QTextCursor::KeepAnchor);
    k.ed->textArea->setTextCursor(cursor);

    QTest::keyClick(k.ed->textArea, Qt::Key_Backspace);
    QApplication::processEvents();

    qDebug() << "AFTER inner-start selection:" << dumpBlocks(doc);

    QCOMPARE(markerBlockCount(doc), 0);
    QVERIFY2(!doc->toPlainText().contains(TypefaceFormatter::pageBreakMark()),
             "marker text must be gone");
    QVERIFY2(doc->toPlainText().contains(QStringLiteral("First")), "First must survive");

    dropEditor(k);
}


void TestPageBreak::stripCleansZombie(void)
{
    // Зомби: пустой блок с page-break и нашим фоном, но без ножниц.
    // В печати серая полоса должна уйти, разрыв - остаться
    QScopedPointer<QTextDocument> zombieDoc(new QTextDocument);
    zombieDoc->setHtml(QStringLiteral("<p>First</p><p style=\"page-break-after: always; background-color: #e8e8e8;\"></p><p>Last</p>"));

    int zombieBreaks=0;
    for(QTextBlock b=zombieDoc->begin(); b.isValid(); b=b.next())
        if(b.blockFormat().pageBreakPolicy()!=QTextFormat::PageBreak_Auto)
            ++zombieBreaks;
    QVERIFY2(zombieBreaks==1, "zombie break must be present");

    TypefaceFormatter::stripPageBreaksForPrint(zombieDoc.data());

    // Разрыв остался
    int cleanBreaks=0;
    for(QTextBlock b=zombieDoc->begin(); b.isValid(); b=b.next())
        if(b.blockFormat().pageBreakPolicy()!=QTextFormat::PageBreak_Auto)
            ++cleanBreaks;
    QCOMPARE(cleanBreaks, 1);

    // Серая полоса ушла из html
    QVERIFY2(!zombieDoc->toHtml().contains(QStringLiteral("#e8e8e8")) &&
             !zombieDoc->toHtml().contains(QStringLiteral("e8e8e8")),
             "zombie background must be gone after strip");

    // Чужой пустой page-break без нашего фона не трогается
    QScopedPointer<QTextDocument> foreignDoc(new QTextDocument);
    foreignDoc->setHtml(QStringLiteral("<p>Hand</p><p style=\"page-break-after: always;\"></p><p>made</p>"));
    TypefaceFormatter::stripPageBreaksForPrint(foreignDoc.data());

    int foreignBreaks=0;
    for(QTextBlock b=foreignDoc->begin(); b.isValid(); b=b.next())
        if(b.blockFormat().pageBreakPolicy()!=QTextFormat::PageBreak_Auto)
            ++foreignBreaks;
    QCOMPARE(foreignBreaks, 1);
}


void TestPageBreak::enterInsideMarkerMakesCleanBlock(void)
{
    EditorKit k=makeEditor();
    k.ed->setTextarea(QStringLiteral("<p>First</p><p>Last</p>"));
    QApplication::processEvents();

    QTextBlock marker=insertMarkerBetween(this, k.ed);

    QTextDocument *doc=k.ed->textArea->document();

    // Enter в середине маркера: маркер цел, под ним обычный абзац
    QTextCursor cursor(doc);
    cursor.setPosition(marker.position()+2);
    k.ed->textArea->setTextCursor(cursor);

    QTest::keyClick(k.ed->textArea, Qt::Key_Enter);
    QApplication::processEvents();

    qDebug() << "AFTER enter in marker:" << dumpBlocks(doc);

    // Маркер один и с текстом
    int markers=0;
    for(QTextBlock b=doc->begin(); b.isValid(); b=b.next())
        if(b.blockFormat().pageBreakPolicy()!=QTextFormat::PageBreak_Auto)
        {
            ++markers;
            QVERIFY2(b.text().contains(TypefaceFormatter::pageBreakMark()),
                     "surviving break must keep marker text");
        }
    QCOMPARE(markers, 1);

    // Новый абзац под маркером - обычный, без фона и разрыва
    QTextBlock fresh=marker.next();
    QVERIFY2(fresh.isValid(), "block after marker must exist");
    QCOMPARE(fresh.blockFormat().pageBreakPolicy(), QTextFormat::PageBreak_Auto);

    dropEditor(k);
}


void TestPageBreak::cutAcrossMarkerRemovesIt(void)
{
    EditorKit k=makeEditor();
    k.ed->setTextarea(QStringLiteral("<p>First</p><p>Last</p>"));
    QApplication::processEvents();

    QTextBlock marker=insertMarkerBetween(this, k.ed);

    QTextDocument *doc=k.ed->textArea->document();

    // Ctrl+X с заходом в маркер: режется весь маркер целиком
    QTextCursor cursor(doc);
    cursor.setPosition(marker.position()+3);
    cursor.setPosition(doc->lastBlock().position()+2, QTextCursor::KeepAnchor);
    k.ed->textArea->setTextCursor(cursor);

    QTest::keyClick(k.ed->textArea, Qt::Key_X, Qt::ControlModifier);
    QApplication::processEvents();

    qDebug() << "AFTER cut across marker:" << dumpBlocks(doc);

    QCOMPARE(markerBlockCount(doc), 0);
    QVERIFY2(!doc->toPlainText().contains(TypefaceFormatter::pageBreakMark()),
             "marker text must be gone after cut");

    dropEditor(k);
}

QTEST_MAIN(TestPageBreak)

#include "test_pagebreak.moc"
