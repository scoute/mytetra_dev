#ifndef _CONFIGPAGE_CLIPPER_H_
#define _CONFIGPAGE_CLIPPER_H_

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QSpinBox;
class QLabel;
class QToolButton;

// Страница настроек клиппера: лимиты картинок и подсказка
// как им пользоваться. Раньше жила в Misc, вынесена отдельно

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

protected:

  void setupUi(void);
  void setupSignals(void);
  void assembly(void);

  // Группа клиппера: текст подсказки нужен слоту кнопки-помощи
  QGroupBox *clipperBox;

  // Знак вопроса у группы клиппера
  QToolButton *clipperHelpButton;

  // Лимиты клиппера: число картинок в одной заметке и размер одной картинки
  QSpinBox *clipperMaxImages;
  QSpinBox *clipperMaxImageSizeMb;
};

#endif // _CONFIGPAGE_CLIPPER_H_
