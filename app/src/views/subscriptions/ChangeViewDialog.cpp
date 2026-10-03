#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>

#include "ChangeViewDialog.h"
#include "libraries/TextDiff.h"
#include "TextDiffDialog.h"


// Роль, хранящая индекс изменения в исходном списке
enum ChangeRowRole
{
    ChangeRowIndexRole=Qt::UserRole+1
};


ChangeViewDialog::ChangeViewDialog(const QString &publicationTitle,
                                   const QString &ownerName,
                                   const QList<DiffChange> &changes,
                                   QWidget *parent,
                                   const QString &publicationDir,
                                   const QMap<QString, QPair<QString, QString>> &recordTexts,
                                   const QMap<QString, QPair<QString, QString>> &recordBaseDirs)
    : QDialog(parent),
      publicationTitle(publicationTitle),
      ownerName(ownerName),
      changes(changes),
      publicationDir(publicationDir),
      recordTexts(recordTexts),
      recordBaseDirs(recordBaseDirs)
{
    setupUi();

    QString titleText=publicationTitle;
    if(!ownerName.isEmpty())
        titleText+=tr(" — %1").arg(ownerName);

    setWindowTitle(tr("Changes: %1").arg(titleText));

    fillTable();
    fillHistory();
    onSelectionChanged();
}


void ChangeViewDialog::setupUi(void)
{
    setModal(true);
    resize(860, 680);

    QLabel *infoLabel=new QLabel(this);
    infoLabel->setWordWrap(true);
    infoLabel->setText(tr("<b>%1</b> — изменения публикации.<br>"
                          "Выберите, что импортировать. Удаления отмечены как опасные."));

    modeSelector=new QComboBox(this);
    modeSelector->setObjectName("importModeSelector");
    modeSelector->addItem(tr("Выборочно (вручную)"));
    modeSelector->addItem(tr("Только новые записи"));
    modeSelector->addItem(tr("Всё, кроме удалений"));
    modeSelector->addItem(tr("Всё, включая удаления"));

    QLabel *modeLabel=new QLabel(tr("Режим импорта:"), this);

    QHBoxLayout *modeLayout=new QHBoxLayout();
    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(modeSelector);
    modeLayout->addStretch(1);

    changesTable=new QTableWidget(0, 4, this);
    changesTable->setObjectName("changeTable");
    changesTable->setHorizontalHeaderLabels(QStringList()
                                            << tr("Изменение")
                                            << tr("Diff")
                                            << tr("Объект")
                                            << tr("Описание"));
    changesTable->verticalHeader()->setVisible(false);
    changesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    changesTable->setAlternatingRowColors(true);
    changesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    changesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    changesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    changesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    changesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    detailsView=new QTextEdit(this);
    detailsView->setObjectName("changeDetails");
    detailsView->setReadOnly(true);
    detailsView->setMinimumHeight(180);

    QSplitter *splitter=new QSplitter(Qt::Vertical, this);
    splitter->addWidget(changesTable);
    splitter->addWidget(detailsView);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes(QList<int>() << 360 << 280);

    // Вкладка истории версий (changelog.json публикации, без git)
    historyView=new QTextEdit(this);
    historyView->setObjectName("changeHistory");
    historyView->setReadOnly(true);

    QTabWidget *tabs=new QTabWidget(this);
    tabs->setObjectName("changeTabs");
    tabs->addTab(splitter, tr("Изменения"));
    tabs->addTab(historyView, tr("История"));

    applyButton=new QPushButton(tr("Применить выбранное"), this);
    applyButton->setDefault(true);

    markViewedButton=new QPushButton(tr("Отметить просмотренным"), this);

    QPushButton *cancelButton=new QPushButton(tr("Отмена"), this);

    // Diff в отдельном окне: дополнение ко встроенному diff в деталях.
    // Активна, только когда у выбранного изменения есть построчный diff
    diffWindowButton=new QPushButton(tr("Diff в отдельном окне…"), this);
    diffWindowButton->setEnabled(false);

    QHBoxLayout *diffLayout=new QHBoxLayout();
    diffLayout->addStretch(1);
    diffLayout->addWidget(diffWindowButton);

    QHBoxLayout *buttonLayout=new QHBoxLayout();
    buttonLayout->addWidget(applyButton);
    buttonLayout->addWidget(markViewedButton);
    buttonLayout->addWidget(cancelButton);

    QVBoxLayout *layout=new QVBoxLayout(this);
    layout->addWidget(infoLabel);
    layout->addLayout(modeLayout);
    layout->addWidget(tabs, 1);
    layout->addLayout(diffLayout);
    layout->addLayout(buttonLayout);

    connect(changesTable, &QTableWidget::itemSelectionChanged,
            this,         &ChangeViewDialog::onSelectionChanged);
    connect(changesTable, &QTableWidget::itemChanged,
            this,         &ChangeViewDialog::onItemChanged);
    connect(changesTable, &QTableWidget::cellDoubleClicked,
            this,         &ChangeViewDialog::onCellDoubleClicked);
    connect(modeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,         &ChangeViewDialog::onModeChanged);
    connect(applyButton, &QPushButton::clicked,
            this,        &ChangeViewDialog::onApplyClicked);
    connect(markViewedButton, &QPushButton::clicked,
            this,             &ChangeViewDialog::onMarkViewedClicked);
    connect(diffWindowButton, &QPushButton::clicked,
            this,             &ChangeViewDialog::onDiffWindowClicked);
    connect(cancelButton, &QPushButton::clicked,
            this,         &QDialog::reject);
}


void ChangeViewDialog::fillTable(void)
{
    changesTable->setRowCount(0);
    textDiffCache.clear();

    for(int i=0; i<changes.size(); ++i)
    {
        const DiffChange &change=changes.at(i);

        changesTable->insertRow(changesTable->rowCount());

        QTableWidgetItem *typeItem=new QTableWidgetItem(typeLabel(change));
        QTableWidgetItem *objectItem=new QTableWidgetItem(change.title);
        QTableWidgetItem *summaryItem=new QTableWidgetItem(changeSummary(change));
        QTableWidgetItem *diffItem=new QTableWidgetItem();

        typeItem->setData(ChangeRowIndexRole, i);

        // Раскраска строк: новое — зелёное, изменённое — синее, удаление — красное
        const QBrush rowBrush(rowColor(change));
        typeItem->setForeground(rowBrush);
        objectItem->setForeground(rowBrush);
        summaryItem->setForeground(rowBrush);

        // Построчный diff имеет смысл только для изменённых записей:
        // маркер в отдельной колонке + кеш для деталей
        if(change.type==DiffChange::RecordUpdate && change.textChanged
           && recordTexts.contains(change.recordId))
        {
            const QPair<QString, QString> texts=recordTexts.value(change.recordId);
            const QString textDiff=TextDiff::diffHtml(texts.first, texts.second);
            if(!textDiff.isEmpty())
            {
                textDiffCache.insert(i, textDiff);
                diffItem->setText(tr("diff"));
                QFont diffFont=diffItem->font();
                diffFont.setBold(true);
                diffFont.setUnderline(true);
                diffItem->setFont(diffFont);
                diffItem->setForeground(QBrush(Qt::blue));
                diffItem->setToolTip(tr("Двойной клик — открыть diff в отдельном окне"));
            }
        }

        // Применимые изменения выбираются чекбоксом (записи и структура веток);
        // удаления по умолчанию не отмечены и требуют подтверждения
        if(isApplicable(change))
        {
            typeItem->setFlags(typeItem->flags() | Qt::ItemIsUserCheckable);
            typeItem->setCheckState(isDangerous(change)
                                        ? Qt::Unchecked : Qt::Checked);
        }
        else
        {
            typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsUserCheckable);
            typeItem->setForeground(QBrush(Qt::gray));
        }

        changesTable->setItem(i, 0, typeItem);
        changesTable->setItem(i, 1, diffItem);
        changesTable->setItem(i, 2, objectItem);
        changesTable->setItem(i, 3, summaryItem);
    }
}


QColor ChangeViewDialog::rowColor(const DiffChange &change)
{
    switch(change.type)
    {
        case DiffChange::RecordAdd:
        case DiffChange::BranchAdd:
            return Qt::darkGreen;

        case DiffChange::RecordDelete:
        case DiffChange::BranchDelete:
            return Qt::red;

        case DiffChange::RecordUpdate:
        case DiffChange::BranchRename:
        case DiffChange::BranchMove:
            return Qt::darkBlue;
    }
    return Qt::black;
}


bool ChangeViewDialog::isApplicable(const DiffChange &change)
{
    // Записи и структура веток импортируются; кнопки режимов отбирают дальше
    return true;
}


void ChangeViewDialog::applyModeChecks(ImportMode mode)
{
    syncingChecks=true;

    for(int row=0; row<changesTable->rowCount(); ++row)
    {
        QTableWidgetItem *item=changesTable->item(row, 0);
        if(!item || !(item->flags() & Qt::ItemIsUserCheckable))
            continue;

        const DiffChange &change=changes.at(item->data(ChangeRowIndexRole).toInt());

        bool checked=false;
        switch(mode)
        {
            case ImportMode::NewOnly:
                checked=(change.type==DiffChange::RecordAdd
                         || change.type==DiffChange::BranchAdd);
                break;
            case ImportMode::AllExceptDelete:
                checked=!isDangerous(change);
                break;
            case ImportMode::AllIncludeDelete:
                checked=true;
                break;
            case ImportMode::Custom:
                checked=!isDangerous(change);
                break;
        }

        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }

    syncingChecks=false;
    onSelectionChanged();
}


void ChangeViewDialog::fillHistory(void)
{
    if(publicationDir.isEmpty())
    {
        historyView->setPlainText(tr("История недоступна."));
        return;
    }

    QFile changelogFile(publicationDir+"/changelog.json");
    if(!changelogFile.open(QIODevice::ReadOnly))
    {
        historyView->setPlainText(tr("История версий отсутствует "
                                     "(публикация создана до введения журнала)."));
        return;
    }

    const QJsonDocument doc=QJsonDocument::fromJson(changelogFile.readAll());
    changelogFile.close();

    const QJsonArray entries=doc.object().value("entries").toArray();
    if(entries.isEmpty())
    {
        historyView->setPlainText(tr("История версий пуста."));
        return;
    }

    // Новые версии сверху
    QStringList html;
    for(int i=entries.size()-1; i>=0; --i)
    {
        const QJsonObject entry=entries.at(i).toObject();
        html << tr("<b>v%1</b> — %2 (%3)<br>")
                .arg(entry.value("v").toInt())
                .arg(entry.value("at").toString().toHtmlEscaped())
                .arg(entry.value("owner").toString().toHtmlEscaped());

        const QJsonArray versionChanges=entry.value("changes").toArray();
        if(versionChanges.isEmpty())
            html << tr("&nbsp;&nbsp;<i>первая публикация</i><br>");
        for(const QJsonValue &value : versionChanges)
        {
            const QJsonObject change=value.toObject();
            html << tr("&nbsp;&nbsp;• %1: %2<br>")
                    .arg(change.value("kind").toString().toHtmlEscaped())
                    .arg(change.value("title").toString().toHtmlEscaped());
        }
        html << "<br>";
    }

    historyView->setHtml(html.join(""));
}


void ChangeViewDialog::onModeChanged(int index)
{
    ImportMode mode=static_cast<ImportMode>(index);
    applyModeChecks(mode);
}


void ChangeViewDialog::onItemChanged(QTableWidgetItem *item)
{
    // Ручное изменение чекбокса возвращает режим в «выборочно»
    if(syncingChecks || !item)
        return;

    if(modeSelector->currentIndex()!=static_cast<int>(ImportMode::Custom))
    {
        const QSignalBlocker blocker(modeSelector);
        modeSelector->setCurrentIndex(static_cast<int>(ImportMode::Custom));
    }

    onSelectionChanged();
}


bool ChangeViewDialog::isDangerous(const DiffChange &change)
{
    return change.isDangerous();
}


QString ChangeViewDialog::typeLabel(const DiffChange &change)
{
    switch(change.type)
    {
        case DiffChange::RecordAdd:    return tr("Новая запись");
        case DiffChange::RecordUpdate: return tr("Изменение записи");
        case DiffChange::RecordDelete: return tr("Удаление записи");
        case DiffChange::BranchRename: return tr("Переименование ветки");
        case DiffChange::BranchMove:   return tr("Перемещение ветки");
        case DiffChange::BranchAdd:    return tr("Новая ветка");
        case DiffChange::BranchDelete: return tr("Удаление ветки");
    }
    return QString();
}


QString ChangeViewDialog::changeSummary(const DiffChange &change)
{
    if(change.type==DiffChange::RecordAdd)
        return tr("добавляется в личную базу");

    if(change.type==DiffChange::RecordDelete)
        return tr("ОПАСНО: локальная копия будет перенесена в корзину");

    if(change.type==DiffChange::BranchAdd)
        return tr("создаётся локальная копия ветки");

    if(change.type==DiffChange::BranchRename)
        return tr("переименование локальной копии ветки");

    if(change.type==DiffChange::BranchMove)
        return tr("перемещение локальной копии ветки");

    if(change.type==DiffChange::BranchDelete)
        return tr("ОПАСНО: ветка с содержимым уйдёт в корзину");

    if(change.type==DiffChange::RecordUpdate)
    {
        QStringList parts;
        QStringList fieldNames;
        for(const auto &field : change.fields)
            fieldNames << field.first;
        if(!fieldNames.isEmpty())
            parts << tr("поля: %1").arg(fieldNames.join(", "));
        if(change.textChanged)
            parts << tr("текст");
        if(!change.attachAdded.isEmpty())
            parts << tr("+вложения: %1").arg(change.attachAdded.join(", "));
        if(!change.attachRemoved.isEmpty())
            parts << tr("-вложения: %1").arg(change.attachRemoved.join(", "));
        return parts.join("; ");
    }

    return tr("структура дерева (не импортируется)");
}


QString ChangeViewDialog::changeDetails(int changeIndex) const
{
    QStringList html;

    if(changeIndex<0 || changeIndex>=changes.size())
        return QString();

    const DiffChange &change=changes.at(changeIndex);

    if(change.type==DiffChange::RecordAdd)
    {
        html << tr("<b>Новая запись</b>: %1<br>").arg(change.title.toHtmlEscaped());
    }
    else if(change.type==DiffChange::RecordUpdate)
    {
        html << tr("<b>Изменение записи</b>: %1<br>").arg(change.title.toHtmlEscaped());
        for(const auto &field : change.fields)
        {
            html << tr("<br><b>%1</b>:<br>").arg(field.first.toHtmlEscaped());
            html << tr("<i>Было:</i> %1<br>").arg(field.second.first.toHtmlEscaped());
            html << tr("<i>Стало:</i> %1<br>").arg(field.second.second.toHtmlEscaped());
        }
        if(change.textChanged)
        {
            // Построчный diff текста (локальная копия -> версия владельца).
            // Картинки в тексте — строками [изображение: ...], вложения —
            // списками ниже (бинарное содержимое не сравнивается)
            if(textDiffCache.contains(changeIndex))
                html << tr("<br><b>Текст записи</b>:<br>%1").arg(textDiffCache.value(changeIndex));
            else
                html << tr("<br><b>Текст записи</b> изменён владельцем.");
        }
        if(!change.attachAdded.isEmpty())
            html << tr("<br><b>Добавлены вложения</b>: %1").arg(change.attachAdded.join(", ").toHtmlEscaped());
        if(!change.attachRemoved.isEmpty())
            html << tr("<br><b>Удалены вложения</b>: %1").arg(change.attachRemoved.join(", ").toHtmlEscaped());
    }
    else if(change.type==DiffChange::RecordDelete)
    {
        html << tr("<b>Удаление записи</b>: %1<br>").arg(change.title.toHtmlEscaped());
        html << tr("Локальная копия будет перенесена в корзину и убрана из дерева.");
    }
    else if(change.type==DiffChange::BranchMove)
    {
        html << tr("<b>Перемещение ветки</b>: %1<br>").arg(change.title.toHtmlEscaped());
        html << tr("Локальная копия ветки будет перемещена к ветке-родителю владельца.");
    }
    else if(change.type==DiffChange::BranchDelete)
    {
        html << tr("<b>Удаление ветки</b>: %1<br>").arg(change.title.toHtmlEscaped());
        html << tr("Локальная копия ветки со всеми записями будет перенесена в корзину.");
    }
    else if(change.type==DiffChange::BranchAdd)
    {
        html << tr("<b>Новая ветка</b>: %1<br>").arg(change.title.toHtmlEscaped());
        html << tr("Будет создана локальная копия ветки; её записи импортируются отдельно.");
    }
    else if(change.type==DiffChange::BranchRename)
    {
        html << tr("<b>Переименование ветки</b>: %1<br>").arg(change.title.toHtmlEscaped());
        html << tr("Локальная копия ветки будет переименована.");
    }
    else
    {
        html << tr("<b>%1</b>: %2<br>").arg(typeLabel(change)).arg(change.title.toHtmlEscaped());
    }

    return html.join(QString());
}


void ChangeViewDialog::onSelectionChanged(void)
{
    selectedIndices.clear();

    for(int row=0; row<changesTable->rowCount(); ++row)
    {
        QTableWidgetItem *item=changesTable->item(row, 0);
        if(item && item->checkState()==Qt::Checked)
            selectedIndices.append(item->data(ChangeRowIndexRole).toInt());
    }

    const QList<QTableWidgetItem *> selectedItems=changesTable->selectedItems();
    if(!selectedItems.isEmpty())
    {
        int row=selectedItems.first()->row();
        int changeIndex=changesTable->item(row, 0)->data(ChangeRowIndexRole).toInt();
        if(changeIndex>=0 && changeIndex<changes.size())
            detailsView->setHtml(changeDetails(changeIndex));
    }

    applyButton->setEnabled(!selectedIndices.isEmpty());

    // Кнопка отдельного окна diff активна, только когда у выбранного
    // изменения есть построчный diff
    bool hasDiff=false;
    if(!selectedItems.isEmpty())
    {
        int row=selectedItems.first()->row();
        int changeIndex=changesTable->item(row, 0)->data(ChangeRowIndexRole).toInt();
        hasDiff=textDiffCache.contains(changeIndex);
    }
    diffWindowButton->setEnabled(hasDiff);
}


void ChangeViewDialog::onDiffWindowClicked(void)
{
    const QList<QTableWidgetItem *> selectedItems=changesTable->selectedItems();
    if(selectedItems.isEmpty())
        return;

    openDiffWindow(selectedItems.first()->row());
}


void ChangeViewDialog::onCellDoubleClicked(int row, int column)
{
    Q_UNUSED(column)

    openDiffWindow(row);
}


void ChangeViewDialog::openDiffWindow(int row)
{
    QTableWidgetItem *item=changesTable->item(row, 0);
    if(!item)
        return;

    const int changeIndex=item->data(ChangeRowIndexRole).toInt();
    if(changeIndex<0 || changeIndex>=changes.size())
        return;
    if(!textDiffCache.contains(changeIndex))
        return;

    const DiffChange &change=changes.at(changeIndex);

    QString oldHtml;
    QString newHtml;
    QPair<QString, QString> baseDirs;
    if(recordTexts.contains(change.recordId))
    {
        oldHtml=recordTexts.value(change.recordId).first;
        newHtml=recordTexts.value(change.recordId).second;
    }
    if(recordBaseDirs.contains(change.recordId))
        baseDirs=recordBaseDirs.value(change.recordId);

    TextDiffDialog diffDialog(change.title,
                              textDiffCache.value(changeIndex),
                              this,
                              oldHtml,
                              newHtml,
                              baseDirs);
    diffDialog.exec();
}


void ChangeViewDialog::onApplyClicked(void)
{
    if(selectedIndices.isEmpty())
    {
        QMessageBox::information(this, tr("Применить выбранное"),
                                 tr("Ничего не выбрано."));
        return;
    }

    // Опасные изменения (удаления) требуют явного подтверждения
    int dangerousCount=0;
    for(int idx : selectedIndices)
    {
        if(idx>=0 && idx<changes.size() && isDangerous(changes.at(idx)))
            dangerousCount++;
    }

    if(dangerousCount>0)
    {
        const int answer=QMessageBox::warning(
                    this, tr("Применить выбранное"),
                    tr("Выбрано опасных изменений: %1 (удаления).\n"
                       "Локальные копии будут перенесены в корзину. "
                       "Продолжить?").arg(dangerousCount),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No);
        if(answer!=QMessageBox::Yes)
            return;
    }

    selectedAction=Action::ApplySelected;
    accept();
}


void ChangeViewDialog::onMarkViewedClicked(void)
{
    const int answer=QMessageBox::question(
                this, tr("Отметить просмотренным"),
                tr("Все изменения будут помечены просмотренными "
                   "без импорта в личную базу. Продолжить?"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

    if(answer!=QMessageBox::Yes)
        return;

    selectedAction=Action::MarkViewed;
    accept();
}


ChangeViewDialog::Action ChangeViewDialog::action(void) const
{
    return selectedAction;
}


QList<int> ChangeViewDialog::selectedChangeIndices(void) const
{
    return selectedIndices;
}


ChangeViewDialog::ImportMode ChangeViewDialog::importMode(void) const
{
    return static_cast<ImportMode>(modeSelector->currentIndex());
}


// Строковое имя режима для журнала действий (идентификатор, не UI — без tr(),
// чтобы язык интерфейса не попадал в логи)
QString ChangeViewDialog::importModeName(void) const
{
    switch(importMode())
    {
        case ImportMode::NewOnly:         return QStringLiteral("newOnly");
        case ImportMode::AllExceptDelete: return QStringLiteral("allExceptDelete");
        case ImportMode::AllIncludeDelete:return QStringLiteral("allIncludeDelete");
        case ImportMode::Custom:          return QStringLiteral("custom");
    }
    return QStringLiteral("custom");
}