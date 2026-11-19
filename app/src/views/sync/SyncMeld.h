#ifndef SYNCMELD_H
#define SYNCMELD_H

#include <QAbstractItemModel>
#include <QHash>
#include <QStyledItemDelegate>
#include <QWidget>

#include "libraries/sync/SyncPreview.h"

// Meld-стиль окна синка (только десктоп): одна таблица, строка = один id.
// Слева моё имя, справа ихнее, в центре стрелки →/← (stage в нужную
// сторону) + diff для поединков. Философия стрелок (согласована):
// → = «сделать как у меня», ← = «сделать как у них».
// Удаления — надгробия: стрелка в сторону живой копии = воскрешение,
// в сторону надгробия = подтвердить удаление.
// Призрак на принимающей стороне показывает, что приедет/воскреснет.
// Виртуализация штатная (QTreeView + модель), тянет и 100k+ строк.
class SyncPreviewDialog;

// Цели стрелок для строки: false — стрелки нет (сторона пуста).
// Маппинг повторяет словарь текущего дерева (accept/reject/дуэли),
// только в терминах «чья версия побеждает».
bool meldUpTarget(const SyncPreview::Item &item,
                  SyncPreview::Action &target);
bool meldDownTarget(const SyncPreview::Item &item,
                    SyncPreview::Action &target);

// Поединок (стрелки ↑/↓ вместо ✓/✕): правки текста записей
// и переименования веток — как isDuel в дереве
bool isMeldDuel(const SyncPreview::Item &item);

// Строка модели: либо элемент предпросмотра (решаемая meld-строка),
// либо ветка-контекст (заголовок группы без решений — предка нет в плане).
// Ветки из плана — и то, и другое: своя meld-строка + дети под ней.
struct SyncMeldNode
{
    int itemIndex=-1; // индекс в preview.items, -1 — заголовок контекста
    QString id;       // id ветки (своё или предка)
    QString name;
    bool isBranch=true;
    SyncMeldNode *parent=nullptr;
    QList<SyncMeldNode *> children;
    int decided=0; // решено в поддереве (для галки готовности шапки)
    int total=0;   // всего решаемого в поддереве
};

// Роли модели сверх Qt::UserRole (индекс элемента)
enum SyncMeldRole
{
    MeldHeaderKeyRole = Qt::UserRole + 20,
    MeldHeaderDoneRole,
    MeldHeaderTotalRole,
};

// Плоская модель: по строке на элемент предпросмотра (порядок — тот же
// стабильный, что в дереве: предки раньше, дальше по имени).
// Фильтр по разделу (0/1/2, как вкладки) — в meld-режиме вкладки фильтруют,
// а не якорят. Синкнутое частичным «Применить» в строки не входит.
class SyncMeldModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit SyncMeldModel(SyncPreviewDialog *dialog,
                           QObject *parent = nullptr);
    ~SyncMeldModel(void) override;

    // -1 — все разделы
    void setSection(int section);
    int section(void) const;

    void rebuild(void);
    // Перерисовать одну строку предпросмотра (решения живут в диалоге,
    // модель их только читает — сигнал дешёвый, красит только видимое)
    void refreshPreviewRow(int itemIndex);
    void refreshAllRows(void);
    // Пересчитать decided/total по поддеревьям (галка готовности шапок)
    void updateSubtreeMarks(void);

    // Строка модели -> индекс в preview.items
    int itemIndex(const QModelIndex &index) const;
    QModelIndex indexForItem(int itemIndex, int column=0) const;
    // Все решаемые id под узлом (включая его самого)
    void collectSubtreeItems(const QModelIndex &node,
                             QList<int> &out) const;
    // Ключ заголовка (id ветки) для панели ветки
    QString headerKey(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent=QModelIndex()) const override;
    int columnCount(const QModelIndex &parent=QModelIndex()) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent=QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QVariant data(const QModelIndex &index,
                  int role=Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role=Qt::DisplayRole) const override;

private:
    SyncPreviewDialog *m_dialog;
    QList<SyncMeldNode *> m_roots;
    QMap<QString, SyncMeldNode *> m_byId;
    QHash<int, SyncMeldNode *> m_byItem;
    int m_section=-1;

    void clearNodes(void);
    SyncMeldNode *nodeFor(const QModelIndex &index) const;
    QModelIndex indexForNode(SyncMeldNode *node, int column) const;
    void collectSubtreeNodes(SyncMeldNode *node,
                            QList<int> &out) const;
    int computeMarks(SyncMeldNode *node);
};

// Делегат meld-таблицы: пилюли по сторонам (как в дереве: цвет категории,
// иконка ветки/листа, надгробие, призрак, «—» пустой стороны),
// в центре — стрелки →/← + diff-ссылка. Stateless: состояние читает
// из диалога в момент отрисовки. Клики — как в дереве: регионы глифа,
// селекцию НЕ двигаем (иначе каждый тап дёргал бы diff с хаба).
class SyncMeldDelegate : public QStyledItemDelegate
{
public:
    explicit SyncMeldDelegate(SyncPreviewDialog *dialog,
                              QAbstractItemView *view);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;
    // Хит-тест центра: 0 — мимо кнопок, 1 — «→», 2 — «←», 3 — diff
    int hitTest(const QModelIndex &index,
                const QPoint &viewportPos) const;

private:
    // Регионы внутри ячейки центра. false — строка без действий
    // (таких нет: diff есть только у поединков, стрелки — по наличию цели)
    bool cellLayout(const QStyleOptionViewItem &option,
                    const QModelIndex &index,
                    QRect &up, QRect &down, QRect &diff) const;

    SyncPreviewDialog *m_dialog;
    QAbstractItemView *m_view;
    mutable QPersistentModelIndex m_hover;
    mutable int m_hoverPart=0;

    friend class SyncPreviewDialog;
};

// Бублик прогресса разбора (как в мокапе): красная дуга — остаток,
// зелёная — разобрано, в центре «M/N». Клик — к первому нерешенному.
class SyncMeldRing : public QWidget
{
    Q_OBJECT

public:
    explicit SyncMeldRing(SyncPreviewDialog *dialog,
                          QWidget *parent = nullptr);

    void setProgress(int done, int total);
    QSize sizeHint(void) const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    SyncPreviewDialog *m_dialog;
    int m_done=0;
    int m_total=0;
};

#endif // SYNCMELD_H
