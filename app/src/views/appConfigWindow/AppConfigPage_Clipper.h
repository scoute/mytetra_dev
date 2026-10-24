#ifndef _CONFIGPAGE_CLIPPER_H_
#define _CONFIGPAGE_CLIPPER_H_

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QSpinBox;
class QLabel;
class QFormLayout;
class QToolButton;
class QCheckBox;
class QKeySequenceEdit;
class QPushButton;
class QRadioButton;
class QButtonGroup;
class MtComboBox;

// Страница настроек клиппера: включение, глобальный хоткей (X11),
// статус бэкенда, кнопка проверки, лимиты картинок, целевая ветка
// и подсказка

class AppConfigPage_Clipper : public ConfigPage
{
  Q_OBJECT

public:
  AppConfigPage_Clipper(QWidget *parent = nullptr);
  virtual ~AppConfigPage_Clipper(void);

  int applyChanges(void);
  void retranslateUi(void) override;

private slots:

  // Знак вопроса у группы клиппера: мгновенная подсказка
  void onClipperHelpButton(void);
  void onClickedClipperTest(void);

  // Переключение радиокнопок ветки: список активен только в режиме выбора
  void onBranchModeChanged(void);

  // Список веток для комбо: корневые + два уровня вложений.
  // Сохраненная ветка глубже третьего уровня дописывается отдельно,
  // чтобы выбор не терялся
  void rebuildBranchList(void);

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

  // Целевая ветка: радиокнопки "по умолчанию" / "выбранная" / "спросить"
  // и список корневых веток с двумя уровнями вложений
  QRadioButton *clipperBranchDefault;
  QRadioButton *clipperBranchCustom;
  QRadioButton *clipperBranchAsk;
  MtComboBox *clipperBranchCombo;

  // Нижние плитки опроса: последние 16 или частые 16
  QRadioButton *clipperTilesRecent;
  QRadioButton *clipperTilesFrequent;

  // Дубль: открыть оригинал заметки
  QCheckBox *clipperOpenDup;

  // Группы эксклюзива: ветка и плитки выбираются независимо
  QButtonGroup *branchGroup;
  QButtonGroup *tilesGroup;

  // Подписи формы и сама форма: нужны для перевода на лету
  QLabel *clipperMaxImagesLabel;
  QLabel *clipperMaxImageSizeLabel;
  QLabel *clipperTitleLabel;
  QLabel *clipperBranchLabel;
  QLabel *clipperTilesLabel;
  QFormLayout *clipperFormLayout;
};

#endif // _CONFIGPAGE_CLIPPER_H_
