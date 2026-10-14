#ifndef _CONFIGPAGE_CLIPPER_H_
#define _CONFIGPAGE_CLIPPER_H_

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QSpinBox;
class QLabel;
class QToolButton;
class QCheckBox;
class QKeySequenceEdit;
class QPushButton;

// Страница настроек клиппера: включение, глобальный хоткей (X11),
// статус бэкенда, кнопка проверки, лимиты картинок и подсказка

class AppConfigPage_Clipper : public ConfigPage
{
  Q_OBJECT

public:
  AppConfigPage_Clipper(QWidget *parent = nullptr);
  virtual ~AppConfigPage_Clipper(void);

  int applyChanges(void);

private slots:

  // Знак вопроса у группы клиппера: мгновенная подсказка
  void onClipperHelpButton(void);
  void onClickedClipperTest(void);

private:
  void updateClipperStatus(void);

protected:

  void setupUi(void);
  void setupSignals(void);
  void assembly(void);

  // Группа клиппера: текст подсказки нужен слоту кнопки-помощи
  QGroupBox *clipperBox;

  // Знак вопроса у группы клиппера
  QToolButton *clipperHelpButton;

  // Включение, хоткей, статус, проверка
  QCheckBox *clipperEnable;
  QKeySequenceEdit *clipperHotkeyEdit;
  QLabel *clipperStatusLabel;
  QPushButton *clipperTestButton;

  // Лимиты клиппера: число картинок в одной заметке и размер одной картинки
  QSpinBox *clipperMaxImages;
  QSpinBox *clipperMaxImageSizeMb;
};

#endif // _CONFIGPAGE_CLIPPER_H_
