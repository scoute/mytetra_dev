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

  retranslateUi();

  connect(fileButton, &QToolButton::clicked,
          this, &TelegramImportDialog::onSelectFile);
  connect(fileInput, &QLineEdit::editingFinished,
          this, &TelegramImportDialog::onSelectFile);
  connect(importButton, &QPushButton::clicked,
          this, &TelegramImportDialog::onImport);
  connect(closeButton, &QPushButton::clicked,
          this, &TelegramImportDialog::accept);

  QHBoxLayout *fileLayout=new QHBoxLayout();
  fileLayout->addWidget(fileInput);
  fileLayout->addWidget(fileButton);

  QHBoxLayout *buttonLayout=new QHBoxLayout();
  buttonLayout->addStretch();
  buttonLayout->addWidget(importButton);
  buttonLayout->addWidget(closeButton);

  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addLayout(fileLayout);
  centralLayout->addWidget(infoLabel);
  centralLayout->addWidget(progressBar);
  centralLayout->addLayout(buttonLayout);
  setLayout(centralLayout);

  resize(520, 220);
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

  const QString modeName=mytetraConfig.get_telegramImportMode();
  const TelegramLayout layout=(modeName==QStringLiteral("message")) ? TelegramLayout::Message :
                               (modeName==QStringLiteral("day")) ? TelegramLayout::Day :
                               TelegramLayout::Month;

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
