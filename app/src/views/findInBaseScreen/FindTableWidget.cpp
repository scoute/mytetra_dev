#include <QWidget>
#include <QRegion>
#include <QLayout>
#include <QString>
#include <QWidget>
#include <QVariant>
#include <QTableWidget>
#include <QtDebug>
#include <QHeaderView>
#include <QPaintEvent>
#include <QTableView>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QTextCursor>
#include <QStyledItemDelegate>
#include <QApplication>

#include <algorithm>

#include "FindTableWidget.h"
#include "views/mainWindow/MainWindow.h"
#include "views/record/MetaEditor.h"
#include "models/appConfig/AppConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"
#include "views/tree/KnowTreeView.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/GestureHelper.h"
#include "libraries/helpers/CssHelper.h"

#define USER_ROLE_PATH      Qt::UserRole
#define USER_ROLE_RECORD_ID Qt::UserRole+1
#define USER_ROLE_IS_RECORD Qt::UserRole+2

extern AppConfig mytetraConfig;


FindTableWidget::FindTableWidget(QWidget *parent) : QWidget(parent)
{
    // По факту объект этого класса имеется в единичном экземпляре. Такой объект сам задает себе имя
    this->setObjectName("findTableWidget");

    setupUI();
    setupModels();
    setupSignals();
    assembly();

    clearAll();
}


FindTableWidget::~FindTableWidget(void)
{

}


void FindTableWidget::setupUI(void)
{
    findTableView=new QTableView(this);
    findTableView->setObjectName("findTableView");
    findTableView->setMinimumSize(1,1);
    findTableView->horizontalHeader()->hide();

    // Установка высоты строки с принудительной стилизацией (если это необходимо),
    // так как стилизация через QSS для элементов QTableView полноценно не работает
    // У таблицы есть вертикальные заголовки, для каждой строки, в которых отображается номер строки.
    // При задании высоты вертикального заголовка, высота применяется и для всех ячеек в строке.
    findTableView->verticalHeader()->setDefaultSectionSize ( findTableView->verticalHeader()->minimumSectionSize () );
    int height=mytetraConfig.getUglyQssReplaceHeightForTableView();
    if(height!=0)
        findTableView->verticalHeader()->setDefaultSectionSize( height );
    if(mytetraConfig.getInterfaceMode()=="mobile")
        findTableView->verticalHeader()->setDefaultSectionSize( CssHelper::getCalculateIconSizePx() );

    // Минимальная высота панели: заголовок плюс одна строка результата.
    // Иначе сплиттер схлопывает панель до нескольких пикселей
    // и непонятно что результаты вообще есть
    int headerHeight=findTableView->fontMetrics().height()+8;

    if(!findTableView->horizontalHeader()->isHidden())
        headerHeight=qMax(headerHeight, findTableView->horizontalHeader()->height());

    int rowHeight=findTableView->verticalHeader()->defaultSectionSize();

    if(rowHeight<=0)
        rowHeight=findTableView->fontMetrics().height()+8;

    findTableView->setMinimumHeight(headerHeight+rowHeight+2*findTableView->frameWidth());

    // Устанавливается режим что могут выделяться только строки
    // а не отдельный item таблицы
    findTableView->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Устанавливается режим что редактирование невозможно
    findTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Настройка области виджета для кинетической прокрутки
    GestureHelper::setKineticScrollArea( qobject_cast<QAbstractItemView*>(findTableView) );
}


void FindTableWidget::setupModels(void)
{
    // Создается модель табличных данных
    findTableModel=new QStandardItemModel(this);

    // Модель привязывается к виду
    findTableView->setModel(findTableModel);
}


void FindTableWidget::setupSignals(void)
{
    connect(findTableView, &QTableView::activated, this, &FindTableWidget::selectCell);
}


void FindTableWidget::assembly(void)
{
    QHBoxLayout *central_layout=new QHBoxLayout();

    central_layout->addWidget(findTableView);
    central_layout->setContentsMargins(0,0,0,0); // Границы убираются

    this->setLayout(central_layout);
}


void FindTableWidget::updateColumnsWidth(void)
{
    findTableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    findTableView->hide();
    findTableView->show();
}


void FindTableWidget::clearAll(void)
{
    // Модель таблицы очищается
    findTableModel->setRowCount(0);
    findTableModel->setColumnCount(0);

    // В модели таблицы устанавливаются три колонки: совпадения, заголовок, детали
    findTableModel->setColumnCount(3);

    // В модели устанавливаются заголовки колонок
    QStringList list;
    list << tr("Matches") << tr("Title") << tr("Details");
    findTableModel->setHorizontalHeaderLabels(list);

    findTableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    findTableView->horizontalHeader()->setStretchLastSection(true);
}


void FindTableWidget::addRow(QString title, QString branchName, QString tags, QStringList path, QString recordId, int matchCount, bool isRecord)
{
    int i=findTableModel->rowCount();

    findTableModel->insertRow(i);

    // Принудительная стилизация, так как стилизация через QSS для элементов QTableView полноценно не работает
    // int height=mytetraconfig.getUglyQssReplaceHeightForTableView();
    // if(height!=0)
    //  findTableView->setRowHeight(i, height);

    // Количество совпадений в записи или ветке. Первый столбец, до названия
    QStandardItem *item_matches=new QStandardItem();
    item_matches->setText(QString::number(matchCount));
    item_matches->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Заголовок (название) записи
    QStandardItem *item_title=new QStandardItem();
    item_title->setText(title);

    // В ячейке заголовка также хранится информация о пути к ветке,
    // номере записи в таблице конечных записей и признак что это запись
    // (а не строка ветки). Признак нужен чтобы открывать поиск по заметке
    // только для настоящих записей
    qDebug() << "Path to record" << path;
    item_title->setData(QVariant(path), USER_ROLE_PATH);
    item_title->setData(QVariant(recordId), USER_ROLE_RECORD_ID);
    item_title->setData(QVariant(isRecord), USER_ROLE_IS_RECORD);

    // Информация о записи
    QStandardItem *item_info=new QStandardItem();
    QString info;
    tags=tags.trimmed();
    if(tags.length()>0)
        item_info->setText(branchName+" ("+tags+")");
    else
        item_info->setText(branchName);

    findTableModel->setItem(i, 0, item_matches);
    findTableModel->setItem(i, 1, item_title);
    findTableModel->setItem(i, 2, item_info);

    qDebug() << "In findtablewidget add_row() row count " << findTableModel->rowCount();
}


int FindTableWidget::getRowCount()
{
    return findTableModel->rowCount();
}


void FindTableWidget::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    if(overdrawMessage.length()>0)
    {
        QPainter painter(this);
        painter.setPen( QApplication::palette().color(QPalette::ToolTipText) );
        painter.drawText(rect(), Qt::AlignCenter, overdrawMessage);
    }
}


void FindTableWidget::setOverdrawMessage(const QString iOverdrawMessage)
{
    overdrawMessage=iOverdrawMessage;

    if(overdrawMessage.length()>0)
        findTableView->hide(); // Скрывается виджет таблицы, потому что он перекрывает выводимую надпись
    else
        findTableView->show();

    // Обновляется внешний вид виджета
    update();
}


void FindTableWidget::setLastSearch(const QString &query, QTextDocument::FindFlags flags)
{
    lastSearchQuery=query;
    lastSearchFlags=flags;
}


// Агрегация счетчиков строк веток снизу вверх: каждая ветка показывает
// суммарные совпадения по всему своему поддереву. Строки записей уже
// содержат свои итоги, строки дочерних веток к этому моменту тоже
// посчитаны (обход от самых глубоких). Прямые потомки ветки это строки
// записей с тем же путем и строки веток с путем на один элемент длиннее
void FindTableWidget::aggregateBranchCounts(void)
{
    int rows=findTableModel->rowCount();

    // Индексы строк веток, сортировка по глубине пути по убыванию
    QList<int> branchRows;
    for(int i=0; i<rows; ++i)
    {
        QStandardItem *titleItem=findTableModel->item(i, 1);

        if(titleItem==nullptr)
            continue;

        if(!titleItem->data(USER_ROLE_IS_RECORD).toBool())
            branchRows << i;
    }

    std::sort(branchRows.begin(), branchRows.end(),
              [this](int first, int second)
              {
                return findTableModel->item(first, 1)->data(USER_ROLE_PATH).toStringList().size() >
                       findTableModel->item(second, 1)->data(USER_ROLE_PATH).toStringList().size();
              });

    // Подсчет итогов от глубоких веток к корневым
    foreach(int branchRow, branchRows)
    {
        QStandardItem *branchTitle=findTableModel->item(branchRow, 1);
        QStringList branchPath=branchTitle->data(USER_ROLE_PATH).toStringList();

        QStandardItem *branchCount=findTableModel->item(branchRow, 0);
        int total=branchCount->text().toInt();

        for(int i=0; i<rows; ++i)
        {
            if(i==branchRow)
                continue;

            QStandardItem *titleItem=findTableModel->item(i, 1);

            if(titleItem==nullptr)
                continue;

            QStringList rowPath=titleItem->data(USER_ROLE_PATH).toStringList();

            // Прямая запись ветки: путь совпадает с путем ветки
            if(titleItem->data(USER_ROLE_IS_RECORD).toBool())
            {
                if(rowPath==branchPath)
                    total+=findTableModel->item(i, 0)->text().toInt();

                continue;
            }

            // Дочерняя ветка: путь длиннее ровно на один элемент
            // с префиксом пути родителя. Ее итог уже посчитан
            if(rowPath.size()==branchPath.size()+1 &&
               rowPath.mid(0, branchPath.size())==branchPath)
                total+=findTableModel->item(i, 0)->text().toInt();
        }

        branchCount->setText(QString::number(total));
    }
}


// void FindTableWidget::selectCell(int row, int column)
void FindTableWidget::selectCell(const QModelIndex & index)
{
    QStandardItem *clickItem=findTableModel->itemFromIndex(index);
    QStandardItem *item=findTableModel->item(clickItem->row(), 1); // Данные находятся в столбце заголовка с индексом 1

    // Выясняется путь к ветке и номер в таблице конечных записей
    QStringList path=item->data(USER_ROLE_PATH).toStringList();
    QString recordId=item->data(USER_ROLE_RECORD_ID).toString();
    bool isRecord=item->data(USER_ROLE_IS_RECORD).toBool();

    qDebug() << "Get path to record:" << path;

    // Редактор переводится в режим отображения текста записи (а не приаттаченных файлов)
    MetaEditor *edView=find_object<MetaEditor>("editorScreen");
    edView->switchToEditorLayout();

    find_object<MainWindow>("mainwindow")->setTreeAndRecordtablePositions(path, recordId);

    // Мост в поиск по заметке: открытая запись сразу подсвечивается
    // тем же запросом с переходом к первому совпадению. Для строк веток
    // подсветка не запускается: там открыта другая запись.
    // Полоска открывается только если запросу есть совпадения в тексте:
    // иначе совпало поле author/url/tags, и мост показывал бы
    // бессмысленное "Нет совпадений"
    if(isRecord && !lastSearchQuery.isEmpty())
    {
        // Дать редактору дочитать текст открывшейся записи
        QCoreApplication::processEvents();

        QString branchId=path.isEmpty() ? QString() : path.last();

        if(noteTextContains(branchId, recordId, lastSearchQuery, lastSearchFlags))
            edView->startFind(lastSearchQuery, lastSearchFlags);
    }
}


// Есть ли запросу совпадения в тексте записи. Проверка тем же движком
// что подсветка полоски (QTextDocument::find с теми же флагами)
bool FindTableWidget::noteTextContains(const QString &branchId,
                                       const QString &recordId,
                                       const QString &query,
                                       QTextDocument::FindFlags flags)
{
    if(branchId.isEmpty() || recordId.isEmpty() || query.isEmpty())
        return false;

    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");
    if(treeView==nullptr)
        return false;

    KnowTreeModel *model=static_cast<KnowTreeModel *>(treeView->model());
    if(model==nullptr)
        return false;

    TreeItem *branchItem=model->getItemById(branchId);
    if(branchItem==nullptr)
        return false;

    RecordTableData *table=branchItem->recordtableGetTableData();
    if(table==nullptr)
        return false;

    int pos=table->getPosById(recordId);
    if(pos<0)
        return false;

    // Направление в проверке не участвует: достаточно знать сам факт
    QTextDocument document;
    document.setHtml(table->getText(pos));

    QTextCursor cursor(&document);
    cursor=document.find(query, cursor, flags & ~QTextDocument::FindBackward);

    return !cursor.isNull();
}

