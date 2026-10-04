#ifndef _CONFIGPAGE_MISC_H_
#define	_CONFIGPAGE_MISC_H_

#include "ConfigPage.h"

class QWidget;
class QCheckBox;
class QGroupBox;
class QPushButton;
class QSpinBox;
class QLabel;
class QToolButton;

class AppConfigPage_Misc : public ConfigPage
{
 Q_OBJECT

public:
  AppConfigPage_Misc(QWidget *parent = nullptr);
  virtual ~AppConfigPage_Misc(void);

  int applyChanges(void);

private slots:
  void onClickedEditMyTetraConfigFile(void);

  // Знак вопроса у группы клиппера: взгляд цепляется, клик сразу
  // показывает подсказку без секундной задержки тултипа
  void onClipperHelpButton(void);

protected:

  void setupUi(void);
  void setupSignals(void);
  void assembly(void);

  QCheckBox *printDebugMessages;      // Выводить ли в консоль отладочные сообщения
  QCheckBox *enableActionLog;         // Разрешено ли логирование действий
  QCheckBox *enableCreateEmptyRecord; // Разрешено ли создание записи, не содержащей текст (а только заголовок)
   QCheckBox *ignoreSelfSignedSslErrors; // Разрешено ли игнорировать ошибки самоподписанных SSL-сертификатов при скачивании
   QPushButton *editMyTetraConfigFile;

   // Лимиты клиппера: число картинок в одной заметке и размер одной картинки
   QSpinBox *clipperMaxImages;
   QSpinBox *clipperMaxImageSizeMb;

   // Знак вопроса у группы клиппера
   QToolButton *clipperHelpButton;

   // Группа лимитов клиппера: текст подсказки нужен слоту кнопки-помощи
   QGroupBox *clipperBox;

  // Объединяющая рамка для блока с кнопкой редактирования конфиг-файла
  QGroupBox *dangerBox;
};


#endif	// _CONFIGPAGE_MISC_H_

