#ifndef _EDITORCONFIGMISC_H_
#define	_EDITORCONFIGMISC_H_

#include <QWidget>
#include <QLineEdit>
#include <QFontComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QComboBox>
#include <QCheckBox>

#include "views/appConfigWindow/ConfigPage.h"

class EditorConfig;


class EditorConfigMisc : public ConfigPage
{
 Q_OBJECT

public:
  EditorConfigMisc(QWidget *parent = nullptr);
  
  int applyChanges(void);
    
private slots:
  void onClickedEditWyEditConfigFile(void);
 
private:

  QLabel *indentStepLabel;
  QLabel *indentStepFlexion;
  QSpinBox *indentStep;

  // Размер табуляции для клавиши Tab
  QLabel *tabStopDistanceLabel;
  QLabel *tabStopDistanceFlexionLabel;
  QSpinBox *tabStopDistanceSpinBox;

  // Формат сохранения новых картинок: PNG без потерь или компактный JPG.
  // Действует только на вновь вставляемые картинки
  QLabel *imageFormatLabel;
  QRadioButton *imagePngButton;
  QRadioButton *imageJpgButton;

  // Лимит длинной стороны новых картинок: большие ужимаются при вставке
  QLabel *imageMaxSizeLabel;
  QComboBox *imageMaxSizeCombo;

  // Мастер-тумблер изменения истинного размера файла картинки
  // (опасная зона: действие необратимо, оригинал уходит в корзину)
  QCheckBox *allowTrueImageResizeCheckBox;

  QPushButton *editWyEditConfigFile;
  
  EditorConfig *conf;

  void setupUi(void);
  void setupSignals(void);
  void assembly(void);
  
};


#endif	// _EDITORCONFIGMISC_H_

