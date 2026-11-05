// Страница Search настроек на QTest (см. 504, 525).
// Порт searchpageprobe: тексты FTS-группы заданы уже в конструкторе.
// Фикстура из репо (app/tests/data/conf.ini), странице отдается
// ее копия в QTemporaryDir.
#include <QtTest>
#include <QApplication>
#include <QSettings>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QTemporaryDir>

#include "views/appConfigWindow/AppConfigPage_Search.h"
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
#include "libraries/InternalClipboard.h"
#include "models/appConfig/AppFiles.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "models/appConfig/AppConfig.h"

extern ShortcutManager shortcutManager;

extern AppConfig mytetraConfig;

class SearchTestPage : public AppConfigPage_Search
{
public:

    explicit SearchTestPage(QWidget *parent=nullptr) : AppConfigPage_Search(parent) {}

    QString boxTitle(void) const { return ftsIndexBox->title(); }
    QString buttonText(void) const { return ftsVacuumButton->text(); }
    QString sizeText(void) const { return ftsIndexSizeLabel->text(); }
};

class TestSearchPage : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void ftsTextsSetAtConstruction(void);

private:

    QTemporaryDir m_tmp;
    QSettings *m_settings=nullptr;
};

void TestSearchPage::initTestCase(void)
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
    static InternalClipboard *ic=new InternalClipboard;
    AppContext::instance().init(fp, gp, &mytetraConfig, af, dbc, tm,
                                wh, vh, al, &shortcutManager, pcb, ps,
                                ic, nullptr, &gp->cryptHolder());

    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    const QString ini=m_tmp.filePath(QStringLiteral("test_search.ini"));
    QVERIFY2(QFile::copy(QStringLiteral(TEST_DATA_DIR "/conf.ini"), ini),
             "repo fixture must be copied to temp dir");
    m_settings=new QSettings(ini, QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;
}

void TestSearchPage::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestSearchPage::ftsTextsSetAtConstruction(void)
{
    SearchTestPage page(nullptr);

    QVERIFY2(!page.boxTitle().isEmpty(), "FTS box title must be set");
    QVERIFY2(!page.buttonText().isEmpty(), "vacuum button text must be set");
    QVERIFY2(!page.sizeText().isEmpty(), "index size text must be set");
}

QTEST_MAIN(TestSearchPage)

#include "test_searchpage.moc"
