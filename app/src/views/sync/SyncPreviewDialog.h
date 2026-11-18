#ifndef SYNCPREVIEWDIALOG_H
#define SYNCPREVIEWDIALOG_H

#include <functional>

#include <QDialog>
#include <QList>
#include <QMap>
#include <QPair>
#include <QPixmap>
#include <QSet>
#include <QStyledItemDelegate>

#include "libraries/sync/SyncPreview.h"

// Роли данных строки дерева для делегата-пилюли: имя рисуется
// в скруглённой пилюле, остальное (маркер/стрелка/галочки) —
// текстом рядом. Пилюля несёт цвет категории, текст имени — обычный
enum SyncPreviewRowRole
{
    MarkerTextRole = Qt::UserRole + 10,
    MarkerColorRole,
    ArrowTextRole,
    ArrowColorRole,
    NameTextRole,
    HasCheckRole,   // решено (зелёная ✓)
    HasWarnRole,    // опасность (красная ⚠)
    PillColorRole,  // базовый цвет пилюли (категория)
    DimmedRole,     // контекстный узел (всё серым)
    SubtreeDoneRole, // ветка разобрана целиком (рамка жирнее + ✓)
    BranchRole      // true — ветка-папка, false — заметка-лист
};

// Делегат нулевой колонки дерева: имя в пилюле, остальное текстом.
// Без частей (групповые заголовки) — рисует как обычно
class SyncPreviewItemDelegate : public QStyledItemDelegate
{
public:
    explicit SyncPreviewItemDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTabBar;
class QTextEdit;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

// Дерево изменений синхронизации (концепция мобильной версии):
// вкладки-направления с git-статистикой, счетчик разобранного,
// дерево по веткам с контекстом, панель ветки с поединком версий
// и diff, защита от бития (почти-пустое не затирает содержательное),
// помощь по обозначениям. Принцип: безопасное (создание) не требует
// решений, остальное — явный выбор пользователя.
//
// Кнопка "Применить" — согласие с выбранными действиями.
// Отмена — сессия прерывается, расхождения остаются.
class SyncPreviewDialog : public QDialog
{
    Q_OBJECT

public:

    // Текст "другой" версии элемента для diff (тянется по требованию:
    // для записей — с противоположной стороны). Пусто = недоступно
    using TextFetcher =
        std::function<QString(const SyncPreview::Item &)>;

    // Частичное выполнение из диалога: сессия режет план по id тем же
    // выполнением. true — всё чисто (строки уходят из дерева),
    // false — оставить строки, показать предупреждение
    using PartialSyncer =
        std::function<bool(const QMap<QString, SyncPreview::Action> &)>;

    SyncPreviewDialog(const SyncPreview::Preview &preview,
                      const SyncPreview::Decisions &initial,
                      const TextFetcher &ownText,
                      const TextFetcher &otherText,
                      QWidget *parent=nullptr);

    void setPartialSyncer(PartialSyncer syncer);

    // Итоговые решения (после exec()==Accepted).
    // Уже синкнутое частичным «Применить» помечается Skip —
    // applyDecisions его вычеркнет из финального плана, наборы
    // с финальным проходом не пересекаются
    SyncPreview::Decisions decisions(void) const;

    // Сохранить выбранные действия как умолчание (галочка "Запомнить")
    bool wantSaveDefaults(void) const;

private slots:

    void onTabChanged(int index);
    void onActionsMenuTriggered(void);
    void onJumpClicked(void);
    void onHelpClicked(void);
    void onApplyClicked(void);
    void onItemSelectionChanged(void);
    void onItemActionChanged(int index);
    void onRowButtonClicked(void);
    void onDuelMine(void);
    void onDuelTheirs(void);

private:

    // Вкладка: 0 = Я→сервер, 1 = Сервер→я, 2 = Обоюдные правки.
    // Обоюдные — только правки текста (как на мобиле)
    int sectionOf(const SyncPreview::Item &item) const;
    bool itemVisibleOnTab(const SyncPreview::Item &item, int tab) const;
    QString tabName(int tab) const;
    QList<QPair<QString, QColor>> tabStatSpans(int tab) const;
    QPixmap tabStatsIcon(int tab) const;
    void refreshTabs(void);

    void buildTabs(void);
    void buildActionsMenu(void);
    void buildItems(void);
    QComboBox *makeActionCombo(int index);
    void syncComboState(QComboBox *combo, const SyncPreview::Item &item);
    void refreshCombos(void);
    QWidget *makeActionWidget(int index);
    void syncWidgetState(QWidget *w, const SyncPreview::Item &item);
    void refreshWidgets(void);
    static SyncPreview::Action acceptFor(const SyncPreview::Item &item);
    static SyncPreview::Action rejectFor(const SyncPreview::Item &item);
    void applyFilters(void);
    void setDimmed(QTreeWidgetItem *node, bool dimmed);
    void refreshRowText(QTreeWidgetItem *row);
    void refreshBranchMarks(void);
    void collectSubtreeItems(QTreeWidgetItem *node, QList<int> &out) const;
    // Уже синкнуто частичным «Применить»: в дереве скрыто,
    // в счётчике — как разобранное, из финальных решений — в Skip
    bool isSynced(const QString &id) const;
    bool isDone(const SyncPreview::Item &item) const;
    // Гейт «Применить» (как canApply мобилы): активно при хоть одном
    // решённом несинкнутом; закрывает окно только когда всё разобрано
    bool canApply(void) const;
    bool allDecided(void) const;
    void refreshCounter(void);
    void refreshAll(void);
    void updateBranchPanel(QTreeWidgetItem *row);
    void updateDuel(const SyncPreview::Item *item);
    void refreshDuel(void);
    void showDiffFor(const SyncPreview::Item *item);
    void refreshDiffView(void);
    void showHelp(void);

    static QString markerFor(const SyncPreview::Item &item);
    static QString directionFor(const SyncPreview::Item &item);

    SyncPreview::Preview m_preview;
    SyncPreview::Decisions m_decisions;
    TextFetcher m_ownText;
    TextFetcher m_otherText;
    PartialSyncer m_partialSync;

    // Уже синкнутые частичным «Применить» (строк нет в дереве,
    // перестройка локальная, без запросов к серверу)
    QSet<QString> m_syncedIds;

    // Защита от бития: id -> опасность (предвычислено в конструкторе)
    QMap<QString, SyncPreview::Danger> m_dangers;

    int m_tab=0;
    bool m_wholeTree=false;
    QString m_duelItemId;

    QLabel *m_statusLabel=nullptr;
    QCheckBox *m_saveDefaults=nullptr;
    QTabBar *m_tabs=nullptr;
    QToolButton *m_actionsButton=nullptr;
    QTreeWidgetItem *m_sectionHeaders[3]={nullptr,nullptr,nullptr};
    QPushButton *m_jumpButton=nullptr;
    QPushButton *m_expandAllButton=nullptr;
    QPushButton *m_collapseAllButton=nullptr;
    QTreeWidget *m_itemsTree=nullptr;
    QLabel *m_branchLabel=nullptr;
    QString m_diffOwn;
    QString m_diffOther;
    QString m_diffHtml;
    int m_diffMode=2; // 0 наша, 1 ихняя, 2 diff
    QPushButton *m_diffOursButton=nullptr;
    QPushButton *m_diffTheirsButton=nullptr;
    QPushButton *m_diffDiffButton=nullptr;
    QPushButton *m_applyButton=nullptr;
    QWidget *m_duelRow=nullptr;
    QPushButton *m_mineButton=nullptr;
    QPushButton *m_theirsButton=nullptr;
    QLabel *m_duelHint=nullptr;
    QTextEdit *m_diffView=nullptr;
};

#endif // SYNCPREVIEWDIALOG_H
