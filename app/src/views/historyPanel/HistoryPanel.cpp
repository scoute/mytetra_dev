#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QMenu>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QShowEvent>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QDateTime>

#include "HistoryPanel.h"

#include "libraries/VisitHistory.h"
#include "models/appConfig/AppConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "models/recordTable/RecordTableData.h"
#include "models/attachTable/AttachTableData.h"
#include "views/tree/KnowTreeView.h"
#include "views/tree/TreeScreen.h"
#include "views/mainWindow/MainWindow.h"
#include "views/contentGallery/ContentGallery.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/ObjectHelper.h"


extern VisitHistory visitHistory;
extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;


// Пункт с числовой сортировкой: даты, счетчики и размеры сортируются
// по значению из UserRole, а не по отображаемому тексту
class HistorySortItem : public QTableWidgetItem
{
public:

    explicit HistorySortItem(const QString &text, qlonglong sortValue)
        : QTableWidgetItem(text)
    {
        setData(Qt::UserRole, sortValue);
    }

    bool operator<(const QTableWidgetItem &other) const override
    {
        return data(Qt::UserRole).toLongLong() < other.data(Qt::UserRole).toLongLong();
    }
};


HistoryPanel::HistoryPanel(QWidget *parent) : QWidget(parent),
    treeMetadataConnected(false),
    resizingProgrammatically(false)
{
    setupUi();
    assembly();
    setupSignals();

    refreshHistory();
}


HistoryPanel::~HistoryPanel(void)
{

}


void HistoryPanel::setupUi(void)
{
    // Колонок семь, уже - только с горизонтальной прокруткой
    setMinimumWidth(360);

    filterEdit=new QLineEdit(this);
    filterEdit->setPlaceholderText(tr("Filter by note title"));
    filterEdit->setClearButtonEnabled(true);

    // Все колонки сортируются кликом по заголовку: дата, частота,
    // картинки, файлы и размер как числа через HistorySortItem
    historyTable=new QTableWidget(this);
    historyTable->setColumnCount(7);
    historyTable->setHorizontalHeaderLabels(QStringList() << tr("Note") << tr("Branch")
                                            << tr("Last visit") << tr("Visits")
                                            << tr("Images") << tr("Files") << tr("Size"));
    historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for(int column=1; column<7; ++column)
        historyTable->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    historyTable->verticalHeader()->setVisible(false);
    historyTable->setSortingEnabled(false);

    clearButton=new QPushButton(tr("Clear"), this);
    clearButton->setToolTip(tr("Forget all visit history"));

    // Живое обновление сыпет визитами на каждый клик по заметкам.
    // Пачка сливается в одну пересборку
    refreshDebounce=new QTimer(this);
    refreshDebounce->setSingleShot(true);
}


void HistoryPanel::assembly(void)
{
    QHBoxLayout *topLayout=new QHBoxLayout();
    topLayout->addWidget(filterEdit, 1);
    topLayout->addWidget(clearButton);

    QVBoxLayout *panelLayout=new QVBoxLayout();
    panelLayout->addLayout(topLayout);
    panelLayout->addWidget(historyTable, 1);

    setLayout(panelLayout);
}


void HistoryPanel::setupSignals(void)
{
    connect(filterEdit, &QLineEdit::textChanged,
            this,       &HistoryPanel::refreshHistory);

    // Прыжок к заметке двойным кликом чтобы не сбивать текущую
    // заметку случайным одинарным кликом
    connect(historyTable, &QTableWidget::cellDoubleClicked,
            this,         &HistoryPanel::onNoteDoubleClicked);

    historyTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(historyTable, &QTableWidget::customContextMenuRequested,
            this,         &HistoryPanel::onHistoryContextMenu);

    connect(clearButton, &QPushButton::clicked,
            this,        &HistoryPanel::onClearHistory);

    connect(&visitHistory, &VisitHistory::visitLogged,
            this,          &HistoryPanel::onVisitLogged);

    connect(refreshDebounce, &QTimer::timeout,
            this,            &HistoryPanel::refreshHistory);

    connect(historyTable->horizontalHeader(), &QHeaderView::sectionResized,
            this,                             &HistoryPanel::onSectionResized);
}


void HistoryPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    refreshHistory();
}


void HistoryPanel::onVisitLogged(const QString &id)
{
    Q_UNUSED(id);

    if(isVisible())
        refreshDebounce->start(400);
}


// Метаданные дерева изменились (удаление, переименование):
// мертвые id пропадают из таблицы при следующей пересборке
void HistoryPanel::onTreeMetadataSaved(void)
{
    if(isVisible())
        refreshHistory();
}


// Прыжок к заметке через готовый механизм позиционирования.
// Путь резолвится вживую: ветку могли переместить после визита
void HistoryPanel::goToNote(const QString &id)
{
    KnowTreeModel *dataModel=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());
    if(dataModel==nullptr)
        return;

    const QStringList path=dataModel->getRecordPath(id);
    if(path.isEmpty() || !dataModel->isItemValid(path))
        return;

    MainWindow *mainWindow=find_object<MainWindow>("mainwindow");
    if(mainWindow==nullptr)
        return;

    mainWindow->setTreeAndRecordtablePositions(path, id);
}


void HistoryPanel::onNoteDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    QTableWidgetItem *noteItem=historyTable->item(row, 0);
    if(noteItem==nullptr)
        return;

    const QString id=noteItem->data(Qt::UserRole).toString();
    if(id.isEmpty())
        return;

    goToNote(id);
}


// Контекстное меню строки: прыжок, забыть заметку
void HistoryPanel::onHistoryContextMenu(const QPoint &pos)
{
    QTableWidgetItem *noteItem=historyTable->itemAt(pos);
    if(noteItem==nullptr)
        return;

    QTableWidgetItem *firstColumnItem=historyTable->item(noteItem->row(), 0);
    if(firstColumnItem==nullptr)
        return;

    const QString id=firstColumnItem->data(Qt::UserRole).toString();
    if(id.isEmpty())
        return;

    contextNoteId=id;

    QMenu menu(this);
    QAction *goAction=menu.addAction(tr("Go to note"));
    QAction *forgetAction=menu.addAction(tr("Forget this note"));

    QAction *chosen=menu.exec(historyTable->viewport()->mapToGlobal(pos));
    if(chosen==nullptr)
        return;

    if(chosen==goAction)
        goToNote(id);
    else if(chosen==forgetAction)
        onForgetNote();
}


void HistoryPanel::onClearHistory(void)
{
    const int answer=QMessageBox::question(this,
                                           tr("Clear visit history"),
                                           tr("Forget all visit history?"),
                                           QMessageBox::Yes | QMessageBox::No,
                                           QMessageBox::No);
    if(answer!=QMessageBox::Yes)
        return;

    visitHistory.clear();
    refreshHistory();
}


void HistoryPanel::onForgetNote(void)
{
    if(contextNoteId.isEmpty())
        return;

    visitHistory.forget(contextNoteId);
    contextNoteId.clear();
    refreshHistory();
}


// Пользователь подвигал границу колонки: ширина запоминается
// чтобы пересборка ее не сбрасывала
void HistoryPanel::onSectionResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(oldSize);

    if(resizingProgrammatically)
        return;

    userColumnWidths[logicalIndex]=newSize;
}


// Относительное время для колонки: "только что", "5 мин назад"...
QString HistoryPanel::formatVisitTime(qint64 msecs)
{
    const qint64 seconds=(QDateTime::currentMSecsSinceEpoch()-msecs)/1000;

    if(seconds < 60)
        return tr("just now");

    const qint64 minutes=seconds/60;
    if(minutes < 60)
        return tr("%1 min ago").arg(minutes);

    const qint64 hours=minutes/60;
    if(hours < 24)
        return tr("%1 h ago").arg(hours);

    const qint64 days=hours/24;
    if(days < 7)
        return tr("%1 d ago").arg(days);

    return QDateTime::fromMSecsSinceEpoch(msecs).date().toString(Qt::DefaultLocaleShortDate);
}


// Суммарный размер файлов каталога записи в байтах.
// Каталоги записей плоские, рекурсия не нужна
qint64 HistoryPanel::recordDirSize(const QString &recordDir)
{
    qint64 totalSize=0;

    QDir dir(recordDir);
    if(!dir.exists())
        return 0;

    const QFileInfoList files=dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for(const QFileInfo &fileInfo : files)
        totalSize+=fileInfo.size();

    return totalSize;
}


// Пересборка таблицы из журнала: агрегат, фильтр, сортировка заголовком.
// Мертвые id (удаленные заметки) отбрасываются, шифрованные без пароля
// показываются заглушкой
void HistoryPanel::refreshHistory(void)
{
    // Подписка на изменения дерева делается один раз и только когда
    // дерево уже собрано: иначе find_object() на отсутствующем объекте
    // завершает программу
    if(!treeMetadataConnected)
    {
        TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
        if(treeScreen==nullptr)
            return;

        treeMetadataConnected=true;

        connect(treeScreen, &TreeScreen::treeMetadataSaved,
                this,       &HistoryPanel::onTreeMetadataSaved);
    }

    KnowTreeModel *dataModel=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());

    const QList<VisitStat> visitStats=visitHistory.stats();

    const QString filter=filterEdit->text().toLower();

    historyTable->setSortingEnabled(false);
    historyTable->setRowCount(0);

    // Карта id -> данные строки одним проходом по дереву.
    // Шифрованные ветки без пароля пропускаются: такие заметки ниже
    // покажутся заглушкой по медленному пути
    struct NoteInfo
    {
        QString name;
        QString branch;
        int attachCount;
        QString recordDir;
    };
    QMap<QString, NoteInfo> noteInfo;
    if(dataModel!=nullptr)
    {
        struct WalkState
        {
            TreeItem *item;
            QStringList branchNames;
        };
        QList<WalkState> stack;

        const TreeItem *rootItem=dataModel->getRootItem();
        if(rootItem!=nullptr)
            for(int i=0; i<rootItem->childCount(); ++i)
            {
                TreeItem *child=rootItem->child(i);
                WalkState state;
                state.item=child;
                // Корень дерева безымянный, пустые имена пропускаются
                const QString childName=child->getField(QStringLiteral("name"));
                if(!childName.isEmpty())
                    state.branchNames << childName;
                stack << state;
            }

        while(!stack.isEmpty())
        {
            const WalkState state=stack.takeLast();
            TreeItem *item=state.item;

            if(item->getField(QStringLiteral("crypt"))!=QStringLiteral("1") ||
               globalParameters.getCryptKey().length()>0)
            {
                RecordTableData *recordTable=item->recordtableGetTableData();
                if(recordTable!=nullptr)
                    for(int row=0; row<static_cast<int>(recordTable->size()); ++row)
                    {
                        // Живая запись читается напрямую из таблицы
                        Record *record=recordTable->getRecord(row);
                        if(record==nullptr)
                            continue;

                        NoteInfo info;
                        info.name=recordTable->getField(QStringLiteral("name"), row);
                        info.branch=state.branchNames.join(QStringLiteral(" / "));
                        AttachTableData *attachTable=record->getAttachTablePointer();
                        info.attachCount=(attachTable!=nullptr) ? attachTable->size() : 0;
                        info.recordDir=mytetraConfig.get_tetradir()
                                        +QStringLiteral("/base/")
                                        +recordTable->getField(QStringLiteral("dir"), row);
                        noteInfo[recordTable->getField(QStringLiteral("id"), row)]=info;
                    }
            }

            for(int i=0; i<item->childCount(); ++i)
            {
                TreeItem *child=item->child(i);
                const QString childName=child->getField(QStringLiteral("name"));
                WalkState childState;
                childState.item=child;
                childState.branchNames=state.branchNames;
                if(!childName.isEmpty())
                    childState.branchNames << childName;
                stack << childState;
            }
        }
    }

    for(const VisitStat &visitStat : visitStats)
    {
        QString noteName;
        QString branchPath;
        int attachCount=0;
        QString recordDir;

        // Быстрый путь: карта из одного обхода. Нет в карте -
        // медленный точечный путь (удаленные отбрасываются,
        // шифрованные без пароля показываются заглушкой)
        auto mapIt=noteInfo.find(visitStat.id);
        if(mapIt!=noteInfo.end())
        {
            noteName=mapIt->name;
            branchPath=mapIt->branch;
            attachCount=mapIt->attachCount;
            recordDir=mapIt->recordDir;
        }
        else
        {
            QStringList recordPath;

            if(dataModel!=nullptr)
                recordPath=dataModel->getRecordPath(visitStat.id);

            if(dataModel==nullptr || recordPath.isEmpty())
                continue; // Заметка удалена, в истории не показывается

            TreeItem *branchItem=dataModel->getItem(recordPath);
            if(branchItem==nullptr)
                continue;

            // Шифрованная ветка без введенного пароля: содержимое недоступно
            if(branchItem->getField(QStringLiteral("crypt"))==QStringLiteral("1") &&
               globalParameters.getCryptKey().length()==0)
            {
                noteName=tr("(encrypted note)");
                branchPath=tr("(encrypted)");
            }
            else
                continue; // Живая незашифрованная обязана быть в карте
        }

        if(!filter.isEmpty() && !noteName.toLower().contains(filter))
            continue;

        const QStringList imageFiles=recordDir.isEmpty()
            ? QStringList()
            : ContentGallery::recordImageFiles(recordDir);
        const qint64 noteBytes=recordDir.isEmpty() ? 0 : recordDirSize(recordDir);

        const int row=historyTable->rowCount();
        historyTable->insertRow(row);

        QTableWidgetItem *noteItem=new QTableWidgetItem(noteName);
        noteItem->setData(Qt::UserRole, visitStat.id);
        noteItem->setToolTip(QDateTime::fromMSecsSinceEpoch(visitStat.lastMsecs)
                             .toString(Qt::DefaultLocaleShortDate)
                             +QStringLiteral("\n")+tr("Visits: %1").arg(visitStat.count));
        historyTable->setItem(row, 0, noteItem);

        historyTable->setItem(row, 1, new QTableWidgetItem(branchPath));

        HistorySortItem *lastItem=new HistorySortItem(formatVisitTime(visitStat.lastMsecs),
                                                      visitStat.lastMsecs);
        historyTable->setItem(row, 2, lastItem);

        HistorySortItem *visitsItem=new HistorySortItem(QString::number(visitStat.count),
                                                        visitStat.count);
        historyTable->setItem(row, 3, visitsItem);

        HistorySortItem *imagesItem=new HistorySortItem(QString::number(imageFiles.size()),
                                                        imageFiles.size());
        historyTable->setItem(row, 4, imagesItem);

        HistorySortItem *filesItem=new HistorySortItem(QString::number(attachCount),
                                                       attachCount);
        historyTable->setItem(row, 5, filesItem);

        HistorySortItem *sizeItem=new HistorySortItem(ContentGallery::formatFileSize(noteBytes),
                                                      noteBytes);
        historyTable->setItem(row, 6, sizeItem);
    }

    // По умолчанию сначала недавние. Дальше выбор запоминается
    // таблицей (индикатор показан) и переживает пересборки
    resizingProgrammatically=true;

    historyTable->resizeColumnsToContents();

    // Автоширина текстовых колонок ограничена потолком чтобы длинное
    // название не раздувало док: дальше только вручную
    const int digitWidth=historyTable->fontMetrics().horizontalAdvance('0');
    for(int column=0; column<2; ++column)
    {
        const int autoWidth=qMin(historyTable->columnWidth(column),
                                 digitWidth*40);
        historyTable->setColumnWidth(column, autoWidth);
    }

    // Ручные ширины переживают пересборку
    for(auto it=userColumnWidths.constBegin(); it!=userColumnWidths.constEnd(); ++it)
        historyTable->setColumnWidth(it.key(), it.value());

    resizingProgrammatically=false;

    if(!historyTable->horizontalHeader()->isSortIndicatorShown())
        historyTable->sortByColumn(2, Qt::DescendingOrder);

    historyTable->setSortingEnabled(true);
}
