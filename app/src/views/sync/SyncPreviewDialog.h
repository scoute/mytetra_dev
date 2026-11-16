#ifndef SYNCPREVIEWDIALOG_H
#define SYNCPREVIEWDIALOG_H

#include <functional>

#include <QDialog>

#include "libraries/sync/SyncPreview.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QTabBar;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

// Дерево изменений синхронизации (концепция мобильной версии):
// вкладки-направления (Я→сервер / Сервер→я / Обоюдные правки),
// счетчик разобранного, дерево по веткам, панель ветки с diff,
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

private:

    // Вкладка: 0 = Я→сервер, 1 = Сервер→я, 2 = Обоюдные правки
    bool itemVisibleOnTab(const SyncPreview::Item &item, int tab) const;
    QString tabName(int tab) const;

    void buildTabs(void);
    void buildActionsMenu(void);
    void buildItems(void);
    void refreshCounter(void);
    void refreshRowText(QTreeWidgetItem *row);
    void updateBranchPanel(QTreeWidgetItem *row);
    void showDiffFor(const SyncPreview::Item *item);
    void showHelp(void);

    static QString markerFor(const SyncPreview::Item &item);
    static QString directionFor(const SyncPreview::Item &item);

    SyncPreview::Preview m_preview;
    SyncPreview::Decisions m_decisions;
    TextFetcher m_ownText;
    TextFetcher m_otherText;

    int m_tab=0;
    bool m_wholeTree=true;

    QLabel *m_statusLabel=nullptr;
    QCheckBox *m_saveDefaults=nullptr;
    QTabBar *m_tabs=nullptr;
    QToolButton *m_actionsButton=nullptr;
    QCheckBox *m_wholeTreeCheck=nullptr;
    QTreeWidget *m_itemsTree=nullptr;
    QLabel *m_branchLabel=nullptr;
    QPlainTextEdit *m_diffView=nullptr;
};

#endif // SYNCPREVIEWDIALOG_H
