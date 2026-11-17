#ifndef SYNCPREVIEWDIALOG_H
#define SYNCPREVIEWDIALOG_H

#include <functional>

#include <QDialog>
#include <QMap>

#include "libraries/sync/SyncPreview.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTabBar;
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

    SyncPreviewDialog(const SyncPreview::Preview &preview,
                      const SyncPreview::Decisions &initial,
                      const TextFetcher &ownText,
                      const TextFetcher &otherText,
                      QWidget *parent=nullptr);

    // Итоговые решения (после exec()==Accepted)
    SyncPreview::Decisions decisions(void) const;

    // Сохранить выбранные действия как умолчание (галочка "Запомнить")
    bool wantSaveDefaults(void) const;

private slots:

    void onTabChanged(int index);
    void onActionsMenuTriggered(void);
    void onWholeTreeToggled(bool checked);
    void onHelpClicked(void);
    void onItemSelectionChanged(void);
    void onItemActionChanged(int index);
    void onDuelMine(void);
    void onDuelTheirs(void);

private:

    // Вкладка: 0 = Я→сервер, 1 = Сервер→я, 2 = Обоюдные правки.
    // Обоюдные — только правки текста (как на мобиле)
    bool itemVisibleOnTab(const SyncPreview::Item &item, int tab) const;
    QString tabName(int tab) const;
    QString tabStats(int tab) const;

    void buildTabs(void);
    void buildActionsMenu(void);
    void buildItems(void);
    QComboBox *makeActionCombo(int index);
    void syncComboState(QComboBox *combo, const SyncPreview::Item &item);
    void refreshCombos(void);
    void applyFilters(void);
    void setDimmed(QTreeWidgetItem *node, bool dimmed);
    void refreshRowText(QTreeWidgetItem *row);
    void refreshBranchMarks(void);
    void collectSubtreeItems(QTreeWidgetItem *node, QList<int> &out) const;
    void refreshCounter(void);
    void refreshAll(void);
    void updateBranchPanel(QTreeWidgetItem *row);
    void updateDuel(const SyncPreview::Item *item);
    void refreshDuel(void);
    void showDiffFor(const SyncPreview::Item *item);
    void showHelp(void);

    static QString markerFor(const SyncPreview::Item &item);
    static QString directionFor(const SyncPreview::Item &item);

    SyncPreview::Preview m_preview;
    SyncPreview::Decisions m_decisions;
    TextFetcher m_ownText;
    TextFetcher m_otherText;

    // Защита от бития: id -> опасность (предвычислено в конструкторе)
    QMap<QString, SyncPreview::Danger> m_dangers;

    int m_tab=0;
    bool m_wholeTree=false;
    QString m_duelItemId;

    QLabel *m_statusLabel=nullptr;
    QCheckBox *m_saveDefaults=nullptr;
    QTabBar *m_tabs=nullptr;
    QToolButton *m_actionsButton=nullptr;
    QCheckBox *m_wholeTreeCheck=nullptr;
    QTreeWidget *m_itemsTree=nullptr;
    QLabel *m_branchLabel=nullptr;
    QWidget *m_duelRow=nullptr;
    QPushButton *m_mineButton=nullptr;
    QPushButton *m_theirsButton=nullptr;
    QLabel *m_duelHint=nullptr;
    QPlainTextEdit *m_diffView=nullptr;
};

#endif // SYNCPREVIEWDIALOG_H
