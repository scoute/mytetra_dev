// Цель Windows — до ЛЮБЫХ инклудов: Qt-заголовки сами тянут windows.h,
// и поздний _WIN32_WINNT уже ничего не даёт (QueryFullProcessImageNameA
// из mcpPeerAlive() тогда не объявлена, MinGW32 падает)
#if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x0600)
#undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <QDialog>
#include <QMap>
#include <QString>
#include <QDir>
#include <QString>
#include <QDesktopWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPrintDialog>
#include <QPrinter>
#include <QCloseEvent>
#include <QMessageBox>
#include <QSplitter>
#include <QDockWidget>
#include <QStatusBar>
#include <QActionGroup>
#include <QColorDialog>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>

#ifdef Q_OS_WIN32
#include <windows.h>
#else
#include <signal.h>
#include <errno.h>
#endif
#include <QTime>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>


// Элемент таблицы с числовой сортировкой: по клику на заголовок
// QTableWidget сравнивает operator<, текст "156" < "20" строкой бы врал
class NumberTableItem : public QTableWidgetItem
{
public:
  explicit NumberTableItem(int value) : QTableWidgetItem(QString::number(value))
  {
    setData(Qt::UserRole, value);
  }

  bool operator<(const QTableWidgetItem &other) const override
  {
    return data(Qt::UserRole).toInt() < other.data(Qt::UserRole).toInt();
  }
};

#include "main.h"
#include "models/appConfig/AppConfig.h"
#include "MainWindow.h"
#include "views/printPreview/PrintPreview.h"
#include "views/appConfigWindow/AppConfigDialog.h"
#include "libraries/TrashMonitoring.h"
#include "views/tree/TreeScreen.h"
#include "views/telegram/TelegramImportDialog.h"
#include "views/record/MetaEditor.h"
#include "libraries/wyedit/EditorToolBarAssistant.h"
#include "libraries/wyedit/EditorConfig.h"
#include "views/recordTable/RecordTableScreen.h"
#include "views/historyPanel/HistoryPanel.h"
#include "views/favoritesPanel/FavoritesPanel.h"
#include "views/backlinksPanel/BacklinksPanel.h"
#include "libraries/BacklinkIndex.h"
#include "views/tagsPanel/TagsPanel.h"
#include "views/contentGallery/ContentGallery.h"
#include "models/tree/TreeItem.h"
#include "views/findInBaseScreen/FindScreen.h"
#include "models/tree/KnowTreeModel.h"
#include "libraries/GlobalParameters.h"
#include "views/consoleEmulator/CommandRunner.h"
#include "libraries/WalkHistory.h"
#include "libraries/ActionLogger.h"
#include "libraries/WindowSwitcher.h"
#include "views/actionLog/ActionLogScreen.h"
#include "views/databasesManagement/DatabasesManagementScreen.h"
#include "libraries/ShortcutManager.h"
#include "libraries/RandomInitter.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/wyedit/EditorShowTextDispatcher.h"
#include "libraries/FixedParameters.h"
#include "libraries/helpers/CssHelper.h"
#include "libraries/mcp/McpTools.h"
#include "libraries/Favorites.h"
#include "models/recordTable/Record.h"


extern AppConfig mytetraConfig;
extern FixedParameters fixedParameters;
extern TrashMonitoring trashMonitoring;
extern GlobalParameters globalParameters;
extern WalkHistory walkHistory;
extern ActionLogger actionLogger;
extern ShortcutManager shortcutManager;


// Слот, обрабатывающий внешние сообщения
// Используется для обработки ссобщений, приходящих от других экземпляров MyTetra
// Разбор аргумента MCP-команд: весь текст после первого пробела это
// компактный JSON (пробелы внутри допустимы, переводов строк нет)
#include "McpGuiBridge.h"


// Пересчитать баннер и блокировку по пирам:
// пиры есть — баннер виден и UI заблокирован, нет — всё снято.
// Данные сессии в мосту, здесь только рендер виджетов
void MainWindow::updateMcpBanner(void)
{
    if(mcpBannerBar==nullptr || mcpBanner==nullptr)
        return;

    if(!mcpBridge->hasPeers())
    {
        setMcpUiLock(false);
        mcpBannerBar->hide();

        // Дисконнект: история исчезает сразу и нигде не сохраняется
        mcpBridge->clearHistory();
        updateMcpHistoryDialog();

        if(mcpHistoryDialog!=nullptr)
            mcpHistoryDialog->close();
    }
    else
    {
        setMcpUiLock(true);
        mcpBanner->setText(mcpBridge->bannerText());
        mcpBannerBar->show();
    }
}


// Блокировка UI на время MCP-сессии поверх глобального read-only.
// Своё досохраняется до блокировки, иначе потеряется на навигации.
// MCP-записи смотрят сырой флаг и продолжают писать.
// Идемпотентна: повтор не дёргает сохранение и перераскладку
void MainWindow::setMcpUiLock(bool locked)
{
    if(mytetraConfig.isMcpUiLocked()==locked)
        return;

    if(locked)
        saveTextarea();

    mytetraConfig.setMcpUiLock(locked);
    applyReadOnly();
}


// Накопление истории действий MCP-сессии. Хвост обрезан до трёхсот:
// окну нужен обзор, а не архив. На диск ничего не пишется


// Накопление истории действий MCP-сессии. Хвост обрезан до трёхсот:
// окну нужен обзор, а не архив. На диск ничего не пишется


// Передать окну текущий список (с его фильтром)
void MainWindow::updateMcpHistoryDialog(void)
{
    if(mcpHistoryDialog!=nullptr)
        mcpHistoryDialog->setHistory(mcpBridge->history());
}


void MainWindow::onMcpSessionStarted(void)
{
    // P3: первая Hello-сессия от моста (было инлайн в mcpHello-ветке)
    updateMcpHistoryDialog();

    if(mcpHistoryDialog!=nullptr)
        mcpHistoryDialog->show();
}


// Синхронный ответ на mcp-команду: фасад над McpGuiBridge
// (дергает сокетный респондер из main)
QByteArray MainWindow::handleMcpMessage(const QString &message)
{
    return mcpBridge->handleMcpMessage(message);
}


// Таблица разбивки статистики: строится отдельно чтобы сортировку
// можно было проверить без модального окна
QTableWidget *MainWindow::buildStatsTableWidget(QWidget *parent)
{
    QList<BaseStatRow> rows;

    if(treeScreen!=nullptr && treeScreen->knowTreeModel!=nullptr)
        rows=treeScreen->knowTreeModel->getBaseStatRows();

    QTableWidget *table=new QTableWidget(parent);
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels(QStringList() << tr("Branch") << tr("Note") << tr("Internal links") << tr("Images") << tr("Files") << tr("Tags"));
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionsClickable(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setRowCount(rows.size());

    // Компактные строки в одну текстовую строку как в истории и метках:
    // дефолтная высота секции QTableWidget заметно выше шрифта
    const int rowHeight=table->fontMetrics().height()+2;
    table->verticalHeader()->setMinimumSectionSize(rowHeight);
    table->verticalHeader()->setDefaultSectionSize(rowHeight);

    for(int r=0; r<rows.size(); ++r)
    {
        const BaseStatRow &row=rows.at(r);

        QTableWidgetItem *branchItem=new QTableWidgetItem(row.branchPath);
        branchItem->setData(Qt::UserRole, row.recordId);
        table->setItem(r, 0, branchItem);
        table->setItem(r, 1, new QTableWidgetItem(row.noteName));
        table->setItem(r, 2, new NumberTableItem(row.links));
        table->setItem(r, 3, new NumberTableItem(row.images));
        table->setItem(r, 4, new NumberTableItem(row.files));
        table->setItem(r, 5, new NumberTableItem(row.tags));
    }

    table->resizeColumnsToContents();
    table->sortByColumn(2, Qt::DescendingOrder);

    // Первый клик по столбцу сортирует по убыванию (там обычно
    // интереснее крупные), повторный клик переключает порядок.
    // Ручной обработчик вместо автосортировки: встроенная всегда
    // начинает с возрастания. Порядок помнится свой, а не читается
    // с индикатора: встроенный слот срабатывает раньше нашего
    // и успевает индикатор перевернуть
    connect(table->horizontalHeader(), &QHeaderView::sectionClicked, table,
            [table, lastColumn=2, lastOrder=Qt::DescendingOrder](int logicalIndex) mutable
    {
      Qt::SortOrder order=Qt::DescendingOrder;

      if(logicalIndex==lastColumn && lastOrder==Qt::DescendingOrder)
        order=Qt::AscendingOrder;

      lastColumn=logicalIndex;
      lastOrder=order;

      table->sortByColumn(logicalIndex, order);
    });

    return table;
}


// Разбивка статистики по заметкам: кто сколько ссылок держит.
// Двойной клик прыгает к заметке
