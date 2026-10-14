#ifndef _CONFIGPAGE_ATTACH_H_
#define	_CONFIGPAGE_ATTACH_H_

#include "ConfigPage.h"

class QWidget;
class QCheckBox;
class QGroupBox;
class QLabel;
class QToolButton;
class QPushButton;

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
  void onClickedRenameAttachesToHumanNames();

protected:

  void setColorForButtonHighlightColor(QColor iColor);

  // Переименование всех вложений в человеко-читаемые имена.
  // Возвращает число переименованных файлов, через счетчики сообщает
  // о пропущенных (файл не найден, запись зашифрована) и ошибках
  int renameAllAttachesToHumanNames(int &missingCount, QStringList &errors);

  // Галка разрешения/запрещения подсветки записи с прикрепленными файлами
  QCheckBox *enableRecordWithAttachHighlight;

  // Выбор цвета для записи с прикрепленными файлами
  QLabel *labelHighlightColor;
  QToolButton *buttonHighlightColor;
  QColor *highlightColor;

  // Объединяющая рамка
  QGroupBox *decorBox;

  // Имена файлов на диске: пояснение, кнопка переименования, предупреждение
  QGroupBox *fileNamesBox;
  QLabel *fileNamesInfo;
  QPushButton *renameAttachesButton;
  QLabel *fileNamesWarning;
};


#endif	// _CONFIGPAGE_ATTACH_H_

