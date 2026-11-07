// Безусловный пункт "Каталог заметки" в контекстном меню (см. 553).
// Живой MetaEditor: пункт виден и доступен на обычном тексте
// (раньше "Показать в папке" был только на голосе/картинках).
// Резолв каталога - чистая функция без файлового менеджера и диалогов.
#include <QtTest>
#include <QApplication>
#include <QWidget>
#include <QSettings>
#include <QAction>
#include <QDir>
#include <QTemporaryDir>

#include "views/record/MetaEditor.h"
#include "libraries/wyedit/Editor.h"
#include "libraries/wyedit/EditorTextArea.h"
#include "libraries/wyedit/EditorContextMenu.h"
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

class TestNoteFolder : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void menuAlwaysVisible(void);
    void resolveDirectory(void);

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

void TestNoteFolder::initTestCase(void)
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
    mytetraConfig.m_conf=new QSettings(m_tmp.filePath(QStringLiteral("test_notefolder.ini")),
                                       QSettings::IniFormat, this);

    AppContext::instance().init(fp, gp, &mytetraConfig, af, dbc, tm,
                                wh, vh, al, &shortcutManager, pcb, ps,
                                nullptr, nullptr, &gp->cryptHolder());

    mytetraConfig.init();
    shortcutManager.init();
}

void TestNoteFolder::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

TestNoteFolder::EditorKit TestNoteFolder::makeEditor(void)
{
    EditorKit kit;
    kit.win=new QWidget;
    kit.win->show();
    kit.ed=new MetaEditor(kit.win);
    kit.ed->show();
    QApplication::processEvents();
    return kit;
}

void TestNoteFolder::dropEditor(EditorKit &kit)
{
    delete kit.win;
    kit.win=nullptr;
    kit.ed=nullptr;
}

void TestNoteFolder::menuAlwaysVisible(void)
{
    EditorKit k=makeEditor();

    // Обычный текст, курсор не на голосе/картинке/ссылке
    k.ed->setTextarea(QStringLiteral("<p>Just plain text</p>"));
    QApplication::processEvents();

    k.ed->editorContextMenu->update();

    QAction *showNote=k.ed->editorContextMenu->findChild<QAction *>(
        QStringLiteral("editor_showNoteInFolder"));
    QVERIFY2(showNote!=nullptr, "note folder action must exist");
    QVERIFY2(showNote->isVisible(), "note folder action must be visible on plain text");
    QVERIFY2(showNote->isEnabled(), "note folder action must be enabled on plain text");

    dropEditor(k);
}

void TestNoteFolder::resolveDirectory(void)
{
    // Существующий каталог - абсолютный путь
    const QString abs=Editor::absoluteNoteDirectory(m_tmp.path());
    QCOMPARE(abs, QDir(m_tmp.path()).absolutePath());

    // Пусто - пусто (заметка без каталога: новая, несохраненная)
    QVERIFY2(Editor::absoluteNoteDirectory(QString()).isEmpty(),
             "empty workdir must resolve to empty");

    // Нет на диске - пусто (удален снаружи, битый путь)
    QVERIFY2(Editor::absoluteNoteDirectory(
                 m_tmp.filePath(QStringLiteral("no-such-dir"))).isEmpty(),
             "missing dir must resolve to empty");
}

QTEST_MAIN(TestNoteFolder)

#include "test_notefolder.moc"
