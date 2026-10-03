#ifndef _CONFIGPAGE_MISC_H_
#define	_CONFIGPAGE_MISC_H_

#include "ConfigPage.h"

class QWidget;
class QCheckBox;
class QGroupBox;
class QPushButton;
class QSpinBox;
class QLabel;
class QKeySequenceEdit;
class MainWindow;

class AppConfigPage_Misc : public ConfigPage
{
 Q_OBJECT

public:
  AppConfigPage_Misc(QWidget *parent = nullptr);
  virtual ~AppConfigPage_Misc(void);

  int applyChanges(void);

 private slots:
   void onClickedEditMyTetraConfigFile(void);
   void onClickedClipperTest(void);

 private:
   void updateClipperStatus(void);

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

   // Веб-клиппер: вставка из буфера в unsorted_notes по глобальному хоткею
   QCheckBox *clipperEnable;
   QKeySequenceEdit *clipperHotkeyEdit;
   QLabel *clipperStatusLabel;
   QPushButton *clipperTestButton;

  // Объединяющая рамка для блока с кнопкой редактирования конфиг-файла
  QGroupBox *dangerBox;
};


#endif	// _CONFIGPAGE_MISC_H_

