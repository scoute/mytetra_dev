#include <QMessageBox>
#include <QCommonStyle>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QTreeView>
#include <QStackedWidget>
#include <QAction>
#include <QStandardItemModel>
#include <QItemSelectionModel>

#include "MainToolbarSettingsScreen.h"
#include "models/appConfig/AppConfig.h"
#include "views/tree/TreeScreen.h"
#include "views/recordTable/RecordTableScreen.h"
#include "libraries/ShortcutManager.h"
#include "libraries/helpers/ObjectHelper.h"


extern AppConfig mytetraConfig;
extern ShortcutManager shortcutManager;


// Соответствие команды тулбара ключу горячих клавиш для подписи.
// Пусто - подписи берутся с самого действия (у таких команд
// отдельного хоткея нет)
static QString shortcutKeyFor(int panel, const QString &command)
{
  if(command=="syncro")
    return QString("misc-syncro");

  if(command=="findInBase")
    return QString("misc-findInBase");

  if(panel==MainToolbarSettingsScreen::TreePanel)
    return QString("tree-")+command;

  if(command=="walkHistoryPrevious")
    return QString("note-previousNote");

  if(command=="walkHistoryNext")
    return QString("note-nextNote");

  return QString("note-")+command;
}


MainToolbarSettingsScreen::MainToolbarSettingsScreen(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(tr("Toolbar settings"));

    // Стартовые списки: конфиг, а если не настраивалось - дефолты экранов
    QStringList treeOrder=mytetraConfig.getTreeToolbarOrder();

    if(treeOrder.isEmpty())
      treeOrder=TreeScreen::defaultToolbarOrder();

    pendingUsed[TreePanel]=treeOrder;
    loadedUsed[TreePanel]=treeOrder;

    QStringList notesOrder=mytetraConfig.getRecordToolbarOrder();

    if(notesOrder.isEmpty())
      notesOrder=RecordTableScreen::defaultToolbarOrder();

    pendingUsed[NotesPanel]=notesOrder;
    loadedUsed[NotesPanel]=notesOrder;

    QStringList extraOrder=mytetraConfig.getRecordExtraToolbarOrder();

    if(extraOrder.isEmpty())
      extraOrder=RecordTableScreen::defaultExtraToolbarOrder();

    pendingUsed[NotesExtraPanel]=extraOrder;
    loadedUsed[NotesExtraPanel]=extraOrder;

    setupUI();
    setupSignals();
    assembly();

    selectPanelListWidget->setCurrentRow(0);
}


MainToolbarSettingsScreen::~MainToolbarSettingsScreen()
{

}


void MainToolbarSettingsScreen::setupUI()
{
    // Выбор настраиваемой панели
    selectPanelListWidget=new QListWidget(this);
    selectPanelListWidget->setObjectName(QStringLiteral("selectPanelListWidget"));
    selectPanelListWidget->setAutoFillBackground(true);
    selectPanelListWidget->addItem(tr("Branches"));
    selectPanelListWidget->addItem(tr("Notes"));
    selectPanelListWidget->addItem(tr("Notes second line"));
    QFontMetrics metr(selectPanelListWidget->font());
    selectPanelListWidget->setFixedHeight(metr.height()*4);

    availableLabel=new QLabel(tr("Available buttons"), this);

    // Невидимая надпись над кнопками перемещения, нужна для вертикального выравнивания
    exchangeLabel=new QLabel(" ");

    // Кнопки для перемещения команд
    toUsedPushButton = new QPushButton(QIcon(":/resource/pic/move_right.svg"), "", this);
    toUsedPushButton->setShortcut(QKeySequence("Alt+Right"));

    toAvailablePushButton = new QPushButton(QIcon(":/resource/pic/move_left.svg"), "", this);
    toAvailablePushButton->setShortcut(QKeySequence("Alt+Left"));

    usedUpPushButton = new QPushButton(QIcon(":/resource/pic/move_up.svg"), "", this);
    usedUpPushButton->setShortcut(QKeySequence("Alt+Up"));

    usedDownPushButton = new QPushButton(QIcon(":/resource/pic/move_dn.svg"), "", this);
    usedDownPushButton->setShortcut(QKeySequence("Alt+Down"));

    usedLabel=new QLabel(tr("Current buttons"), this);

    // Модели списков
    availableModel=new QStandardItemModel(this);

    treeUsedModel=new QStandardItemModel(this);
    notesUsedModel=new QStandardItemModel(this);
    notesExtraUsedModel=new QStandardItemModel(this);

    // Виды списков
    availableView=new QTreeView(this);
    availableView->setObjectName(QStringLiteral("availableView"));
    availableView->setModel(availableModel);
    availableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    availableView->setHeaderHidden(true);
    availableView->setRootIsDecorated(false);

    treeUsedView=new QTreeView(this);
    treeUsedView->setObjectName(QStringLiteral("treeUsedView"));
    treeUsedView->setModel(treeUsedModel);
    treeUsedView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    treeUsedView->setHeaderHidden(true);
    treeUsedView->setRootIsDecorated(false);

    notesUsedView=new QTreeView(this);
    notesUsedView->setObjectName(QStringLiteral("notesUsedView"));
    notesUsedView->setModel(notesUsedModel);
    notesUsedView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    notesUsedView->setHeaderHidden(true);
    notesUsedView->setRootIsDecorated(false);

    notesExtraUsedView=new QTreeView(this);
    notesExtraUsedView->setObjectName(QStringLiteral("notesExtraUsedView"));
    notesExtraUsedView->setModel(notesExtraUsedModel);
    notesExtraUsedView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    notesExtraUsedView->setHeaderHidden(true);
    notesExtraUsedView->setRootIsDecorated(false);

    // Создание информационной кнопки
    QCommonStyle styleHelp;
    infoPushButton = new QPushButton(styleHelp.standardIcon(QStyle::SP_MessageBoxQuestion), "", this);

    // Создание набора диалоговых кнопок
    dialogButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    loadPanel(TreePanel);
}


QAction *MainToolbarSettingsScreen::commandAction(int panel, const QString &command) const
{
    if(panel==TreePanel)
    {
        TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

        if(treeScreen!=nullptr)
            return treeScreen->toolAction(command);

        return nullptr;
    }

    RecordTableScreen *recordScreen=find_object<RecordTableScreen>("recordTableScreen");

    if(recordScreen!=nullptr)
        return recordScreen->toolAction(command);

    return nullptr;
}


QString MainToolbarSettingsScreen::commandText(int panel, const QString &command) const
{
    if(command=="separator")
        return tr("<Separator>");

    if(command=="spring")
        return tr("<Spring>");

    const QString key=shortcutKeyFor(panel, command);
    const QString description=key.isEmpty() ? QString() : shortcutManager.getDescription(key);

    if(!description.isEmpty())
        return description;

    QAction *action=commandAction(panel, command);

    if(action!=nullptr && !action->text().isEmpty())
        return action->text();

    return command;
}


QIcon MainToolbarSettingsScreen::commandIcon(int panel, const QString &command) const
{
    QAction *action=commandAction(panel, command);

    if(action!=nullptr)
        return action->icon();

    return QIcon();
}


QStandardItemModel *MainToolbarSettingsScreen::usedModel(void) const
{
    const int row=selectPanelListWidget->currentRow();

    if(row==NotesPanel)
        return notesUsedModel;

    if(row==NotesExtraPanel)
        return notesExtraUsedModel;

    return treeUsedModel;
}


void MainToolbarSettingsScreen::loadPanel(int panel)
{
    // Доступные: объединение дефолтных минус задействованные.
    // Разделитель всегда доступен первым и неудаляем (как в редакторе)
    availableModel->clear();

    QStandardItem *separatorItem=new QStandardItem();
    separatorItem->setData(QString("separator"), Qt::UserRole);
    separatorItem->setData(tr("<Separator>"), Qt::DisplayRole);
    availableModel->appendRow(separatorItem);

    QStringList all;

    if(panel==TreePanel)
        all=TreeScreen::defaultToolbarOrder();
    else if(panel==NotesPanel)
        all=RecordTableScreen::defaultToolbarOrder();
    else
        all=RecordTableScreen::defaultExtraToolbarOrder();

    const QStringList used=pendingUsed.value(panel);

    foreach(const QString &command, all)
    {
        if(command=="separator" || command=="spring" || used.contains(command))
            continue;

        QStandardItem *item=new QStandardItem();
        item->setData(command, Qt::UserRole);
        item->setData(commandText(panel, command), Qt::DisplayRole);
        item->setIcon(commandIcon(panel, command));
        availableModel->appendRow(item);
    }

    // Пружина-распор: отдельная неудаляемая строка после разделителя
    {
        QStandardItem *springItem=new QStandardItem();
        springItem->setData(QString("spring"), Qt::UserRole);
        springItem->setData(tr("<Spring>"), Qt::DisplayRole);
        availableModel->appendRow(springItem);
    }

    // Задействованные выбранной панели
    QStandardItemModel *model=usedModel();

    model->clear();

    foreach(const QString &command, pendingUsed.value(panel))
    {
        QStandardItem *item=new QStandardItem();
        item->setData(command, Qt::UserRole);
        item->setData(commandText(panel, command), Qt::DisplayRole);
        item->setIcon(commandIcon(panel, command));
        model->appendRow(item);
    }

    // Строки, неприменимые к текущему режиму интерфейса (десктоп/мобила),
    // прячутся из обоих списков, но из моделей не удаляются: конфиг
    // хранит полный список, сборщик тулбара всё равно фильтрует по режиму.
    // Так мобильные "Пружина" и "Найти в базе" не смущают на десктопе
    applyModeVisibility(panel);
}


bool MainToolbarSettingsScreen::isCommandVisibleForMode(int panel, const QString &command) const
{
    if(command=="separator")
        return true;

    QString mode;

    if(panel==TreePanel)
        mode=TreeScreen::toolbarItemMode(command);
    else
        mode=RecordTableScreen::toolbarItemMode(command);

    if(mode.isEmpty())
        return true;

    const bool isMobile=(mytetraConfig.getInterfaceMode()=="mobile");

    if(mode=="mobile")
        return isMobile;

    return !isMobile;
}


void MainToolbarSettingsScreen::applyModeVisibility(int panel)
{
    for(int row=0; row<availableModel->rowCount(); ++row)
    {
        const QString command=availableModel->index(row, 0).data(Qt::UserRole).toString();
        availableView->setRowHidden(row, QModelIndex(), !isCommandVisibleForMode(panel, command));
    }

    QStandardItemModel *model=usedModel();
    QTreeView *view=currentUsedView();

    for(int row=0; row<model->rowCount(); ++row)
    {
        const QString command=model->index(row, 0).data(Qt::UserRole).toString();
        view->setRowHidden(row, QModelIndex(), !isCommandVisibleForMode(panel, command));
    }
}


QStringList MainToolbarSettingsScreen::usedCommands(void) const
{
    QStringList result;
    QStandardItemModel *model=usedModel();

    for(int i=0; i<model->rowCount(); ++i)
        result.append(model->index(i, 0).data(Qt::UserRole).toString());

    return result;
}


void MainToolbarSettingsScreen::setupSignals()
{
    // Переключение настраиваемой панели
    connect(selectPanelListWidget, &QListWidget::currentRowChanged,
            this, &MainToolbarSettingsScreen::onPanelChange);

    connect(toUsedPushButton, &QPushButton::clicked,
            this, &MainToolbarSettingsScreen::onMoveAvailableToUsed);

    connect(toAvailablePushButton, &QPushButton::clicked,
            this, &MainToolbarSettingsScreen::onMoveUsedToAvailable);

    connect(usedUpPushButton, &QPushButton::clicked,
            this, &MainToolbarSettingsScreen::onMoveSelectedUp);

    connect(usedDownPushButton, &QPushButton::clicked,
            this, &MainToolbarSettingsScreen::onMoveSelectedDown);

    connect(infoPushButton, &QPushButton::clicked,
            this, &MainToolbarSettingsScreen::onInfoClick);

    connect(dialogButtonBox, &QDialogButtonBox::accepted,
            this, &MainToolbarSettingsScreen::applyChanges);

    connect(dialogButtonBox, &QDialogButtonBox::rejected,
            this, &MainToolbarSettingsScreen::close);
}


void MainToolbarSettingsScreen::assembly()
{
    usedStackedWidget=new QStackedWidget(this);
    usedStackedWidget->addWidget(treeUsedView);
    usedStackedWidget->addWidget(notesUsedView);
    usedStackedWidget->addWidget(notesExtraUsedView);

    QVBoxLayout *moveLayout=new QVBoxLayout();
    moveLayout->addWidget(toUsedPushButton);
    moveLayout->addWidget(toAvailablePushButton);
    moveLayout->addWidget(usedUpPushButton);
    moveLayout->addWidget(usedDownPushButton);
    moveLayout->addStretch(1);
    moveLayout->addWidget(infoPushButton);

    availableLayout=new QVBoxLayout();
    availableLayout->addWidget(availableLabel);
    availableLayout->addWidget(availableView);

    exchangeLayout=new QHBoxLayout();
    exchangeLayout->addWidget(exchangeLabel);
    exchangeLayout->addLayout(moveLayout);

    usedLayout=new QVBoxLayout();
    usedLayout->addWidget(usedLabel);
    usedLayout->addWidget(usedStackedWidget);

    QHBoxLayout *middleLayout=new QHBoxLayout();
    middleLayout->addLayout(availableLayout);
    middleLayout->addLayout(exchangeLayout);
    middleLayout->addLayout(usedLayout);

    screenLayout=new QVBoxLayout();
    screenLayout->addWidget(selectPanelListWidget);
    screenLayout->addLayout(middleLayout);
    screenLayout->addWidget(dialogButtonBox);

    setLayout(screenLayout);
}


void MainToolbarSettingsScreen::onPanelChange(int row)
{
    if(row<0)
        return;

    // Запоминаем правки текущей панели перед переключением
    // (индекс виджета совпадает с прошлой выбранной панелью,
    // поэтому сначала сохраняем по нему, потом переключаем вид)
    const int prevIndex=usedStackedWidget->currentIndex();

    if(prevIndex>=0)
        pendingUsed[prevIndex]=usedCommandsFor(prevIndex);

    usedStackedWidget->setCurrentIndex(row);
    loadPanel(row);

    QString text = QString("%1: %2")
        .arg(tr("Current buttons"))
        .arg(selectPanelListWidget->item(row)->text());

    usedLabel->setText(text);
}


QStringList MainToolbarSettingsScreen::usedCommandsFor(int panel) const
{
    QStringList result;
    QStandardItemModel *model=nullptr;

    if(panel==NotesPanel)
        model=notesUsedModel;
    else if(panel==NotesExtraPanel)
        model=notesExtraUsedModel;
    else
        model=treeUsedModel;

    for(int i=0; i<model->rowCount(); ++i)
        result.append(model->index(i, 0).data(Qt::UserRole).toString());

    return result;
}


void MainToolbarSettingsScreen::onMoveAvailableToUsed()
{
    QModelIndex selectedAvailableIndex=availableView->selectionModel()->currentIndex();

    if(!selectedAvailableIndex.isValid() ||
       availableView->isRowHidden(selectedAvailableIndex.row(), QModelIndex()))
        return;

    const QString command=selectedAvailableIndex.data(Qt::UserRole).toString();
    const int panel=selectPanelListWidget->currentRow();

    QStandardItemModel *model=usedModel();
    QModelIndex toolbarSelectedIndex=currentUsedView()->selectionModel()->currentIndex();
    int newRow=toolbarSelectedIndex.isValid() ? toolbarSelectedIndex.row() : 0;

    // Дубль запрещён (кроме разделителя и пружины, их может быть много)
    if(command!="separator" && command!="spring")
    {
        for(int i=0; i<model->rowCount(); ++i)
            if(model->index(i, 0).data(Qt::UserRole).toString()==command)
            {
                QMessageBox msbox(QMessageBox::Icon::Warning, "mytetra",
                                  tr("<b>%1</b> is already on this toolbar.").arg(command),
                                  QMessageBox::Ok, this);
                msbox.exec();
                return;
            }
    }

    QStandardItem *newItem=new QStandardItem();
    newItem->setData(command, Qt::UserRole);
    newItem->setData(commandText(panel, command), Qt::DisplayRole);
    newItem->setIcon(commandIcon(panel, command));
    model->insertRow(newRow, newItem);

    // Разделитель (строка 0) и пружина из доступных не удаляются
    if(selectedAvailableIndex.row()!=0 && command!="spring")
        availableModel->removeRow(selectedAvailableIndex.row());
}


QTreeView *MainToolbarSettingsScreen::currentUsedView(void) const
{
    const int row=selectPanelListWidget->currentRow();

    if(row==NotesPanel)
        return notesUsedView;

    if(row==NotesExtraPanel)
        return notesExtraUsedView;

    return treeUsedView;
}


void MainToolbarSettingsScreen::onMoveUsedToAvailable()
{
    QTreeView *view=currentUsedView();
    QModelIndex selectedIndex=view->selectionModel()->currentIndex();

    if(!selectedIndex.isValid())
        return;

    QStandardItemModel *model=usedModel();

    // В модели должна остаться хотя бы одна команда
    if(model->rowCount()==1)
        return;

    const QString command=selectedIndex.data(Qt::UserRole).toString();

    if(command=="separator" || command=="spring")
    {
        // Разделитель и пружина просто убираются из задействованных
        model->removeRow(selectedIndex.row());
        return;
    }

    QStandardItem *newItem=new QStandardItem();
    newItem->setData(command, Qt::UserRole);
    newItem->setData(commandText(selectPanelListWidget->currentRow(), command), Qt::DisplayRole);
    newItem->setIcon(commandIcon(selectPanelListWidget->currentRow(), command));
    availableModel->appendRow(newItem);

    model->removeRow(selectedIndex.row());
}


void MainToolbarSettingsScreen::onMoveSelectedUp()
{
    QTreeView *view=currentUsedView();
    QStandardItemModel *model=usedModel();
    QModelIndex selectedIndex=view->selectionModel()->currentIndex();

    if(!selectedIndex.isValid() || view->isRowHidden(selectedIndex.row(), QModelIndex()))
        return;

    // Ближайшая видимая строка выше (скрытые по режиму перепрыгиваются)
    int target=-1;

    for(int row=selectedIndex.row()-1; row>=0; --row)
        if(!view->isRowHidden(row, QModelIndex()))
        {
            target=row;
            break;
        }

    if(target<0)
        return;

    const int row=selectedIndex.row();
    QList<QStandardItem *> items=model->takeRow(row);
    model->insertRow(target, items);
    view->setCurrentIndex(model->index(target, 0));
}


void MainToolbarSettingsScreen::onMoveSelectedDown()
{
    QTreeView *view=currentUsedView();
    QStandardItemModel *model=usedModel();
    QModelIndex selectedIndex=view->selectionModel()->currentIndex();

    if(!selectedIndex.isValid() || view->isRowHidden(selectedIndex.row(), QModelIndex()))
        return;

    // Ближайшая видимая строка ниже (скрытые по режиму перепрыгиваются)
    int target=-1;

    for(int row=selectedIndex.row()+1; row<model->rowCount(); ++row)
        if(!view->isRowHidden(row, QModelIndex()))
        {
            target=row;
            break;
        }

    if(target<0 || target>=model->rowCount())
        return;

    const int row=selectedIndex.row();
    QList<QStandardItem *> items=model->takeRow(row);
    // После takeRow индексы ниже съехали на один вверх
    model->insertRow(target, items);
    view->setCurrentIndex(model->index(target, 0));
}


void MainToolbarSettingsScreen::onInfoClick()
{
    QString message = tr("<b>Information</b> for toolbar buttons setup:");
    message += "<br/>";
    message += "<br/>";
    message += tr("The command is inserted <b>above the cursor</b> in the used commands list.");
    message += "<br/>";
    message += tr("Panel toggle buttons (tags, history, backlinks) always stay at the end of the second notes line and are not listed here. Hidden shortcut-only buttons stay active too.");
    message += "<br/>";
    message += "<br/>";
    message += tr("<b>Shortcuts:</b>");
    message += "<br/>";
    message += "- Move the selected command from the All Available Commands Model <b>to</b> the Commands Used Toolbar: <b>Alt+Right</b>.";
    message += "<br/>";
    message += "- Move the selected command from the Commands Used Toolbar <b>to</b> the All Available Commands Model: <b>Alt+Left</b>.";
    message += "<br/>";
    message += "- Commands used toolbar: Move <b>up</b> the selected command: <b>Alt+Up</b>.";
    message += "<br/>";
    message += "- Commands used toolbar: Move <b>down</b> the selected command: <b>Alt+Down</b>.";

    QMessageBox *msgBox = new QMessageBox(QMessageBox::Icon::Information, tr("Information"), message, QMessageBox::Ok, this);
    msgBox->exec();
}


void MainToolbarSettingsScreen::applyChanges()
{
    // Запоминаем правки открытой панели
    pendingUsed[selectPanelListWidget->currentRow()]=usedCommands();

    if(pendingUsed[TreePanel]!=loadedUsed[TreePanel])
        mytetraConfig.setTreeToolbarOrder(pendingUsed[TreePanel]);

    if(pendingUsed[NotesPanel]!=loadedUsed[NotesPanel])
        mytetraConfig.setRecordToolbarOrder(pendingUsed[NotesPanel]);

    if(pendingUsed[NotesExtraPanel]!=loadedUsed[NotesExtraPanel])
        mytetraConfig.setRecordExtraToolbarOrder(pendingUsed[NotesExtraPanel]);

    // Живое перестроение тулбаров без перезапуска
    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

    if(treeScreen!=nullptr)
        treeScreen->rebuildToolbar();

    RecordTableScreen *recordScreen=find_object<RecordTableScreen>("recordTableScreen");

    if(recordScreen!=nullptr)
        recordScreen->rebuildToolbars();

    close();
}
