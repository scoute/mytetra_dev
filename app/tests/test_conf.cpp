// Плоский AppConfig на QTest, линия v2 (см. 500).
// Фасады пишут те же ключи QSettings, геттеры читают обратно.
// Ключи не переименованы. В v2 нет секций и счетчиков фолбэков:
// фолбэк на дефолт таблицы проверяется напрямую через get_parameter.
// Полностью программный: свежие ini в QTemporaryDir, без фикстур.
// Глобал mytetraConfig — extern из main.cpp (в v2 нет AppContext).
#include <QtTest>
#include <QApplication>
#include <QSettings>
#include <QFile>
#include <QTemporaryDir>

#include "models/appConfig/AppConfig.h"

extern AppConfig mytetraConfig;

class TestConf : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void storageFlat(void);
    void uiFlat(void);
    void findFlat(void);
    void syncFlat(void);
    void cryptoFlat(void);
    void recordTableFlat(void);
    void clipperFlat(void);
    void servicesFlat(void);
    void parameterFallback(void);
    void tableShape(void);
    void tableBoundsSafe(void);

private:

    QTemporaryDir m_tmp;
    QSettings *m_settings=nullptr;
    QSettings *m_raw=nullptr;
};

void TestConf::initTestCase(void)
{
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    m_settings=new QSettings(m_tmp.filePath(QStringLiteral("test_conf.ini")),
                             QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;
    m_raw=m_settings;
}

void TestConf::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestConf::storageFlat(void)
{
    mytetraConfig.set_trashmaxfilecount(42);
    QCOMPARE(mytetraConfig.get_trashmaxfilecount(), 42);
    QCOMPARE(m_raw->value("trashmaxfilecount").toInt(), 42);
    mytetraConfig.setEnableLogging(true);
    QCOMPARE(mytetraConfig.getEnableLogging(), true);
    mytetraConfig.set_attachNameMode(1);
    QCOMPARE(mytetraConfig.get_attachNameMode(), 1);
}

void TestConf::uiFlat(void)
{
    mytetraConfig.setInterfaceMode(QStringLiteral("mobile"));
    QCOMPARE(mytetraConfig.getInterfaceMode(), QStringLiteral("mobile"));
    QCOMPARE(m_raw->value("interfaceMode").toString(), QStringLiteral("mobile"));
    mytetraConfig.set_treeFilterVisible(true);
    QCOMPARE(mytetraConfig.get_treeFilterVisible(), true);
    mytetraConfig.set_themeCustomTableRowHeight(33);
    QCOMPARE(mytetraConfig.get_themeCustomTableRowHeight(), 33);
    mytetraConfig.setEditorCursorPosition(123);
    QCOMPARE(mytetraConfig.getEditorCursorPosition(), 123);
    mytetraConfig.setShowSplashScreen(false);
    QCOMPARE(mytetraConfig.getShowSplashScreen(), false);
}

void TestConf::findFlat(void)
{
    mytetraConfig.set_findscreen_wordregard(2);
    QCOMPARE(mytetraConfig.get_findscreen_wordregard(), 2);
    mytetraConfig.set_findscreen_show(true);
    QCOMPARE(mytetraConfig.get_findscreen_show(), true);
}

void TestConf::syncFlat(void)
{
    mytetraConfig.set_synchrocommand(QStringLiteral("echo hi"));
    QCOMPARE(mytetraConfig.get_synchrocommand(), QStringLiteral("echo hi"));
    mytetraConfig.set_historySquashKeep(77);
    QCOMPARE(mytetraConfig.get_historySquashKeep(), 77);
    mytetraConfig.set_autosaveEnable(true);
    QCOMPARE(mytetraConfig.get_autosaveEnable(), true);
}

void TestConf::cryptoFlat(void)
{
    mytetraConfig.set_howpassrequest(QStringLiteral("atStartProgram"));
    QCOMPARE(mytetraConfig.get_howpassrequest(), QStringLiteral("atStartProgram"));
    mytetraConfig.set_cryptMethod(QStringLiteral("xchacha"));
    QCOMPARE(mytetraConfig.get_cryptMethod(), QStringLiteral("xchacha"));
    QCOMPARE(m_raw->value("cryptMethod").toString(), QStringLiteral("xchacha"));
}

void TestConf::recordTableFlat(void)
{
    // readOnly с рантайм-локом
    mytetraConfig.set_readOnly(false);
    mytetraConfig.setMcpUiLock(false);
    QCOMPARE(mytetraConfig.get_readOnly(), false);
    mytetraConfig.setMcpUiLock(true);
    QCOMPARE(mytetraConfig.get_readOnly(), true);
    QCOMPARE(mytetraConfig.isMcpUiLocked(), true);
    QCOMPARE(mytetraConfig.isGlobalReadOnly(), false);
    mytetraConfig.setMcpUiLock(false);
    mytetraConfig.setRecordTableShowVerticalHeaders(true);
    QCOMPARE(mytetraConfig.getRecordTableShowVerticalHeaders(), true);
}

void TestConf::clipperFlat(void)
{
    mytetraConfig.set_clipperMaxImages(5);
    QCOMPARE(mytetraConfig.get_clipperMaxImages(), 5);
    QCOMPARE(m_raw->value("clipperMaxImages").toInt(), 5);
    mytetraConfig.set_clipperbranchmode(2);
    QCOMPARE(mytetraConfig.get_clipperbranchmode(), 2);
}

void TestConf::servicesFlat(void)
{
    mytetraConfig.set_voiceNoteQuality(5);
    QCOMPARE(mytetraConfig.get_voiceNoteQuality(), 5);
    QCOMPARE(m_raw->value("voiceNoteQuality").toInt(), 5);
    mytetraConfig.set_voiceNoteFormat(QStringLiteral("opus"));
    QCOMPARE(mytetraConfig.get_voiceNoteFormat(), QStringLiteral("opus"));
    mytetraConfig.set_mcpAllow(true);
    QCOMPARE(mytetraConfig.get_mcpAllow(), true);
    mytetraConfig.set_telegramImportMode(QStringLiteral("day"));
    QCOMPARE(mytetraConfig.get_telegramImportMode(), QStringLiteral("day"));
}

void TestConf::parameterFallback(void)
{
    // Свежий ini без известных ключей: get_parameter отдает дефолт
    // актуальной таблицы вместо падения; неизвестное имя роняет
    QSettings *fresh=new QSettings(m_tmp.filePath(QStringLiteral("test_conf_fresh.ini")),
                                   QSettings::IniFormat, this);
    mytetraConfig.m_conf=fresh;
    m_raw=fresh;

    QCOMPARE(mytetraConfig.get_parameter(QStringLiteral("cryptMethod")),
             QStringLiteral("rc5"));
    QCOMPARE(mytetraConfig.get_parameter(QStringLiteral("interfacelanguage")),
             QStringLiteral("en"));
    QCOMPARE(mytetraConfig.get_parameter(QStringLiteral("treeFilterVisible")),
             QStringLiteral("true"));
}

void TestConf::tableShape(void)
{
    // Актуальная таблица: единый источник вместо захардкоженного номера
    QStringList table=mytetraConfig.get_parameter_table_60(true);
    QVERIFY2(table.size()%3==0, "table must hold name/type/value triples");
    QVERIFY2(table.contains(QStringLiteral("treeFilterVisible")),
             "table must contain newest key");
    QVERIFY2(table.contains(QStringLiteral("cryptMethod")),
             "table must contain mid key");
    QVERIFY2(table.contains(QStringLiteral("tetradir")),
             "table must contain old key");
}

void TestConf::tableBoundsSafe(void)
{
    // Хелперы не должны падать на неизвестном имени и на таблице
    // без сигнатуры конца (раньше table.at() за границей)
    QStringList noSig;
    noSig << QStringLiteral("a") << QStringLiteral("int") << QStringLiteral("1")
          << QStringLiteral("b") << QStringLiteral("bool") << QStringLiteral("true");

    QCOMPARE(mytetraConfig.getParameterTypeFromTable(QStringLiteral("missing"), noSig),
             QString());
    QCOMPARE(mytetraConfig.getParameterDefaultValueFromTable(QStringLiteral("missing"), noSig),
             QString());
    QCOMPARE(mytetraConfig.getParameterTypeFromTable(QStringLiteral("b"), noSig),
             QStringLiteral("bool"));

    QStringList stripped=mytetraConfig.removeParameterFromTable(QStringLiteral("missing"), noSig);
    QCOMPARE(stripped.size(), noSig.size());

    QStringList replaced=mytetraConfig.replaceParameterInTable(QStringLiteral("missing"),
                                                               QStringLiteral("int"),
                                                               QStringLiteral("0"),
                                                               noSig);
    QCOMPARE(replaced.size(), noSig.size());
}

QTEST_MAIN(TestConf)

#include "test_conf.moc"
