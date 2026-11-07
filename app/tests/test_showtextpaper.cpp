// Бумага открепляемого окна как в основном редакторе (см. 564).
// На теме Custom - явный цвет из настроек, на базовых темах поле
// полностью отдается теме (пустой stylesheet).
#include <QtTest>
#include <QApplication>
#include <QWidget>
#include <QSettings>
#include <QTextEdit>
#include <QTemporaryDir>

#include "libraries/wyedit/EditorShowText.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppConfigStorage.h"
#include "models/appConfig/AppConfigUi.h"
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

class TestShowTextPaper : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void paperFollowsTheme(void);

private:

    QTemporaryDir m_tmp;
};

void TestShowTextPaper::initTestCase(void)
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
    mytetraConfig.m_conf=new QSettings(m_tmp.filePath(QStringLiteral("test_showtextpaper.ini")),
                                       QSettings::IniFormat, this);

    AppContext::instance().init(fp, gp, &mytetraConfig, af, dbc, tm,
                                wh, vh, al, &shortcutManager, pcb, ps,
                                nullptr, nullptr, &gp->cryptHolder());

    mytetraConfig.init();
    shortcutManager.init();
}

void TestShowTextPaper::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestShowTextPaper::paperFollowsTheme(void)
{
    // Тема Custom с явным цветом бумаги: открепленное окно красится
    QVERIFY2(AppContext::instance().appConfig()->uiSection()->setInterfaceTheme(QStringLiteral("custom")),
             "custom theme must be settable");
    AppContext::instance().appConfig()->uiSection()->set_notePaperColor(QStringLiteral("#112233"));

    EditorShowText themedWin;
    themedWin.setHtml(QStringLiteral("<p>Paper</p>"));
    themedWin.show();
    QApplication::processEvents();

    QTextEdit *themedArea=themedWin.findChild<QTextEdit *>();
    QVERIFY2(themedArea!=nullptr, "detached text area must exist");

    // То же имя поля что у основного редактора: CSS-правила бумаг
    // (QWidget[objectName="textArea"]) применяются и здесь
    QCOMPARE(themedArea->objectName(), QStringLiteral("textArea"));

    QVERIFY2(themedArea->styleSheet().contains(QStringLiteral("#112233")),
             "detached window must use custom paper color");

    // Базовая тема: поле полностью за темой, своего stylesheet нет
    QVERIFY2(AppContext::instance().appConfig()->uiSection()->setInterfaceTheme(QStringLiteral("default")),
             "default theme must be settable");

    EditorShowText plainWin;
    plainWin.setHtml(QStringLiteral("<p>Paper</p>"));
    plainWin.show();
    QApplication::processEvents();
    QVERIFY2(plainWin.findChild<QTextEdit *>()->styleSheet().isEmpty(),
             "detached window must not override base theme");
}

QTEST_MAIN(TestShowTextPaper)

#include "test_showtextpaper.moc"
