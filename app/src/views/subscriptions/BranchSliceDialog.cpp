#include <QBrush>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

#include "BranchSliceDialog.h"


// Роли данных в элементах дерева записей
enum SliceItemRole
{
    RoleRecordDir=Qt::UserRole+1,   // Каталог записи в records/ (признак записи)
    RoleRecordFile=Qt::UserRole+2,  // Файл записи в каталоге
    RoleBranchPath=Qt::UserRole+3,  // Путь ветки «ветка / подветка»
    RoleRecordId=Qt::UserRole+4,    // id записи (для подсветки изменений)
    RoleBranchId=Qt::UserRole+5,    // id ветки (для подсветки изменений)
    RoleDeleted=Qt::UserRole+6      // плейсхолдер удалённого владельцем объекта
};


BranchSliceDialog::BranchSliceDialog(const QString &publicationDir, QWidget *parent,
                                     const QList<DiffChange> &changes)
    : QDialog(parent), publicationDir(publicationDir), changes(changes)
{
    // Метаданные публикации (заголовок, владелец, версия)
    meta=BranchPublisher::readPublication(publicationDir);

    setupUi();

    // Дерево записей строится из branch.xml
    QString branchXmlPath=publicationDir+"/branch.xml";
    QFile branchXmlFile(branchXmlPath);
    if(!branchXmlFile.open(QIODevice::ReadOnly))
    {
        showRecordText(nullptr);
        setWindowTitle(tr("Slice: publication is broken"));
        return;
    }

    QDomDocument branchDoc;
    if(!branchDoc.setContent(&branchXmlFile))
    {
        branchXmlFile.close();
        showRecordText(nullptr);
        setWindowTitle(tr("Slice: publication is broken"));
        return;
    }
    branchXmlFile.close();

    // Корневой узел ветки лежит сразу под <branch>
    QDomElement rootNode=branchDoc.documentElement().firstChildElement("node");
    if(!rootNode.isNull())
        buildTree(rootNode, nullptr, QString());

    QString title=meta.title;
    if(title.isEmpty())
        title=rootNode.attribute("name");

    QString titleText=title;
    if(!meta.ownerName.isEmpty())
        titleText+=tr(" — %1").arg(meta.ownerName);

    setWindowTitle(tr("Slice: %1").arg(titleText));
    infoLabel->setText(tr("<b>%1</b> — версия %2, опубликовано %3")
                       .arg(titleText)
                       .arg(meta.publishVersion)
                       .arg(meta.publishedAt));

    recordTree->expandAll();
    recordTree->setCurrentItem(recordTree->topLevelItem(0));

    applyChangesHighlight();
}


void BranchSliceDialog::setupUi(void)
{
    setWindowTitle(tr("Slice"));
    resize(760, 480);

    infoLabel=new QLabel(this);
    infoLabel->setWordWrap(true);

    recordTree=new QTreeWidget(this);
    recordTree->setObjectName("sliceRecordTree");
    recordTree->setHeaderHidden(true);
    recordTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    recordTree->setMinimumWidth(220);

    textView=new QTextEdit(this);
    textView->setObjectName("sliceRecordText");
    textView->setReadOnly(true); // Показываемый текст можно только просматривать
    textView->setMinimumWidth(280);

    QSplitter *splitter=new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(recordTree);
    splitter->addWidget(textView);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes(QList<int>() << 260 << 500);

    QDialogButtonBox *buttonBox=new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::clicked, this, &BranchSliceDialog::reject);

    QVBoxLayout *layout=new QVBoxLayout(this);
    layout->addWidget(infoLabel);
    layout->addWidget(splitter, 1);
    layout->addWidget(buttonBox);

    connect(recordTree, &QTreeWidget::currentItemChanged,
            this,       &BranchSliceDialog::onCurrentItemChanged);
}


// Рекурсивное построение дерева записей: ветки — <node>, записи — <record>
void BranchSliceDialog::buildTree(const QDomElement &nodeElement,
                                  QTreeWidgetItem *parentBranchItem,
                                  const QString &branchPath)
{
    QString nodeName=nodeElement.attribute("name");
    if(nodeName.isEmpty())
        nodeName=nodeElement.attribute("id");

    QString currentPath=branchPath;
    if(!nodeName.isEmpty())
    {
        if(!currentPath.isEmpty())
            currentPath+=tr(" / ");
        currentPath+=nodeName;
    }

    // Корневая ветка (вызов с parentBranchItem==nullptr) — верхний уровень
    QTreeWidgetItem *branchItem=parentBranchItem
                                    ? new QTreeWidgetItem(parentBranchItem)
                                    : new QTreeWidgetItem(recordTree);
    branchItem->setText(0, nodeName);
    branchItem->setToolTip(0, currentPath);
    branchItem->setData(0, RoleBranchId, nodeElement.attribute("id"));

    const QString branchId=nodeElement.attribute("id");
    if(!branchId.isEmpty())
        branchItems.insert(branchId, branchItem);

    // Записи текущей ветки
    QDomElement recordTable=nodeElement.firstChildElement("recordtable");
    QDomElement record=recordTable.firstChildElement("record");
    while(!record.isNull())
    {
        QTreeWidgetItem *recordItem=new QTreeWidgetItem(branchItem);

        QString recordTitle=record.attribute("name");
        if(recordTitle.isEmpty())
            recordTitle=record.attribute("id");
        recordItem->setText(0, recordTitle);

        recordItem->setData(0, RoleRecordDir,  record.attribute("dir"));
        recordItem->setData(0, RoleRecordFile, record.attribute("file"));
        recordItem->setData(0, RoleBranchPath, currentPath);
        recordItem->setData(0, RoleRecordId,  record.attribute("id"));

        const QString recordId=record.attribute("id");
        if(!recordId.isEmpty())
            recordItems.insert(recordId, recordItem);

        QString tooltip=currentPath;
        if(!record.attribute("author").isEmpty())
            tooltip+=tr("\nauthor: ")+record.attribute("author");
        if(!record.attribute("tags").isEmpty())
            tooltip+=tr("\ntags: ")+record.attribute("tags");
        if(!record.attribute("ctime").isEmpty())
            tooltip+=tr("\nctime: ")+record.attribute("ctime");
        recordItem->setToolTip(0, tooltip);

        record=record.nextSiblingElement("record");
    }

    // Вложенные ветки
    QDomElement childNode=nodeElement.firstChildElement("node");
    while(!childNode.isNull())
    {
        buildTree(childNode, branchItem, currentPath);
        childNode=childNode.nextSiblingElement("node");
    }
}


// Подсветка элементов по списку изменений с базовой точки подписки.
// Новые — зелёным жирным, изменённые — синим жирным; удалённые владельцем
// в дереве отсутствуют, поэтому перечисляются в заголовке
void BranchSliceDialog::applyChangesHighlight(void)
{
    if(changes.isEmpty() || recordTree->topLevelItemCount()==0)
        return;

    int addedCount=0;
    int updatedCount=0;
    int structuralCount=0;

    // Удалённые владельцем объекты: красная секция (их нет в дереве среза)
    QStringList deletedBranches;
    QStringList deletedRecords;

    for(const DiffChange &change : changes)
    {
        switch(change.type)
        {
            case DiffChange::RecordAdd:
            {
                QTreeWidgetItem *item=recordItems.value(change.recordId);
                if(item)
                {
                    QFont font=item->font(0);
                    font.setBold(true);
                    item->setFont(0, font);
                    item->setForeground(0, QBrush(Qt::darkGreen));
                    addedCount++;
                }
                break;
            }

            case DiffChange::RecordUpdate:
            {
                QTreeWidgetItem *item=recordItems.value(change.recordId);
                if(item)
                {
                    QFont font=item->font(0);
                    font.setBold(true);
                    item->setFont(0, font);
                    item->setForeground(0, QBrush(Qt::darkBlue));
                    updatedCount++;
                }
                break;
            }

            case DiffChange::RecordDelete:
                deletedRecords << change.title;
                break;

            case DiffChange::BranchAdd:
            case DiffChange::BranchRename:
            case DiffChange::BranchMove:
            {
                QTreeWidgetItem *item=branchItems.value(change.branchId);
                if(item)
                {
                    QFont font=item->font(0);
                    font.setBold(true);
                    item->setFont(0, font);
                    item->setForeground(0, QBrush(Qt::darkGreen));
                    structuralCount++;
                }
                break;
            }

            case DiffChange::BranchDelete:
                deletedBranches << change.title;
                break;
        }
    }

    // Красная секция удалённого: ветки и записи, которые были, а теперь их нет
    if(!deletedBranches.isEmpty() || !deletedRecords.isEmpty())
    {
        QTreeWidgetItem *deletedGroup=new QTreeWidgetItem(recordTree);
        deletedGroup->setText(0, tr("Удалено владельцем"));
        QFont groupFont=deletedGroup->font(0);
        groupFont.setBold(true);
        deletedGroup->setFont(0, groupFont);
        deletedGroup->setForeground(0, QBrush(Qt::red));
        deletedGroup->setData(0, RoleDeleted, true);

        for(const QString &title : deletedBranches)
        {
            QTreeWidgetItem *item=new QTreeWidgetItem(deletedGroup);
            item->setText(0, tr("Ветка: %1").arg(title));
            item->setForeground(0, QBrush(Qt::red));
            item->setData(0, RoleDeleted, true);
            item->setToolTip(0, tr("Ветка удалена владельцем"));
        }

        for(const QString &title : deletedRecords)
        {
            QTreeWidgetItem *item=new QTreeWidgetItem(deletedGroup);
            item->setText(0, tr("Запись: %1").arg(title));
            item->setForeground(0, QBrush(Qt::red));
            item->setData(0, RoleDeleted, true);
            item->setToolTip(0, tr("Запись удалена владельцем"));
        }

        deletedGroup->setExpanded(true);
    }

    QStringList summary;
    if(addedCount>0)
        summary << tr("новых записей: %1").arg(addedCount);
    if(updatedCount>0)
        summary << tr("изменено: %1").arg(updatedCount);
    if(structuralCount>0)
        summary << tr("изменения структуры: %1").arg(structuralCount);

    QString summaryText;
    if(!summary.isEmpty())
        summaryText=tr("Изменений с последнего импорта: %1").arg(summary.join("; "));
    if(!deletedBranches.isEmpty() || !deletedRecords.isEmpty())
    {
        if(!summaryText.isEmpty())
            summaryText+=tr("; ");
        summaryText+=tr("<font color=\"red\">удалено владельцем: веток %1, записей %2</font>")
                     .arg(deletedBranches.size()).arg(deletedRecords.size());
    }

    if(!summaryText.isEmpty())
        infoLabel->setText(infoLabel->text()+tr("<br>%1").arg(summaryText));
}


// Показ текста записи из records/<dir>/<file>
void BranchSliceDialog::onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous)
    showRecordText(current);
}


void BranchSliceDialog::showRecordText(QTreeWidgetItem *item)
{
    if(!item)
    {
        textView->clear();
        return;
    }

    // Плейсхолдер удалённого владельцем объекта
    if(item->data(0, RoleDeleted).toBool())
    {
        textView->clear();
        textView->setHtml(tr("<p><font color=\"red\"><b>%1</b></font></p>"
                             "<p><i>Этот объект удалён владельцем в текущей версии публикации.<br>"
                             "Ранее импортированная локальная копия остаётся в вашей базе; "
                             "её удаление — только явным действием в диалоге «Что изменилось».</i></p>")
                            .arg(item->text(0).toHtmlEscaped()));
        return;
    }

    QString dir=item->data(0, RoleRecordDir).toString();
    if(dir.isEmpty())
    {
        // Выбран узел-ветка — показывается список её записей подсказкой
        textView->clear();
        textView->setHtml(tr("<p>%1</p><p><i>Выберите запись слева для просмотра.</i></p>")
                            .arg(item->data(0, RoleBranchPath).toString()));
        return;
    }

    QString file=item->data(0, RoleRecordFile).toString();
    if(file.isEmpty())
        file=QStringLiteral("text.html");

    QString recordDir=QFileInfo(publicationDir+"/records/"+dir).absoluteFilePath();
    QString filePath=recordDir+"/"+file;

    QFile recordFile(filePath);
    if(!recordFile.open(QIODevice::ReadOnly))
    {
        textView->clear();
        textView->setHtml(tr("<p>%1</p><p><i>Файл записи не найден.</i></p>")
                            .arg(item->data(0, RoleBranchPath).toString()));
        return;
    }

    QString content=QString::fromUtf8(recordFile.readAll());
    recordFile.close();

    // Базовый адрес — каталог записи, чтобы относительные ссылки
    // на вложения (картинки и т. п.) открывались из records/<dir>
    textView->document()->setBaseUrl(QUrl::fromLocalFile(recordDir+"/"));
    textView->setHtml(content);
}