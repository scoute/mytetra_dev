#ifndef _EDITORCONFIGFONT_H_
#define	_EDITORCONFIGFONT_H_

#include <QWidget>
#include <QLineEdit>
#include <QFontComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

#include "views/appConfigWindow/ConfigPage.h"


class EditorConfig;


class EditorConfigFont : public ConfigPage
{
 Q_OBJECT

public:
  EditorConfigFont(QWidget *parent = nullptr);
  
  void setup_ui(void);
  void setup_signals(void);
  void assembly(void);
  int applyChanges(void);

private slots:
  void on_monospace_applysize_state_changed(int i);

  void on_code_applysize_state_changed(int i);
  void on_code_applyindent_state_changed(int i);

  void on_code_select_color_button_click();

  void on_linkcolor_external_button_click();
  void on_linkcolor_internal_button_click();

  // Общий запрос цвета для кнопок ссылок
  void on_linkcolor_button_click(QToolButton *button, QColor *color);
  
private:
  QLabel        *defaultFontselectLabel;
  QFontComboBox *defaultFontselect;
  QSpinBox      *defaultFontsize;

  QLabel        *monospaceFontselectLabel;
  QFontComboBox *monospaceFontselect;
  QCheckBox     *monospaceFontsizeapply;
  QSpinBox      *monospaceFontsize;

  QLabel        *codeFontselectLabel;
  QFontComboBox *codeFontselect;
  QCheckBox     *codeFontsizeapply;
  QSpinBox      *codeFontsize;
  QCheckBox     *codeIndentsizeapply;
  QSpinBox      *codeIndentsize;
  QLabel        *codeSelectColorLabel;
  QToolButton   *codeSelectColorButton;
  QColor        *codeColor;

  // Клик по кнопке цвета красит в последний цвет без пикера
  QCheckBox     *applyLastColorCheckBox;

  QLabel        *linkColorExternalLabel;
  QToolButton   *linkColorExternalButton;
  QColor        *linkColorExternal;

  QLabel        *linkColorInternalLabel;
  QToolButton   *linkColorInternalButton;
  QColor        *linkColorInternal;

  EditorConfig *conf;
 
};


#endif	// _EDITORCONFIGFONT_H_

