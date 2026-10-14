#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QTextBrowser>
#include <QLabel>
#include <QPushButton>

#include "ChangeHistoryDialog.h"
#include "libraries/UnifiedDiff.h"


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
    closeButton=new QPushButton(tr("Close"), this);

    QSplitter *splitter=new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(versionsList);
    splitter->addWidget(diffView);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    QHBoxLayout *buttonsLayout=new QHBoxLayout();
    buttonsLayout->addWidget(statusLabel, 1);
    buttonsLayout->addWidget(rollbackButton);
    buttonsLayout->addWidget(closeButton);

    QVBoxLayout *centralLayout=new QVBoxLayout(this);
    centralLayout->addWidget(splitter, 1);
    centralLayout->addLayout(buttonsLayout);
    setLayout(centralLayout);

    versionList=ChangeHistory::versions(tetradir, recordId);

    for(const ChangeHistory::Version &version : versionList)
        versionsList->addItem(version.date+QStringLiteral("  ")+version.subject);

    if(versionList.isEmpty())
        statusLabel->setText(tr("No saved versions yet"));
    else
    {
        statusLabel->setText(tr("%1 versions").arg(versionList.size()));
        versionsList->setCurrentRow(0);
    }

    connect(versionsList, &QListWidget::currentRowChanged,
            this,          &ChangeHistoryDialog::onVersionSelected);
    connect(rollbackButton, &QPushButton::clicked,
            this,            &ChangeHistoryDialog::onRollback);
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

    accept();
}


void ChangeHistoryDialog::updateDiff(void)
{
    const int row=versionsList->currentRow();

    if(row<0 || row>=versionList.size())
    {
        diffView->clear();
        rollbackButton->setEnabled(false);
        return;
    }

    rollbackButton->setEnabled(true);

    const QString diff=ChangeHistory::diffToCurrent(tetradir, recordId,
                                                    versionList.at(row).rev,
                                                    currentHtml, recordName);

    if(diff.isEmpty())
        diffView->setPlainText(tr("No differences from current text"));
    else
        diffView->setHtml(UnifiedDiff::toColoredHtml(diff));
}
