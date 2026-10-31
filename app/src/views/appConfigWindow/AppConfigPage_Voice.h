#ifndef APPCONFIGPAGE_VOICE_H
#define APPCONFIGPAGE_VOICE_H

#include "ConfigPage.h"

class QGroupBox;
class QRadioButton;

// Страница Voice notes: формат записи (Opus/MP3) и качество Opus
// по умолчанию. Диалог записи предвыбирает их, переключать
// на ходу можно. Неподдержанный бэкендом формат сбрасывается
class AppConfigPage_Voice : public ConfigPage
{
    Q_OBJECT

public:
    AppConfigPage_Voice(QWidget *parent = nullptr);
    virtual ~AppConfigPage_Voice();

    void retranslateUi(void);
    int applyChanges(void);

protected:
    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    QGroupBox *formatBox;
    QRadioButton *opusRadio;
    QRadioButton *mp3Radio;

    QGroupBox *qualityBox;
    QRadioButton *qualitySuperLowRadio;
    QRadioButton *qualityExtraLowRadio;
    QRadioButton *qualityLowRadio;
    QRadioButton *qualityMiddleRadio;
    QRadioButton *qualityHighRadio;
    QRadioButton *qualityExtraRadio;

};

#endif // APPCONFIGPAGE_VOICE_H
