// Страница Misc настроек на QTest (см. 504, 518).
// Порт miscprobe: тексты группы статистики должны быть заданы
// уже в конструкторе (retranslateUi в ctor, паттерн Theme/Voice).
// Фикстура живет в репо (app/tests/data/conf.ini), странице отдается
// ее копия в QTemporaryDir (AppConfig может дописать миграцию обратно).
#include <QtTest>
#include <QApplication>
#include <QSettings>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QGroupBox>
#include <QTemporaryDir>

#include "views/appConfigWindow/AppConfigPage_Misc.h"
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
#include "libraries/helpers/UniqueIdHelper.h"

extern ShortcutManager shortcutManager;

extern AppConfig mytetraConfig;

// Доступ к protected-членам страницы без макросов:
// наследник видит protected родителя
class MiscTestPage : public AppConfigPage_Misc
{
public:

    explicit MiscTestPage(QWidget *parent=nullptr) : AppConfigPage_Misc(parent) {}

    QString boxTitle(void) const { return statsBox->title(); }
    QString askText(void) const { return statsAsk->text(); }
    QString alwaysText(void) const { return statsAlways->text(); }
    QString neverText(void) const { return statsNever->text(); }
    QString limitText(void) const { return statsLimitLabel->text(); }
    QString computeNowText(void) const { return statsComputeNow->text(); }
    bool computeNowVisible(void) const { return statsComputeNow->isVisibleTo(const_cast<MiscTestPage*>(this)); }
};

class TestMisc : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void statsTextsSetAtConstruction(void);
    void uniqueIdsDiffer(void);

private:

    QTemporaryDir m_tmp;
    QSettings *m_settings=nullptr;
};

void TestMisc::initTestCase(void)
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

    // Полный конфиг из репозиторной фикстуры: странице нужны все секции.
    // Копия: AppConfig может дописать миграцию обратно
    const QString ini=m_tmp.filePath(QStringLiteral("test_misc.ini"));
    QVERIFY2(QFile::copy(QStringLiteral(TEST_DATA_DIR "/conf.ini"), ini),
             "repo fixture must be copied to temp dir");
    m_settings=new QSettings(ini, QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;
}

void TestMisc::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestMisc::statsTextsSetAtConstruction(void)
{
    MiscTestPage page(nullptr);

    QVERIFY2(!page.boxTitle().isEmpty(), "stats box title must be set");
    QVERIFY2(!page.askText().isEmpty(), "stats ask text must be set");
    QVERIFY2(!page.alwaysText().isEmpty(), "stats always text must be set");
    QVERIFY2(!page.neverText().isEmpty(), "stats never text must be set");
    QVERIFY2(!page.limitText().isEmpty(), "stats limit text must be set");
    QVERIFY2(!page.computeNowText().isEmpty(), "compute now button must be set");
    QVERIFY2(page.computeNowVisible(), "compute now button must be visible");
}

void TestMisc::uniqueIdsDiffer(void)
{
    // fix/547: QRandomGenerator вместо rand() - серия id различается
    QSet<QString> ids;
    for(int i=0; i<50; ++i)
        ids.insert(getUniqueId());
    QVERIFY2(ids.size()>45, "unique ids must differ");
}

QTEST_MAIN(TestMisc)

#include "test_misc.moc"
