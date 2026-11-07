#ifndef _CONFIGPAGE_ATTACH_H_
#define	_CONFIGPAGE_ATTACH_H_

#include "ConfigPage.h"

class QWidget;
class QCheckBox;
class QGroupBox;
class QLabel;
class QToolButton;
class QPushButton;
class MtComboBox;

class AppConfigPage_Attach : public ConfigPage
{
 Q_OBJECT

public:
  AppConfigPage_Attach(QWidget *parent = nullptr);
  virtual ~AppConfigPage_Attach();

  void setupUi(void);
  void setupSignals(void);
  void assembly(void);

  int applyChanges(void);
  void retranslateUi(void);

private slots:

  void onEnableRecordWithAttachHighlight(bool);
  void onClickedHighlightColor();
  void onClickedConvertAttaches();
  void onClickedConvertRecordDirs();

protected:

  void setColorForButtonHighlightColor(QColor iColor);

  // Приведение всех вложений к заданному виду имен.
  // Возвращает число сконвертированных файлов, через счетчики сообщает
  // о ненайденных на диске, пропущенных зашифрованных и ошибках
  int convertAllAttachesToMode(int targetMode, int &notFoundCount, int &encryptedCount, QStringList &errors);

  // Галка разрешения/запрещения подсветки записи с прикрепленными файлами
  QCheckBox *enableRecordWithAttachHighlight;

  // Выбор цвета для записи с прикрепленными файлами
  QLabel *labelHighlightColor;
  QToolButton *buttonHighlightColor;
  QColor *highlightColor;

  // Объединяющая рамка
  QGroupBox *decorBox;

  // Имена файлов на диске: пояснение, выбор режима, кнопка конвертации,
  // предупреждение
  QGroupBox *fileNamesBox;
  QLabel *fileNamesInfo;
  QLabel *attachNameModeLabel;
  MtComboBox *attachNameModeCombo;
  QPushButton *convertAttachesButton;
  QLabel *fileNamesWarning;

  // Каталоги записей на диске: пояснение, выбор режима, кнопка конвертации,
  // предупреждение. Режим действует на новые записи и переименования,
  // старые каталоги трогает только конвертер
  QGroupBox *recordDirsBox;
  QLabel *recordDirsInfo;
  QLabel *recordDirNameModeLabel;
  MtComboBox *recordDirNameModeCombo;
  QPushButton *convertRecordDirsButton;
  QLabel *recordDirsWarning;
};


#endif	// _CONFIGPAGE_ATTACH_H_

