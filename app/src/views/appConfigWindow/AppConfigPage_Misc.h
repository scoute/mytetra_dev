#ifndef _CONFIGPAGE_MISC_H_
#define	_CONFIGPAGE_MISC_H_

#include "ConfigPage.h"

class QWidget;
class QCheckBox;
class QGroupBox;
class QPushButton;
class QLabel;
class QToolButton;
class QColor;

class AppConfigPage_Misc : public ConfigPage
{
 Q_OBJECT

public:
  AppConfigPage_Misc(QWidget *parent = nullptr);
  virtual ~AppConfigPage_Misc(void);

  int applyChanges(void);
  void retranslateUi(void) override;

private slots:
  void onClickedEditMyTetraConfigFile(void);
  void onClickedSecretColor(void);
  void onClickedNotePaperColor(void);
  void onClickedNotePaperColorReset(void);

  void setColorForSecretButton(QColor iColor);
  void setColorForNotePaperButton(QColor iColor);

protected:

  void setupUi(void);
  void setupSignals(void);
  void assembly(void);

  QCheckBox *printDebugMessages;      // Выводить ли в консоль отладочные сообщения
  QCheckBox *enableActionLog;         // Разрешено ли логирование действий
  QCheckBox *enableCreateEmptyRecord; // Разрешено ли создание записи, не содержащей текст (а только заголовок)
  QPushButton *editMyTetraConfigFile;

  QLabel *secretColorLabel;
  QToolButton *secretColorButton;
  QColor *secretColor;

  QLabel *notePaperColorLabel;
  QToolButton *notePaperColorButton;
  QColor *notePaperColor;
  QPushButton *notePaperColorReset;

  // True пока действует цвет темы (настройка пуста): тогда Apply
  // не пишет выбранный на кнопке цвет в конфиг
  bool notePaperColorIsTheme;

  // Объединяющая рамка для блока с кнопкой редактирования конфиг-файла
  QGroupBox *dangerBox;
};


#endif	// _CONFIGPAGE_MISC_H_

