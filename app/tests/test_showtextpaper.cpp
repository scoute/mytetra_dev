// Бумага открепляемого окна как в основном редакторе (см. 564, линия v2).
// На теме Custom - явный цвет из настроек, на базовых темах поле
// полностью отдается теме (пустой stylesheet). С 566 окно содержит
// полноценный Editor без тулбаров: проверяется и это.
#include <QtTest>
#include <QApplication>
#include <QWidget>
#include <QSettings>
#include <QTemporaryDir>

#include "libraries/wyedit/EditorShowText.h"
#include "libraries/wyedit/Editor.h"
#include "libraries/wyedit/EditorTextArea.h"
#include "libraries/wyedit/EditorFindBar.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/ShortcutManager.h"

extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;
extern ShortcutManager shortcutManager;

class TestShowTextPaper : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void paperFollowsTheme(void);
    void toolbarHidden(void);

private:

    QTemporaryDir m_tmp;
};

void TestShowTextPaper::initTestCase(void)
{
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    globalParameters.setMainProgramFile(QCoreApplication::applicationFilePath());
    QVERIFY2(globalParameters.initHeadless(), "headless workdir must init");

    mytetraConfig.m_conf=new QSettings(m_tmp.filePath(QStringLiteral("test_showtextpaper.ini")),
                                       QSettings::IniFormat, this);

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
    QVERIFY2(mytetraConfig.setInterfaceTheme(QStringLiteral("custom")),
             "custom theme must be settable");
    mytetraConfig.set_notePaperColor(QStringLiteral("#112233"));

    EditorShowText themedWin;
    themedWin.ensureEditor();
    themedWin.show();
    QApplication::processEvents();

    // Поле - тот же EditorTextArea что в главном окне
    EditorTextArea *themedArea=themedWin.findChild<EditorTextArea *>();
    QVERIFY2(themedArea!=nullptr, "detached editor text area must exist");

    // То же имя поля что у основного редактора: CSS-правила бумаг
    // (QWidget[objectName="textArea"]) применяются и здесь
    QCOMPARE(themedArea->objectName(), QStringLiteral("textArea"));

    QVERIFY2(themedArea->styleSheet().contains(QStringLiteral("#112233")),
             "detached window must use custom paper color");

    // Базовая тема: поле полностью за темой, своего stylesheet нет
    QVERIFY2(mytetraConfig.setInterfaceTheme(QStringLiteral("default")),
             "default theme must be settable");

    EditorShowText plainWin;
    plainWin.ensureEditor();
    plainWin.show();
    QApplication::processEvents();
    QVERIFY2(plainWin.findChild<EditorTextArea *>()->styleSheet().isEmpty(),
             "detached window must not override base theme");
}

void TestShowTextPaper::toolbarHidden(void)
{
    // Окно 1:1 как заметка, но без панелей кнопок: тулбары созданы,
    // в слой не добавлены и скрыты, полоска поиска скрыта до Ctrl+F
    EditorShowText win;
    win.ensureEditor();
    win.show();
    QApplication::processEvents();

    Editor *editor=win.editor();
    QVERIFY2(editor!=nullptr, "detached editor must exist after ensureEditor");

    QWidget *toolBar=editor->findChild<QWidget *>(QStringLiteral("toolBarAssistant"));
    QVERIFY2(toolBar!=nullptr, "toolbar assistant must exist");
    QVERIFY2(!toolBar->isVisible(), "toolbar must stay hidden in detached window");

    QVERIFY2(editor->findBarWidget()!=nullptr, "find bar must exist");
    QVERIFY2(!editor->findBarWidget()->isVisible(), "find bar must stay hidden until Ctrl+F");

    EditorTextArea *area=win.findChild<EditorTextArea *>();
    QVERIFY2(area!=nullptr, "detached text area must exist");
    QVERIFY2(area->isVisible(), "text area must be visible in shown window");
}

QTEST_MAIN(TestShowTextPaper)

#include "test_showtextpaper.moc"
