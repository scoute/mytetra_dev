// Opus-качество 409: комбо диалога, битрейт писателя, радиокнопки настроек
#include <QApplication>
#include <QTimer>
#include <QDebug>
#include <QSettings>
#include <QDir>
#include <QCheckBox>
#include <QLabel>
#include <QRadioButton>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
#include "libraries/MtComboBox.h"
#include "views/voiceNote/VoiceRecorderDialog.h"
#include "views/appConfigWindow/AppConfigPage_Voice.h"
#include "libraries/OpusFileWriter.h"
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
extern GlobalParameters globalParameters;
extern FixedParameters fixedParameters;
extern AppFiles mytetraFiles;
extern DataBaseConfig dataBaseConfig;
extern TrashMonitoring trashMonitoring;
extern WalkHistory walkHistory;
extern VisitHistory visitHistory;
extern ActionLogger actionLogger;
extern ShortcutManager shortcutManager;
extern PeriodicCheckBase periodicCheckBase;
extern PeriodicSyncro periodicSyncro;

static int fails=0;
static void check(bool ok, const char *name)
{
    qDebug() << (ok ? "PASS" : "FAIL") << name;
    if(!ok)
        ++fails;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    mytetraConfig.m_conf=new QSettings(QStringLiteral("/tmp/opencode/probe.ini"),
                                       QSettings::IniFormat);

    // P1: диалог ходит в конфиг через AppContext - контекст как в main()
    AppContext::instance().init(&fixedParameters,
                                &globalParameters,
                                &mytetraConfig,
                                &mytetraFiles,
                                &dataBaseConfig,
                                &trashMonitoring,
                                &walkHistory,
                                &visitHistory,
                                &actionLogger,
                                &shortcutManager,
                                &periodicCheckBase,
                                &periodicSyncro,
                                nullptr,
                                nullptr,
                                &globalParameters.cryptHolder());

    AppContext::instance().appConfig()->servicesSection()->set_voiceNoteQuality(3);

    QDir().mkpath(QStringLiteral("/tmp/opencode/voicedir"));

    QWidget win;
    win.show();

    VoiceRecorderDialog *dlg=new VoiceRecorderDialog(
        QStringLiteral("/tmp/opencode/voicedir"), &win);

    QTimer::singleShot(800, [&]()
    {
        // Статика битрейтов
        check(VoiceRecorderDialog::opusBitrateForQuality(0)==12000, "BITRATE-SUPERLOW");
        check(VoiceRecorderDialog::opusBitrateForQuality(1)==16000, "BITRATE-16");
        check(VoiceRecorderDialog::opusBitrateForQuality(2)==24000, "BITRATE-24");
        check(VoiceRecorderDialog::opusBitrateForQuality(3)==32000, "BITRATE-32");
        check(VoiceRecorderDialog::opusBitrateForQuality(4)==48000, "BITRATE-48");
        check(VoiceRecorderDialog::opusBitrateForQuality(5)==64000, "BITRATE-64");
        check(VoiceRecorderDialog::opusBitrateForQuality(99)==64000, "BITRATE-GARBAGE");

        // Режим кодирования: средний битрейт (ABR) - аналог
        // constrained VBR ручного писателя, а не фиксированный CBR
        check(VoiceRecorderDialog::encodingMode()==QMultimedia::AverageBitRateEncoding, "ENCODING-ABR");

        // Комбо: 6 пунктов, дефолт из конфига
        check(dlg->opusQuality->count()==6, "COMBO-COUNT");
        check(dlg->opusQuality->currentIndex()==3, "COMBO-DEFAULT");

        // Хвост пунктов - прикидка веса минуты (кбит/с*60/8): 90..480 Кбайт
        check(dlg->opusQuality->itemText(0).contains(QStringLiteral("90")), "COMBO-SIZE-12");
        check(dlg->opusQuality->itemText(5).contains(QStringLiteral("480")), "COMBO-SIZE-64");

        // Смена пункта: конфиг + подпись
        dlg->opusQuality->setCurrentIndex(1);
        QApplication::processEvents();
        check(AppContext::instance().appConfig()->servicesSection()->get_voiceNoteQuality()==1, "COMBO-SAVED-1");
        check(dlg->formatInfoLabel->text().contains(QStringLiteral("16")),
              "INFO-16");
        dlg->opusQuality->setCurrentIndex(5);
        QApplication::processEvents();
        check(AppContext::instance().appConfig()->servicesSection()->get_voiceNoteQuality()==5, "COMBO-SAVED");
        check(dlg->formatInfoLabel->text().contains(QStringLiteral("64")),
              "INFO-64");

        // Писатель: профиль доходит до файла
        OpusFileWriter w;
        w.setTargetBitrate(
            VoiceRecorderDialog::qualityProfile(
                VoiceRecorderDialog::VoiceFormatOpus).bitRate);
        if(w.open(QStringLiteral("/tmp/opencode/q64.opus"), 48000))
        {
            short buf[960]={0};
            for(int f=0; f<50; ++f)
                w.writePcm(buf, 960);
            w.close();
            check(true, "WRITER-64-OPEN");
        }
        else
            check(false, "WRITER-64-OPEN");

        // Баблы: round-trip ссылок для превью истории голосовых.
        // Превью делает setHtml версии + materialize, отмена -
        // setHtml исходника + materialize: обе опираются на это
        {
            QTextDocument doc;
            doc.setHtml(QStringLiteral("<p>hi <a href=\"voicenote://v.opus\">[voice 0:10]</a></p>"));
            check(VoiceNoteHelper::hasVoiceLinks(&doc), "VOICE-LINK-SEEN");
            check(VoiceNoteHelper::materializeVoiceLinks(&doc)==1, "VOICE-MATERIALIZED");
            bool bubble=false;
            for(QTextBlock block=doc.firstBlock(); block.isValid(); block=block.next())
                for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
                    if(it.fragment().isValid() &&
                       it.fragment().charFormat().objectType()==VoiceNoteHelper::BubbleObjectType)
                        bubble=true;
            check(bubble, "VOICE-BUBBLE");
            check(VoiceNoteHelper::dematerializeBubbles(&doc)==1, "VOICE-DEMATERIALIZED");
            check(VoiceNoteHelper::hasVoiceLinks(&doc), "VOICE-LINK-BACK");
        }

        // MP3 гасит выбор качества
        dlg->mp3Radio->setChecked(true);
        QApplication::processEvents();
        check(!dlg->opusQuality->isEnabled(), "MP3-DISABLED");

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
        check(low!=nullptr && extra!=nullptr, "SETTINGS-RADIOS");
        if(low!=nullptr && extra!=nullptr)
        {
            low->setChecked(true);
            QApplication::processEvents();
            page.applyChanges();
            check(AppContext::instance().appConfig()->servicesSection()->get_voiceNoteQuality()==1, "SETTINGS-LOW");
            extra->setChecked(true);
            page.applyChanges();
            check(AppContext::instance().appConfig()->servicesSection()->get_voiceNoteQuality()==5, "SETTINGS-EXTRA");
        }

        qDebug() << (fails==0 ? "VOICEQ-OK" : "VOICEQ-BAD");
        qApp->quit();
    });

    return app.exec();
}
