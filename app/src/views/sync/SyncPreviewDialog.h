#ifndef SYNCPREVIEWDIALOG_H
#define SYNCPREVIEWDIALOG_H

#include <functional>

#include <QDialog>

#include "libraries/sync/SyncPreview.h"

class QCheckBox;
class QComboBox;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

// Окно предпросмотра синка: категории с действиями, список элементов
// с точечными действиями, diff по требованию. Принцип: безопасное
// (создание) не требует решений, остальное — явный выбор пользователя.
//
// Кнопка "Синхронизировать" — согласие с выбранными действиями.
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

    // Сохранить выбранные действия как умолчание (чекбокс внизу)
    bool wantSaveDefaults(void) const;

private slots:

    void onCategoryActionChanged(int index);
    void onItemSelectionChanged(void);
    void onItemActionChanged(int index);

private:

    void buildCategories(void);
    void buildItems(void);
    void updateItemAction(QTreeWidgetItem *item);
    void showDiffFor(QTreeWidgetItem *item);

    SyncPreview::Preview m_preview;
    SyncPreview::Decisions m_decisions;
    TextFetcher m_ownText;
    TextFetcher m_otherText;

    QComboBox *m_categoryActions[5];
    QTreeWidget *m_itemsTree;
    QPlainTextEdit *m_diffView;
    QCheckBox *m_saveDefaults;
};

#endif // SYNCPREVIEWDIALOG_H
