#ifndef APPCONFIGPAGE_TELEGRAM_H
#define APPCONFIGPAGE_TELEGRAM_H

#include "ConfigPage.h"

class QWidget;
class QRadioButton;
class QGroupBox;

// Страница настроек импорта Telegram: живет после Branch tree.
// Выбор раскладки: заметка на месяц или заметка на сообщение
class AppConfigPage_Telegram : public ConfigPage
{
 Q_OBJECT

public:
  AppConfigPage_Telegram(QWidget *parent = nullptr);
  virtual ~AppConfigPage_Telegram(void);

  int applyChanges(void);
  void retranslateUi(void);

protected:

  void setupUi(void);
  void setupSignals(void);
  void assembly(void);

  // Заметка на месяц (по умолчанию), на день
  // либо на сообщение: чат-год, чат-год-месяц, чат-год-месяц-день
  QRadioButton *monthNote;
  QRadioButton *dayNote;
  QRadioButton *messageNote;

  QGroupBox *layoutBox;
};


#endif // APPCONFIGPAGE_TELEGRAM_H
