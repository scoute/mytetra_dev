#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QMap>
#include <QDateTime>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QGroupBox>
#include <QApplication>

#include "views/telegram/TelegramImportDialog.h"
#include "models/appConfig/AppConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "views/tree/TreeScreen.h"
#include "libraries/helpers/ObjectHelper.h"


extern AppConfig mytetraConfig;


TelegramImportDialog::TelegramImportDialog(QWidget *parent) : QDialog(parent)
{
  fileInput=new QLineEdit(this);

  fileButton=new QToolButton(this);
  fileButton->setText(QStringLiteral("..."));

  infoLabel=new QLabel(this);
  infoLabel->setWordWrap(true);

  progressBar=new QProgressBar(this);
  progressBar->setRange(0, 100);
  progressBar->setValue(0);

  importButton=new QPushButton(this);
  closeButton=new QPushButton(this);
  closeButton->setDefault(true);

  // Раскладка заметок в самом диалоге: выбор сразу в конфиг,
  // импорт читает его при старте
  layoutBox=new QGroupBox(this);

  yearNote=new QRadioButton(this);
  monthNote=new QRadioButton(this);
  dayNote=new QRadioButton(this);
  messageNote=new QRadioButton(this);

  retranslateUi();

  const QString savedMode=mytetraConfig.get_telegramImportMode();
  yearNote->setChecked(savedMode==QStringLiteral("year"));
  monthNote->setChecked(savedMode!=QStringLiteral("message") &&
                        savedMode!=QStringLiteral("day") &&
                        savedMode!=QStringLiteral("year"));
  dayNote->setChecked(savedMode==QStringLiteral("day"));
  messageNote->setChecked(savedMode==QStringLiteral("message"));

  connect(fileButton, &QToolButton::clicked,
          this, &TelegramImportDialog::onSelectFile);
  connect(fileInput, &QLineEdit::editingFinished,
          this, &TelegramImportDialog::onSelectFile);
  connect(importButton, &QPushButton::clicked,
          this, &TelegramImportDialog::onImport);
  connect(closeButton, &QPushButton::clicked,
          this, &TelegramImportDialog::accept);
  connect(yearNote, &QRadioButton::toggled,
          this, &TelegramImportDialog::onLayoutToggle);
  connect(monthNote, &QRadioButton::toggled,
          this, &TelegramImportDialog::onLayoutToggle);
  connect(dayNote, &QRadioButton::toggled,
          this, &TelegramImportDialog::onLayoutToggle);
  connect(messageNote, &QRadioButton::toggled,
          this, &TelegramImportDialog::onLayoutToggle);

  QHBoxLayout *fileLayout=new QHBoxLayout();
  fileLayout->addWidget(fileInput);
  fileLayout->addWidget(fileButton);

  QHBoxLayout *buttonLayout=new QHBoxLayout();
  buttonLayout->addStretch();
  buttonLayout->addWidget(importButton);
  buttonLayout->addWidget(closeButton);

  QVBoxLayout *layoutLayout=new QVBoxLayout;
  layoutLayout->addWidget(yearNote);
  layoutLayout->addWidget(monthNote);
  layoutLayout->addWidget(dayNote);
  layoutLayout->addWidget(messageNote);
  layoutBox->setLayout(layoutLayout);

  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addLayout(fileLayout);
  centralLayout->addWidget(infoLabel);
  centralLayout->addWidget(layoutBox);
  centralLayout->addWidget(progressBar);
  centralLayout->addLayout(buttonLayout);
  setLayout(centralLayout);

  resize(520, 300);
  importButton->setEnabled(false);
}


TelegramImportDialog::~TelegramImportDialog(void)
{
}


void TelegramImportDialog::retranslateUi(void)
{
  setWindowTitle(tr("Import Telegram chat"));
  importButton->setText(tr("Import"));
  closeButton->setText(tr("Close"));
  infoLabel->setText(tr("Select Telegram Desktop export file (result.json or messages.html)."));

  monthNote->setText(tr("One note per month: chat - year (default)"));
  yearNote->setText(tr("One note per year: chat"));
  dayNote->setText(tr("One note per day: chat - year - month"));
  messageNote->setText(tr("One note per message: chat - year - month - day"));

  layoutBox->setTitle(tr("Notes layout"));
}


void TelegramImportDialog::onLayoutToggle(void)
{
  QString mode=QStringLiteral("month");

  if(messageNote->isChecked())
    mode=QStringLiteral("message");
  else if(dayNote->isChecked())
    mode=QStringLiteral("day");
  else if(yearNote->isChecked())
    mode=QStringLiteral("year");

  if(mytetraConfig.get_telegramImportMode()!=mode)
    mytetraConfig.set_telegramImportMode(mode);
}


void TelegramImportDialog::onSelectFile(void)
{
  QString fileName=fileInput->text().trimmed();

  // Пустое поле или каталог: диалог выбора result.json
  if(fileName.isEmpty() || !QFileInfo(fileName).isFile())
  {
    QFileDialog fileDialog(this);
    fileDialog.setFileMode(QFileDialog::ExistingFile);
    fileDialog.setNameFilter(tr("Telegram export (result.json, messages.html)"));
    fileDialog.setWindowTitle(tr("Select Telegram export file"));

    if(!fileDialog.exec())
      return;

    if(fileDialog.selectedFiles().isEmpty())
      return;

    fileName=fileDialog.selectedFiles().first();
    fileInput->setText(fileName);
  }

  TelegramChat parsed;
  QString error;

  if(!parseTelegramExport(fileName, parsed, error))
  {
    infoLabel->setText(tr("Parse error: ")+error);
    importButton->setEnabled(false);
    chatLoaded=false;
    return;
  }

  int mediaCount=0;

  foreach(const TelegramMessage &message, parsed.messages)
    mediaCount+=message.media.size();

  QMap<QString, int> months;

  foreach(const TelegramMessage &message, parsed.messages)
  {
    const QDate date=QDateTime::fromSecsSinceEpoch(message.unixtime).date();

    if(date.isValid())
      ++months[QStringLiteral("%1-%2").arg(date.year()).arg(date.month(), 2, 10, QChar('0'))];
  }

  chat=parsed;
  chatLoaded=true;

  infoLabel->setText(tr("Chat: %1. Messages: %2, months: %3, media files: %4.").arg(chat.name).arg(chat.messages.size()).arg(months.size()).arg(mediaCount));
  importButton->setEnabled(true);
  progressBar->setValue(0);
}


void TelegramImportDialog::onImport(void)
{
  if(!chatLoaded)
    return;

  TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

  if(treeScreen==nullptr || treeScreen->knowTreeModel==nullptr)
  {
    QMessageBox::warning(this, tr("Warning"),
                         tr("Database tree is unavailable."));
    return;
  }

  importButton->setEnabled(false);
  progressBar->setRange(0, chat.messages.size());
  progressBar->setValue(0);

  TelegramImportResult result;
  QString error;

  const TelegramLayout layout=telegramLayoutForMode(mytetraConfig.get_telegramImportMode());

  const bool ok=importTelegramChat(treeScreen->knowTreeModel, chat,
                                   result, error, layout,
                                   [&](int done, int total) {
    Q_UNUSED(total);
    progressBar->setValue(done);
    QApplication::processEvents();
  });

  if(!ok)
  {
    QMessageBox::warning(this, tr("Warning"),
                         tr("Import failed: ")+error);
    importButton->setEnabled(true);
    return;
  }

  treeScreen->saveKnowTree();

  progressBar->setValue(chat.messages.size());

  QMessageBox::information(this, tr("Import Telegram chat"),
                           tr("Done. Branches: %1, notes: %2, attached files: %3, missing: %4.").arg(result.branches).arg(result.notes).arg(result.attaches).arg(result.skippedMedia));

  accept();
}
