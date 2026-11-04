#ifndef _CONFIGPAGE_MISC_H_
#define	_CONFIGPAGE_MISC_H_

#include "ConfigPage.h"

class QWidget;
class QCheckBox;
class QSpinBox;
class QGroupBox;
class QPushButton;
class QRadioButton;
class QLabel;

class AppConfigPage_Misc : public ConfigPage
{
 Q_OBJECT

public:
  AppConfigPage_Misc(QWidget *parent = nullptr);
  virtual ~AppConfigPage_Misc(void);

  int applyChanges(void);
  void retranslateUi(void);

private slots:
  void onClickedEditMyTetraConfigFile(void);

protected:

  void setupUi(void);
  void setupSignals(void);
  void assembly(void);

  QCheckBox *printDebugMessages;      // Выводить ли в консоль отладочные сообщения
  QCheckBox *enableActionLog;         // Разрешено ли логирование действий
  QCheckBox *enableCreateEmptyRecord; // Разрешено ли создание записи, не содержащей текст (а только заголовок)
  QPushButton *editMyTetraConfigFile;

  QGroupBox *autosaveBox;      // Рамка умного автосохранения
  QCheckBox *autosaveEnable;   // Включено ли автосохранение
  QSpinBox *autosaveInterval;  // Интервал автосохранения, минут

  QGroupBox *networkBox;                  // Рамка сетевых настроек
  QCheckBox *tlsIgnoreInvalidCertificates; // Игнорировать ошибки TLS при загрузке

  QGroupBox *statsBox;        // Рамка статистики базы
  QRadioButton *statsAsk;     // Спрашивать каждый раз
  QRadioButton *statsAlways;  // Всегда считать полную
  QRadioButton *statsNever;   // Никогда (только быстрая)
  QLabel *statsLimitLabel;    // Подпись лимита автосчета
  QSpinBox *statsLimit;       // Лимит записей для автосчета полной без вопросов

  // Цвета заметки и секрета переехали на страницу Themes.
  // Объединяющая рамка для блока с кнопкой редактирования конфиг-файла
  QGroupBox *dangerBox;
};


#endif	// _CONFIGPAGE_MISC_H_
