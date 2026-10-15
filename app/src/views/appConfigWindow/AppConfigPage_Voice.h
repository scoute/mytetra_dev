#ifndef APPCONFIGPAGE_VOICE_H
#define APPCONFIGPAGE_VOICE_H

#include "ConfigPage.h"

class QGroupBox;
class QRadioButton;

// Страница Voice notes: формат записи (Opus/MP3) по умолчанию.
// Диалог записи предвыбирает его, переключать на ходу можно.
// Неподдержанный бэкендом формат сбрасывается. Качество одно,
// лучшее, выбора нет
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
};

#endif // APPCONFIGPAGE_VOICE_H
