// Бумага открепляемого окна как в основном редакторе (см. 564, линия v2).
// На теме Custom - явный цвет из настроек, на базовых темах поле
// полностью отдается теме (пустой stylesheet).
#include <QtTest>
#include <QApplication>
#include <QWidget>
#include <QTextEdit>
#include <QTemporaryDir>

#include "libraries/wyedit/EditorShowText.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"

extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;

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
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    // Полный init ради флага is_init(): applyNoteColors в showEvent
    // читает конфиг только в инициализированном состоянии
    globalParameters.setMainProgramFile(QCoreApplication::applicationFilePath());
    QVERIFY2(globalParameters.initHeadless(), "headless workdir must init");

    mytetraConfig.init();
}

void TestShowTextPaper::cleanupTestCase(void)
{
}

void TestShowTextPaper::paperFollowsTheme(void)
{
    // Тема Custom с явным цветом бумаги: открепленное окно красится
    QVERIFY2(mytetraConfig.setInterfaceTheme(QStringLiteral("custom")),
             "custom theme must be settable");
    mytetraConfig.set_notePaperColor(QStringLiteral("#112233"));

    EditorShowText themedWin;
    themedWin.setHtml(QStringLiteral("<p>Paper</p>"));
    themedWin.show();
    QApplication::processEvents();

    QTextEdit *themedArea=themedWin.findChild<QTextEdit *>();
    QVERIFY2(themedArea!=nullptr, "detached text area must exist");
    QVERIFY2(themedArea->styleSheet().contains(QStringLiteral("#112233")),
             "detached window must use custom paper color");

    // Базовая тема: поле за темой, своего stylesheet нет
    QVERIFY2(mytetraConfig.setInterfaceTheme(QStringLiteral("default")),
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
