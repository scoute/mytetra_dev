#ifndef APPCONFIGPAGE_RECORDTABLE_H
#define APPCONFIGPAGE_RECORDTABLE_H

#include <QMap>

#include "ConfigPage.h"

class QWidget;
class QCheckBox;
class QGroupBox;

class AppConfigPage_RecordTable : public ConfigPage
{
 Q_OBJECT

public:
  AppConfigPage_RecordTable(QWidget *parent = nullptr);
  int applyChanges(void);
  void retranslateUi(void) override;

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

  // Объединяющая рамка
  QGroupBox *showFieldsBox;
  QGroupBox *showHeadersBox;

  // Рамки групп: нужны члены для перевода на лету
  QGroupBox *groupBoxVisibleHeaders;
  QGroupBox *groupBoxNoteHeader;
  QGroupBox *groupBoxVisibleColumns;
};


#endif // APPCONFIGPAGE_RECORDTABLE_H
