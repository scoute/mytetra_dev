// Opus-качество на QTest (см. 409, 532).
// Порт voiceqprobe: комбо диалога, битрейт писателя, радиокнопки
// настроек, баблы превью. Без QTimer/app.exec: тело выполняется
// синхронно в слотах, processEvents как в пробе.
#include <QtTest>
#include <QApplication>
#include <QDebug>
#include <QSettings>
#include <QDir>
#include <QCheckBox>
#include <QLabel>
#include <QRadioButton>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
#include <QTemporaryDir>

#include "libraries/MtComboBox.h"
#include "views/voiceNote/VoiceRecorderDialog.h"
#include "views/appConfigWindow/AppConfigPage_Voice.h"
#include "libraries/OpusFileWriter.h"
#include "libraries/OpusFileReader.h"
#include "libraries/VoiceNoteHelper.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppConfigStorage.h"
#include "models/appConfig/AppConfigUi.h"
#include "models/appConfig/AppConfigFind.h"
#include "models/appConfig/AppConfigSync.h"
#include "models/appConfig/AppConfigCrypto.h"
#include "models/appConfig/AppConfigRecordTable.h"
#include "models/appConfig/AppConfigClipper.h"
#include "models/appConfig/AppConfigServices.h"
#include "models/appConfig/AppConfigGeneral.h"
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

class TestVoice : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void bitrates(void);
    void combo(void);
    void writer(void);
    void stereoRead(void);
    void bubbles(void);
    void settingsRadios(void);

private:

    QTemporaryDir m_tmp;
    QSettings *m_settings=nullptr;
    QString m_voiceDir;
};

void TestVoice::initTestCase(void)
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
    AppContext::instance().init(fp, gp, &mytetraConfig, af, dbc, tm,
                                wh, vh, al, &shortcutManager, pcb, ps,
                                nullptr, nullptr, &gp->cryptHolder());

    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    m_settings=new QSettings(m_tmp.filePath(QStringLiteral("test_voice.ini")),
                             QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;

    AppContext::instance().appConfig()->servicesSection()->set_voiceNoteQuality(3);

    m_voiceDir=m_tmp.filePath(QStringLiteral("voicedir"));
    QVERIFY2(QDir().mkpath(m_voiceDir), "voice dir must be created");
}

void TestVoice::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestVoice::bitrates(void)
{
    // Статика битрейтов
    QCOMPARE(VoiceRecorderDialog::opusBitrateForQuality(0), 12000);
    QCOMPARE(VoiceRecorderDialog::opusBitrateForQuality(1), 16000);
    QCOMPARE(VoiceRecorderDialog::opusBitrateForQuality(2), 24000);
    QCOMPARE(VoiceRecorderDialog::opusBitrateForQuality(3), 32000);
    QCOMPARE(VoiceRecorderDialog::opusBitrateForQuality(4), 48000);
    QCOMPARE(VoiceRecorderDialog::opusBitrateForQuality(5), 64000);
    QCOMPARE(VoiceRecorderDialog::opusBitrateForQuality(99), 64000);

    // Режим кодирования: средний битрейт (ABR) - аналог
    // constrained VBR ручного писателя, а не фиксированный CBR
    QCOMPARE(VoiceRecorderDialog::encodingMode(), QMultimedia::AverageBitRateEncoding);
}

void TestVoice::combo(void)
{
    QWidget win;
    win.show();

    VoiceRecorderDialog *dlg=new VoiceRecorderDialog(m_voiceDir, &win);

    // Комбо: 6 пунктов, дефолт из конфига
    QCOMPARE(dlg->opusQuality->count(), 6);
    QCOMPARE(dlg->opusQuality->currentIndex(), 3);

    // Хвост пунктов - прикидка веса минуты (кбит/с*60/8): 90..480 Кбайт
    QVERIFY2(dlg->opusQuality->itemText(0).contains(QStringLiteral("90")),
             "first item must mention 90");
    QVERIFY2(dlg->opusQuality->itemText(5).contains(QStringLiteral("480")),
             "last item must mention 480");

    // Смена пункта: конфиг + подпись
    dlg->opusQuality->setCurrentIndex(1);
    QApplication::processEvents();
    QCOMPARE(AppContext::instance().appConfig()->servicesSection()->get_voiceNoteQuality(), 1);
    QVERIFY2(dlg->formatInfoLabel->text().contains(QStringLiteral("16")),
             "info must mention 16");
    dlg->opusQuality->setCurrentIndex(5);
    QApplication::processEvents();
    QCOMPARE(AppContext::instance().appConfig()->servicesSection()->get_voiceNoteQuality(), 5);
    QVERIFY2(dlg->formatInfoLabel->text().contains(QStringLiteral("64")),
             "info must mention 64");

    // MP3 гасит выбор качества
    dlg->mp3Radio->setChecked(true);
    QApplication::processEvents();
    QVERIFY2(!dlg->opusQuality->isEnabled(), "opus combo must disable on mp3");

    delete dlg;
}

void TestVoice::writer(void)
{
    // Писатель: профиль доходит до файла
    OpusFileWriter w;
    w.setTargetBitrate(
        VoiceRecorderDialog::qualityProfile(
            VoiceRecorderDialog::VoiceFormatOpus).bitRate);
    QVERIFY2(w.open(m_tmp.filePath(QStringLiteral("q64.opus")), 48000),
             "opus writer must open");
    short buf[960]={0};
    for(int f=0; f<50; ++f)
        w.writePcm(buf, 960);
    w.close();
}

void TestVoice::stereoRead(void)
{
    // fix/539: стерео-формула фида: maxFrames делится на channels,
    // preskip считается во фреймах. Здесь - roundtrip моно-файла
    // через ридер без переполнения буфера
    OpusFileReader r;
    QVERIFY2(r.open(m_tmp.filePath(QStringLiteral("q64.opus"))),
             "opus reader must open written file");
    QVERIFY2(r.channels()>=1, "channels must be known");
    QVERIFY2(r.durationMs()>=0, "duration must be known");
    short out[960*2]={0};
    const int got=r.readPcm(out, 960);
    QVERIFY2(got>=0, "readPcm must not fail");
    r.close();
}

void TestVoice::bubbles(void)
{
    // Баблы: round-trip ссылок для превью истории голосовых.
    // Превью делает setHtml версии + materialize, отмена -
    // setHtml исходника + materialize: обе опираются на это
    QTextDocument doc;
    doc.setHtml(QStringLiteral("<p>hi <a href=\"voicenote://v.opus\">[voice 0:10]</a></p>"));
    QVERIFY2(VoiceNoteHelper::hasVoiceLinks(&doc), "voice link must be seen");
    QCOMPARE(VoiceNoteHelper::materializeVoiceLinks(&doc), 1);
    bool bubble=false;
    for(QTextBlock block=doc.firstBlock(); block.isValid(); block=block.next())
        for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
            if(it.fragment().isValid() &&
               it.fragment().charFormat().objectType()==VoiceNoteHelper::BubbleObjectType)
                bubble=true;
    QVERIFY2(bubble, "bubble must be materialized");
    QCOMPARE(VoiceNoteHelper::dematerializeBubbles(&doc), 1);
    QVERIFY2(VoiceNoteHelper::hasVoiceLinks(&doc), "voice link must be back");
}

void TestVoice::settingsRadios(void)
{
    // Настройки: радиокнопки пишут конфиг
    AppConfigPage_Voice page;
    auto findRadio=[&](const QString &text)
    {
        for(QRadioButton *r : page.findChildren<QRadioButton *>())
            if(r->text()==text)
                return r;
        return (QRadioButton *)nullptr;
    };
    QRadioButton *low=findRadio(QStringLiteral("Extra low (16 kbit/s), 1 minute ~ 120 KB"));
    QRadioButton *extra=findRadio(QStringLiteral("Extra high (64 kbit/s), 1 minute ~ 480 KB"));
    QVERIFY2(low!=nullptr && extra!=nullptr, "quality radios must exist");

    low->setChecked(true);
    QApplication::processEvents();
    page.applyChanges();
    QCOMPARE(AppContext::instance().appConfig()->servicesSection()->get_voiceNoteQuality(), 1);

    extra->setChecked(true);
    page.applyChanges();
    QCOMPARE(AppContext::instance().appConfig()->servicesSection()->get_voiceNoteQuality(), 5);
}

QTEST_MAIN(TestVoice)

#include "test_voice.moc"
