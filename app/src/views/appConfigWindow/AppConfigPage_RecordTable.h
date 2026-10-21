#ifndef APPCONFIGPAGE_RECORDTABLE_H
#define APPCONFIGPAGE_RECORDTABLE_H

#include <QMap>

#include "ConfigPage.h"

class QWidget;
class QCheckBox;
class QGroupBox;
class QRadioButton;
class QPushButton;

class AppConfigPage_RecordTable : public ConfigPage
{
 Q_OBJECT

public:
  AppConfigPage_RecordTable(QWidget *parent = nullptr);
  int applyChanges(void);
  void retranslateUi(void);

signals:
  void recordTableConfigChange(void); // Сигнал, испускающийся когда изменились настройки таблицы конечных записей

private slots:
 void onFieldToggle(bool);

protected:

  void setupSignals(void);

  QMap<QString, QCheckBox *> fields;

  QCheckBox *showHorizontalHeader;
  QCheckBox *showVerticalHeader;
  QCheckBox *showRecordHeader;

  // Положение списка записей: справа от дерева либо под деревом
  QRadioButton *positionRight;
  QRadioButton *positionUnder;

  QGroupBox *groupBoxPosition;

  // Кнопка открытия диалога настройки кнопок панелей
  QPushButton *editToolbarButtons;

  void onClickedEditToolbarButtons(void);

  QGroupBox *groupBoxVisibleHeaders;
  QGroupBox *groupBoxVisibleColumns;

  // Объединяющая рамка
  QGroupBox *showFieldsBox;
  QGroupBox *showHeadersBox;
};


#endif // APPCONFIGPAGE_RECORDTABLE_H
