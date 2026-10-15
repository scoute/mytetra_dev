#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QTextBrowser>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>

#include "ChangeHistoryDialog.h"
#include "libraries/UnifiedDiff.h"
#include "models/appConfig/AppConfig.h"


extern AppConfig mytetraConfig;


ChangeHistoryDialog::ChangeHistoryDialog(const QString &tetradir,
                                         const QString &recordId,
                                         const QString &recordName,
                                         const QString &currentHtml,
                                         QWidget *parent) : QDialog(parent),
    tetradir(tetradir),
    recordId(recordId),
    recordName(recordName),
    currentHtml(currentHtml)
{
    setWindowTitle(tr("Change history: %1").arg(recordName));
    setModal(true);
    resize(760, 480);

    versionsList=new QListWidget(this);
    diffView=new QTextBrowser(this);
    diffView->setOpenExternalLinks(false);
    statusLabel=new QLabel(this);
    rollbackButton=new QPushButton(tr("Rollback to selected"), this);
    rollbackButton->setEnabled(false);
    compactButton=new QPushButton(tr("Compact history"), this);
    compactButton->setEnabled(false);
    closeButton=new QPushButton(tr("Close"), this);

    QSplitter *splitter=new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(versionsList);
    splitter->addWidget(diffView);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    QHBoxLayout *buttonsLayout=new QHBoxLayout();
    buttonsLayout->addWidget(statusLabel, 1);
    buttonsLayout->addWidget(compactButton);
    buttonsLayout->addWidget(rollbackButton);
    buttonsLayout->addWidget(closeButton);

    QVBoxLayout *centralLayout=new QVBoxLayout(this);
    centralLayout->addWidget(splitter, 1);
    centralLayout->addLayout(buttonsLayout);
    setLayout(centralLayout);

    versionList=ChangeHistory::versions(tetradir, recordId);
    refreshVersionList();

    connect(versionsList, &QListWidget::currentRowChanged,
            this,          &ChangeHistoryDialog::onVersionSelected);
    connect(rollbackButton, &QPushButton::clicked,
            this,            &ChangeHistoryDialog::onRollback);
    connect(compactButton, &QPushButton::clicked,
            this,           &ChangeHistoryDialog::onCompact);
    connect(closeButton, &QPushButton::clicked,
            this,          &ChangeHistoryDialog::reject);

    updateDiff();
}


QString ChangeHistoryDialog::rollbackContent(void) const
{
    return selectedContent;
}


bool ChangeHistoryDialog::hasHistory(void) const
{
    return !versionList.isEmpty();
}


void ChangeHistoryDialog::onVersionSelected(int row)
{
    Q_UNUSED(row);

    updateDiff();
    updatePreview();
}


void ChangeHistoryDialog::updatePreview(void)
{
    const int row=versionsList->currentRow();

    if(row<0 || row>=versionList.size())
        return;

    const QString content=ChangeHistory::contentAt(tetradir, recordId,
                                                   versionList.at(row).rev);

    if(content.isNull())
        return;

    emit previewRequested(content);
}


// Перечитать версии и перестроить список (после сжатия).
// Выбранной становится новейшая, превью и diff обновляются следом
void ChangeHistoryDialog::refreshVersionList(void)
{
    versionsList->clear();

    for(const ChangeHistory::Version &version : versionList)
    {
        // Штамп времени в хвосте темы ("Имя (2026-10-15 15:26:16)")
        // дублирует дату слева: для показа срезается, в git остается
        QString subject=version.subject;
        subject.remove(QRegularExpression(QStringLiteral("\\s*\\(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\)$")));
        versionsList->addItem(version.date+QStringLiteral("  ")+subject);
    }

    if(versionList.isEmpty())
        statusLabel->setText(tr("No saved versions yet"));
    else
    {
        statusLabel->setText(tr("%1 versions").arg(versionList.size()));
        versionsList->setCurrentRow(0);
    }
}


// Ручное сжатие истории по лимиту из настроек.
// Работает независимо от тумблера автосжатия: раз нажали, значит надо.
// Лимит берется из выпадашки при любом положении галочки:
// ручное сжатие это "применить лимит прямо сейчас"
void ChangeHistoryDialog::onCompact(void)
{
    const int keep=qMax(2, mytetraConfig.get_historySquashKeep());
    const int count=versionList.size();

    // Ниже лимита сносить нечего: объясняется сразу, иначе кнопка
    // выглядит сломанной (как раз случай "5 коммитов, меньше не стало")
    if(count<=keep)
    {
        QMessageBox::information(this, tr("Compact history"),
                                 tr("Nothing to squash: %1 version(s), the limit is %2.\n"
                                    "Old versions are removed only above the limit; "
                                    "the first version is always kept.").arg(count).arg(keep));
        return;
    }

    // Пояснение до уничтожения: сколько снесется, сколько останется
    QMessageBox confirmBox(QMessageBox::Question,
                           tr("Compact history"),
                           tr("Compact the history of \"%1\"?\n"
                              "%2 old version(s) will be permanently destroyed, "
                              "%3 will be kept (the first and the newest).\n"
                              "This cannot be undone.").arg(recordName,
                                                            QString::number(count-keep),
                                                            QString::number(keep)),
                           QMessageBox::NoButton, this);
    confirmBox.addButton(QMessageBox::Ok);
    confirmBox.addButton(QMessageBox::Cancel);
    confirmBox.setDefaultButton(QMessageBox::Cancel);
    confirmBox.setEscapeButton(QMessageBox::Cancel);

    if(confirmBox.exec()!=QMessageBox::Ok)
        return;

    QString squashError;

    if(!ChangeHistory::squashOldVersions(tetradir, recordId, keep, &squashError))
    {
        QMessageBox::critical(this, tr("Compaction failed"),
                              tr("Can not squash old versions:\n%1").arg(squashError));
        return;
    }

    versionList=ChangeHistory::versions(tetradir, recordId);
    refreshVersionList();
}


void ChangeHistoryDialog::onRollback(void)
{
    const int row=versionsList->currentRow();

    if(row<0 || row>=versionList.size())
        return;

    selectedContent=ChangeHistory::contentAt(tetradir, recordId,
                                             versionList.at(row).rev);

    if(selectedContent.isNull())
    {
        statusLabel->setText(tr("Can not read version"));
        return;
    }

    // Строка 0 это новейшая версия: уничтожать нечего,
    // номер строки равен числу более новых версий
    if(row>0)
    {
        QMessageBox destroyBox(QMessageBox::Warning,
                               tr("Destroy newer versions?"),
                               tr("Rollback to \"%1\" (%2) will permanently destroy %3 newer version(s).\n"
                                  "The note text will be replaced by this version, no new commit will be created.\n"
                                  "This cannot be undone.").arg(recordName,
                                                                versionList.at(row).date,
                                                                QString::number(row)),
                               QMessageBox::NoButton, this);

        QPushButton *destroyButton=destroyBox.addButton(tr("Destroy and rollback"),
                                                        QMessageBox::DestructiveRole);
        destroyButton->setObjectName(QStringLiteral("destroyButton"));
        destroyBox.addButton(QMessageBox::Cancel);

        // По умолчанию стоит отмена: мимо кнопки не промахнуться
        destroyBox.setDefaultButton(QMessageBox::Cancel);
        destroyBox.setEscapeButton(QMessageBox::Cancel);
        destroyBox.exec();

        if(destroyBox.clickedButton()!=destroyButton)
            return;

        QString resetError;

        if(!ChangeHistory::resetToVersion(tetradir, recordId,
                                          versionList.at(row).rev, &resetError))
        {
            QMessageBox::critical(this, tr("Rollback failed"),
                                  tr("Can not destroy newer versions:\n%1").arg(resetError));
            return;
        }
    }

    accept();
}


void ChangeHistoryDialog::updateDiff(void)
{
    const int row=versionsList->currentRow();

    if(row<0 || row>=versionList.size())
    {
        diffView->clear();
        rollbackButton->setEnabled(false);
        compactButton->setEnabled(false);
        return;
    }

    rollbackButton->setEnabled(true);
    compactButton->setEnabled(true);

    const QString diff=ChangeHistory::diffToCurrent(tetradir, recordId,
                                                    versionList.at(row).rev,
                                                    currentHtml, recordName);

    if(diff.isEmpty())
        diffView->setPlainText(tr("No differences from current text"));
    else
        diffView->setHtml(UnifiedDiff::toColoredHtml(diff));
}
