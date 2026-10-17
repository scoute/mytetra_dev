#ifndef TELEGRAMIMPORTDIALOG_H
#define TELEGRAMIMPORTDIALOG_H

#include <QDialog>

class QLineEdit;
class QToolButton;
class QLabel;
class QProgressBar;
class QPushButton;

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

private:
  void retranslateUi(void);

  QLineEdit *fileInput;
  QToolButton *fileButton;
  QLabel *infoLabel;
  QProgressBar *progressBar;
  QPushButton *importButton;
  QPushButton *closeButton;

  TelegramChat chat;
  bool chatLoaded=false;
};

#endif // TELEGRAMIMPORTDIALOG_H
