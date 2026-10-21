#ifndef TELEGRAMIMPORTDIALOG_H
#define TELEGRAMIMPORTDIALOG_H

#include <QDialog>

class QLineEdit;
class QToolButton;
class QLabel;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QGroupBox;

#include "libraries/telegram/TelegramImport.h"

// Диалог импорта JSON-экспорта Telegram Desktop.
// Выбор result.json, предпросмотр (чат, сообщения, месяцы),
// импорт в Telegram -> чат -> год -> заметки месяцев
class TelegramImportDialog : public QDialog
{
  Q_OBJECT

public:
  TelegramImportDialog(QWidget *parent=nullptr);
  virtual ~TelegramImportDialog(void);

private slots:
  void onSelectFile(void);
  void onImport(void);
  void onLayoutToggle(void);

private:
  void retranslateUi(void);

  QLineEdit *fileInput;
  QToolButton *fileButton;
  QLabel *infoLabel;
  QProgressBar *progressBar;
  QPushButton *importButton;
  QPushButton *closeButton;

  // Раскладка заметок: месяц, день или сообщение.
  // Как в настройках: выбор сразу пишется в конфиг
  QGroupBox *layoutBox;
  QRadioButton *yearNote;
  QRadioButton *monthNote;
  QRadioButton *dayNote;
  QRadioButton *messageNote;

  TelegramChat chat;
  bool chatLoaded=false;
};

#endif // TELEGRAMIMPORTDIALOG_H
