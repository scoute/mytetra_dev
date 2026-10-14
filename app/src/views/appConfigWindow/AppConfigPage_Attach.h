#ifndef _CONFIGPAGE_ATTACH_H_
#define	_CONFIGPAGE_ATTACH_H_

#include "ConfigPage.h"

class QWidget;
class QCheckBox;
class QGroupBox;
class QLabel;
class QToolButton;
class QPushButton;
class QComboBox;

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
  QComboBox *attachNameModeCombo;
  QPushButton *convertAttachesButton;
  QLabel *fileNamesWarning;
};


#endif	// _CONFIGPAGE_ATTACH_H_

