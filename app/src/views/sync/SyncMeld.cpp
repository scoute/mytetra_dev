#include "views/sync/SyncMeld.h"

#include <functional>

#include <QAbstractItemView>
#include <QApplication>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QtMath>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QToolTip>
#include <QTreeView>

#include "views/sync/SyncPreviewDialog.h"

using SyncPreview::Action;
using SyncPreview::Category;
using SyncPreview::Danger;
using SyncPreview::Item;

namespace
{

bool isLightTheme(void)
{
    return QApplication::palette().color(QPalette::Window).lightness() > 128;
}

uint meldCategoryColor(Category category)
{
    switch(category)
    {
    case Category::Create:      return 0xFF66BB6A;
    case Category::Rename:      return 0xFF42A5F6;
    case Category::Edit:        return 0xFFFFB74D;
    case Category::MyDelete:    return 0xFFEF5350;
    case Category::TheirDelete: return 0xFF8E244D;
    }

    return 0xFFB0B0B0;
}

QString meldMarker(const Item &item)
{
    switch(item.category)
    {
    case Category::Create: return QStringLiteral("+");
    case Category::Rename: return QStringLiteral("≡");
    case Category::Edit:   return QStringLiteral("~");
    default:               return QStringLiteral("−");
    }
}

// Сторона существует: создание — только с одной стороны,
// остальное — с обеих (удаления — надгробием на своей)
bool meldLeftExists(const Item &item)
{
    return !(item.category==Category::Create && !item.fromLocal);
}

bool meldRightExists(const Item &item)
{
    return !(item.category==Category::Create && item.fromLocal);
}

bool meldLeftKilled(const Item &item)
{
    return item.category==Category::MyDelete;
}

bool meldRightKilled(const Item &item)
{
    return item.category==Category::TheirDelete;
}

QString meldLeftName(const SyncPreviewDialog *dialog, const Item &item)
{
    Q_UNUSED(dialog);
    if(!meldLeftExists(item))
        return QString();

    if(item.category==Category::Rename && !item.ownName.isEmpty())
        return item.ownName;

    return item.name;
}

QString meldRightName(const SyncPreviewDialog *dialog, const Item &item)
{
    Q_UNUSED(dialog);
    if(!meldRightExists(item))
        return QString();

    if(item.category==Category::Rename && !item.otherName.isEmpty())
        return item.otherName;

    return item.name;
}

// Имя, которое встанет слева, если побеждают «они» (переименование):
// для остального имя одно с обеих сторон
QString meldLeftWinName(const Item &item)
{
    if(item.category==Category::Rename && !item.otherName.isEmpty())
        return item.otherName;

    return item.name;
}

// Имя, которое встанет справа, если побеждаю «я»
QString meldRightWinName(const Item &item)
{
    if(item.category==Category::Rename && !item.ownName.isEmpty())
        return item.ownName;

    return item.name;
}

} // namespace


bool isMeldDuel(const Item &item)
{
    return (item.category==Category::Edit && !item.isBranch)
        || (item.category==Category::Rename && item.isBranch);
}


bool meldUpTarget(const Item &item, Action &target)
{
    // → = «сделать как у меня»: моё состояние побеждает
    switch(item.category)
    {
    case Category::Create:
        if(!item.fromLocal)
            return false;
        target=Action::Apply;
        return true;
    case Category::Edit:
        target=Action::ForceMine;
        return true;
    case Category::Rename:
        target=item.isBranch ? Action::ForceMine
                            : (item.fromLocal ? Action::Apply
                                              : Action::ForceMine);
        return true;
    case Category::MyDelete:
        // моё удаление уезжает на сервер
        target=Action::ForceMine;
        return true;
    case Category::TheirDelete:
        // моя живая копия воскресает наверх
        target=Action::ForceMine;
        return true;
    }

    return false;
}


bool meldDownTarget(const Item &item, Action &target)
{
    // ← = «сделать как у них»: их состояние побеждает
    switch(item.category)
    {
    case Category::Create:
        if(item.fromLocal)
            return false;
        target=Action::Apply;
        return true;
    case Category::Edit:
        target=Action::TakeServer;
        return true;
    case Category::Rename:
        target=item.isBranch ? Action::TakeServer
                            : (item.fromLocal ? Action::TakeServer
                                              : Action::Apply);
        return true;
    case Category::MyDelete:
        // воскрешение моей копии с сервера
        target=Action::TakeServer;
        return true;
    case Category::TheirDelete:
        // принимаю их удаление
        target=Action::TakeServer;
        return true;
    }

    return false;
}


// --- SyncMeldModel ---

SyncMeldModel::SyncMeldModel(SyncPreviewDialog *dialog, QObject *parent)
    : QAbstractItemModel(parent),
      m_dialog(dialog)
{
}


SyncMeldModel::~SyncMeldModel(void)
{
    clearNodes();
}


void SyncMeldModel::clearNodes(void)
{
    // Рекурсивное удаление дерева
    std::function<void(SyncMeldNode *)> drop=[&](SyncMeldNode *node) {
        for(SyncMeldNode *child : node->children)
            drop(child);
        delete node;
    };

    for(SyncMeldNode *root : m_roots)
        drop(root);

    m_roots.clear();
    m_byId.clear();
    m_byItem.clear();
}


void SyncMeldModel::setSection(int section)
{
    if(m_section==section)
        return;

    m_section=section;
    rebuild();
}


int SyncMeldModel::section(void) const
{
    return m_section;
}


void SyncMeldModel::rebuild(void)
{
    beginResetModel();
    clearNodes();

    if(m_dialog!=nullptr)
    {
        const SyncPreview::Preview &preview=m_dialog->meldPreviewData();

        // Тот же стабильный порядок, что в дереве: предки раньше
        // (меньше глубина), дальше по имени — дети строго под своей веткой
        QList<int> order;

        for(int i=0; i<preview.items.size(); ++i)
        {
            const Item &item=preview.items.at(i);

            if(m_dialog->meldItemSynced(item.id))
                continue;

            if(m_section>=0 && m_dialog->meldItemSection(item)!=m_section)
                continue;

            order << i;
        }

        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            const Item &ia=preview.items.at(a), &ib=preview.items.at(b);

            if(ia.pathIds.size()!=ib.pathIds.size())
                return ia.pathIds.size() < ib.pathIds.size();

            return ia.name < ib.name;
        });

        // Узлы веток (свои + контекст-предки внутри своей секции)
        auto contextNode=[&](SyncMeldNode *parent, const QString &id,
                             const QString &name) -> SyncMeldNode * {
            if(SyncMeldNode *found=m_byId.value(id, nullptr))
                return found;

            SyncMeldNode *node=new SyncMeldNode();
            node->id=id;
            node->name=name.isEmpty() ? QStringLiteral("(ветка)") : name;
            node->isBranch=true;
            node->parent=parent;

            if(parent!=nullptr)
                parent->children << node;
            else
                m_roots << node;

            m_byId.insert(id, node);
            return node;
        };

        for(int index : order)
        {
            const Item &item=preview.items.at(index);
            SyncMeldNode *parent=nullptr;

            for(int level=0; level<item.pathIds.size(); ++level)
            {
                parent=contextNode(
                    parent,
                    item.pathIds.at(level),
                    level < item.path.size() ? item.path.at(level)
                                             : QString());
            }

            SyncMeldNode *node=new SyncMeldNode();
            node->itemIndex=index;
            node->id=item.id;
            node->name=item.name;
            node->isBranch=item.isBranch;
            node->parent=parent;

            if(parent!=nullptr)
                parent->children << node;
            else
                m_roots << node;

            // Ветка из плана адресуется по id: потомки встанут под неё,
            // а не под дублирующий контекст
            if(item.isBranch)
                m_byId.insert(item.id, node);

            m_byItem.insert(index, node);
        }

        updateSubtreeMarks();
    }

    endResetModel();
}


void SyncMeldModel::refreshPreviewRow(int itemIndex)
{
    SyncMeldNode *node=m_byItem.value(itemIndex, nullptr);

    if(node==nullptr)
        return;

    const QModelIndex top=indexForNode(node, 0);
    const QModelIndex bottom=indexForNode(node, 2);

    if(top.isValid() && bottom.isValid())
        emit dataChanged(top, bottom);
}


void SyncMeldModel::refreshAllRows(void)
{
    updateSubtreeMarks();

    if(m_roots.isEmpty())
        return;

    emit dataChanged(indexForNode(m_roots.first(), 0),
                     indexForNode(m_roots.last(), 2));
}


int SyncMeldModel::computeMarks(SyncMeldNode *node)
{
    if(node==nullptr)
        return 0;

    int total=0;
    int done=0;

    if(node->itemIndex>=0 && m_dialog!=nullptr)
    {
        const Item *item=m_dialog->meldPreviewItem(node->itemIndex);

        if(item!=nullptr)
        {
            total=1;
            done=(m_dialog->meldEffectiveDecision(*item)!=Action::Confirm)
                ? 1 : 0;
        }
    }

    for(SyncMeldNode *child : node->children)
    {
        computeMarks(child);
        total+=child->total;
        done+=child->decided;
    }

    node->total=total;
    node->decided=done;
    return total;
}


void SyncMeldModel::updateSubtreeMarks(void)
{
    for(SyncMeldNode *root : m_roots)
        computeMarks(root);
}


SyncMeldNode *SyncMeldModel::nodeFor(const QModelIndex &index) const
{
    if(!index.isValid())
        return nullptr;

    return static_cast<SyncMeldNode *>(index.internalPointer());
}


QModelIndex SyncMeldModel::indexForNode(SyncMeldNode *node,
                                       int column) const
{
    if(node==nullptr)
        return QModelIndex();

    int row=0;

    if(node->parent!=nullptr)
        row=node->parent->children.indexOf(node);
    else
        row=m_roots.indexOf(node);

    if(row<0)
        return QModelIndex();

    return createIndex(row, column, node);
}


int SyncMeldModel::itemIndex(const QModelIndex &index) const
{
    SyncMeldNode *node=nodeFor(index);

    if(node==nullptr)
        return -1;

    return node->itemIndex;
}


QModelIndex SyncMeldModel::indexForItem(int itemIndex, int column) const
{
    return indexForNode(m_byItem.value(itemIndex, nullptr), column);
}


void SyncMeldModel::collectSubtreeNodes(SyncMeldNode *node,
                                       QList<int> &out) const
{
    if(node==nullptr)
        return;

    if(node->itemIndex>=0)
        out << node->itemIndex;

    for(SyncMeldNode *child : node->children)
        collectSubtreeNodes(child, out);
}


void SyncMeldModel::collectSubtreeItems(const QModelIndex &node,
                                       QList<int> &out) const
{
    out.clear();
    collectSubtreeNodes(nodeFor(node), out);
}


QString SyncMeldModel::headerKey(const QModelIndex &index) const
{
    SyncMeldNode *node=nodeFor(index);

    if(node==nullptr)
        return QString();

    return node->id;
}


int SyncMeldModel::rowCount(const QModelIndex &parent) const
{
    if(!parent.isValid())
        return m_roots.size();

    SyncMeldNode *node=nodeFor(parent);

    if(node==nullptr)
        return 0;

    return node->children.size();
}


int SyncMeldModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 3;
}


QModelIndex SyncMeldModel::index(int row, int column,
                                 const QModelIndex &parent) const
{
    if(column<0 || column>=3 || row<0)
        return QModelIndex();

    if(!parent.isValid())
    {
        if(row>=m_roots.size())
            return QModelIndex();

        return createIndex(row, column, m_roots.at(row));
    }

    SyncMeldNode *node=nodeFor(parent);

    if(node==nullptr || row>=node->children.size())
        return QModelIndex();

    return createIndex(row, column, node->children.at(row));
}


QModelIndex SyncMeldModel::parent(const QModelIndex &child) const
{
    SyncMeldNode *node=nodeFor(child);

    if(node==nullptr || node->parent==nullptr)
        return QModelIndex();

    return indexForNode(node->parent, 0);
}


QVariant SyncMeldModel::data(const QModelIndex &index, int role) const
{
    SyncMeldNode *node=nodeFor(index);

    if(node==nullptr)
        return QVariant();

    if(role==Qt::UserRole)
        return node->itemIndex;

    if(role==MeldHeaderKeyRole)
        return node->id;

    if(role==MeldHeaderDoneRole)
        return node->decided;

    if(role==MeldHeaderTotalRole)
        return node->total;

    if(role==Qt::DisplayRole)
    {
        if(node->itemIndex>=0)
        {
            const Item *item=(m_dialog!=nullptr)
                ? m_dialog->meldPreviewItem(node->itemIndex) : nullptr;

            if(item==nullptr)
                return QVariant();

            if(index.column()==0)
                return meldLeftName(m_dialog, *item);

            if(index.column()==2)
                return meldRightName(m_dialog, *item);

            return QVariant();
        }

        // Заголовок контекста: только имя в нулевой колонке
        if(index.column()==0)
            return node->name;

        return QVariant();
    }

    if(role==Qt::UserRole+10) // BranchRole (зеркало дерева)
        return node->isBranch;

    return QVariant();
}


QVariant SyncMeldModel::headerData(int section, Qt::Orientation orientation,
                                  int role) const
{
    if(orientation!=Qt::Horizontal || role!=Qt::DisplayRole)
        return QVariant();

    if(section==0)
        return tr("Я · это устройство");

    if(section==1)
        return tr("копировать");

    if(section==2)
        return tr("Сервер");

    return QVariant();
}


// --- SyncMeldDelegate ---

SyncMeldDelegate::SyncMeldDelegate(SyncPreviewDialog *dialog,
                                   QAbstractItemView *view)
    : QStyledItemDelegate(view),
      m_dialog(dialog),
      m_view(view)
{
}


namespace
{

struct MeldCellState
{
    const Item *item=nullptr;
    // Вид стороны после «Применить» (таблица — превью исхода,
    // а не текущего состояния): Missing — «—», Tomb — надгробие,
    // Live — обычная пилюля, Fresh — твёрдая зелёная (приедет/воскреснет)
    enum class SideLook { Missing, Tomb, Live, Fresh };
    SideLook leftLook=SideLook::Missing;
    SideLook rightLook=SideLook::Missing;
    QString leftName;
    QString rightName;
    // Подсказка в состоянии ДО (стрелки не нажаты): серый пунктир
    // «сюда может приехать по стрелке». Пусто — обычное «—»
    QString leftHint;
    QString rightHint;
    bool upOn=false;
    bool downOn=false;
    bool upEnabled=false;
    bool downEnabled=false;
    bool duel=false;
    bool danger=false;
    int depth=0;
};

MeldCellState meldCellState(SyncPreviewDialog *dialog, int itemIndex)
{
    MeldCellState st;

    if(dialog==nullptr)
        return st;

    const Item *itemPtr=dialog->meldPreviewItem(itemIndex);

    if(itemPtr==nullptr)
        return st;

    const Item &item=*itemPtr;
    st.item=&item;
    st.depth=item.pathIds.size();
    st.duel=isMeldDuel(item);
    st.danger=dialog->meldItemDanger(item.id);

    Action upAction=Action::Confirm, downAction=Action::Confirm;
    st.upEnabled=meldUpTarget(item, upAction);
    st.downEnabled=meldDownTarget(item, downAction);

    const Action effective=dialog->meldEffectiveDecision(item);

    if(effective!=Action::Confirm)
    {
        st.upOn=st.upEnabled && effective==upAction;
        st.downOn=st.downEnabled && effective==downAction;
    }

    const bool myWins=st.upOn;
    const bool theirWins=st.downOn;
    const bool liveL=meldLeftExists(item) && !meldLeftKilled(item);
    const bool liveR=meldRightExists(item) && !meldRightKilled(item);

    // Подсказки состояния ДО: пустая сторона + живая стрелка в неё —
    // серый пунктир «сюда может приехать». Умирающим сторонам (staged)
    // подсказок нет — там будет честное «—»
    if(!theirWins && st.downEnabled && !meldLeftExists(item))
        st.leftHint=meldRightName(dialog, item);

    if(!myWins && st.upEnabled && !meldRightExists(item))
        st.rightHint=meldLeftName(dialog, item);

    // Левая сторона после «Применить»
    if(theirWins && item.category==Category::TheirDelete && liveL)
        st.leftLook=MeldCellState::SideLook::Missing; // приму удаление
    else if(!meldLeftExists(item) && !theirWins)
        st.leftLook=MeldCellState::SideLook::Missing;
    else if(theirWins && !liveL)
    {
        // сюда приедет/воскреснет: имя — с правой (живой) стороны
        const QString incoming=meldRightName(dialog, item);
        if(incoming.isEmpty())
            st.leftLook=MeldCellState::SideLook::Missing;
        else
        {
            st.leftLook=MeldCellState::SideLook::Fresh;
            st.leftName=incoming;
        }
    }
    else if(meldLeftKilled(item))
    {
        st.leftLook=MeldCellState::SideLook::Tomb;
        st.leftName=meldLeftName(dialog, item);
    }
    else
    {
        st.leftLook=MeldCellState::SideLook::Live;
        st.leftName=theirWins ? meldLeftWinName(item)
                              : meldLeftName(dialog, item);
    }

    // Правая сторона после «Применить» — зеркально
    if(myWins && item.category==Category::MyDelete && liveR)
        st.rightLook=MeldCellState::SideLook::Missing; // удаление уедет
    else if(!meldRightExists(item) && !myWins)
        st.rightLook=MeldCellState::SideLook::Missing;
    else if(myWins && !liveR)
    {
        const QString incoming=meldLeftName(dialog, item);
        if(incoming.isEmpty())
            st.rightLook=MeldCellState::SideLook::Missing;
        else
        {
            st.rightLook=MeldCellState::SideLook::Fresh;
            st.rightName=incoming;
        }
    }
    else if(meldRightKilled(item))
    {
        st.rightLook=MeldCellState::SideLook::Tomb;
        st.rightName=meldRightName(dialog, item);
    }
    else
    {
        st.rightLook=MeldCellState::SideLook::Live;
        st.rightName=myWins ? meldRightWinName(item)
                            : meldRightName(dialog, item);
    }

    return st;
}

// Пилюля имени: язык дерева (цвет категории, иконка ветки/листа).
// Состояния: обычная (цвет категории), надгробие (серая пунктирная,
// зачёркнута), свежая (твёрдая зелёная — здесь будет после «Применить»).
void paintMeldPill(QPainter *painter, const QFont &font, const QRect &cell,
                   const QString &marker, const QColor &markerColor,
                   const QString &name, const QColor &pillBase,
                   bool isBranch, bool tomb,
                   bool warn, bool selected,
                   bool hugRight, int indent, bool hint=false)
{
    QFontMetrics fm(font);
    const int rowH=cell.height();
    const int cy=cell.top()+rowH/2;
    const QColor gray(0x9E9E9E);

    // Маркер категории — текстом перед пилюлей
    const int right=cell.right()-4;

    if(hugRight)
    {
        // правая граница пилюли: у центра минус отступ глубины
        const int pillRightEdge=right-indent;
        // сначала меряем пилюлю, потом рисуем справа налево
        const QString shown=fm.elidedText(
            name.isEmpty() ? QStringLiteral("…") : name,
            Qt::ElideRight, qMax(20, cell.width()-indent-60));
        const int iconW=fm.height()*4/5;
        const int pillW=fm.horizontalAdvance(shown)+iconW+6+16;
        const int pillH=qMin(rowH-2, fm.height()+4);
        const qreal topEdge=qRound(cy-pillH/2.0);
        const QRectF pillRect(pillRightEdge-pillW, topEdge, pillW, pillH);
        const qreal pillRadius=pillH/2;

        QColor fill=tomb ? QColor(0xEEEEEE) : pillBase;
        if(!tomb)
            fill.setAlpha(selected ? 255 : (hint ? 30 : 90));

        // Прозрачная заливка именно цветом, а не NoBrush
        if(hint)
            fill=QColor(0, 0, 0, 0);

        painter->setBrush(fill);
        QPen pen(tomb || hint ? gray : pillBase, 1);
        pen.setWidth(0);
        if(tomb || hint)
            pen.setStyle(Qt::DashLine);
        if(!tomb && !hint && isBranch && !selected)
            pen=QPen(QColor(Qt::black), 1);
        painter->setPen(pen);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->drawRoundedRect(pillRect, pillRadius, pillRadius);

        // иконка ветки/листа — рисованная, как в дереве
        const int iconH=pillH-8;
        const int iconX=pillRect.left()+6;
        const int iconY=cy-iconH/2;
        if(isBranch)
        {
            const int tabW=iconW*45/100, tabH=iconH*32/100;
            painter->setBrush(QColor(0xFFC107));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(QRectF(iconX, iconY, tabW, tabH+2), 2, 2);
            painter->drawRoundedRect(QRectF(iconX, iconY+tabH*0.5,
                iconW, iconH-tabH*0.5), 2, 2);
        }
        else
        {
            const int f=iconW*30/100;
            QPolygonF page;
            page << QPointF(iconX, iconY)
                 << QPointF(iconX+iconW-f, iconY)
                 << QPointF(iconX+iconW, iconY+f)
                 << QPointF(iconX+iconW, iconY+iconH)
                 << QPointF(iconX, iconY+iconH);
            painter->setBrush(QColor(Qt::white));
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(page);
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(QColor(0x9E9E9E), 1));
            painter->drawPolygon(page);
        }
        painter->setRenderHint(QPainter::Antialiasing, false);

        QFont nameFont=font;
        nameFont.setStrikeOut(tomb);
        nameFont.setBold(isBranch);
        painter->setFont(nameFont);
        painter->setPen(selected ? QColor(Qt::white)
                                : (tomb ? gray : QColor(0x212121)));
        painter->drawText(QRect(pillRect.left()+6+iconW+6, pillRect.top(),
            pillW-6-iconW-6-8, pillH),
            Qt::AlignLeft | Qt::AlignVCenter, shown);

        // маркер слева от пилюли
        QFont markerFont=font;
        markerFont.setBold(true);
        painter->setFont(markerFont);
        painter->setPen(markerColor);
        painter->drawText(QRect(pillRect.left()-24, pillRect.top(),
            22, pillH), Qt::AlignRight | Qt::AlignVCenter, marker);

        if(warn)
        {
            painter->setFont(font);
            painter->setPen(QColor(0xC00000));
            painter->drawText(QRect(pillRightEdge+2, pillRect.top(),
                20, pillH), Qt::AlignLeft | Qt::AlignVCenter,
                QStringLiteral("⚠"));
        }
        return;
    }

    // hugLeft (правая колонка): маркер + пилюля от левого края + indent
    int x=cell.left()+4;
    x+=indent;
    QFont markerFont=font;
    markerFont.setBold(true);
    painter->setFont(markerFont);
    painter->setPen(markerColor);
    const int markerW=fm.horizontalAdvance(marker)+6;
    painter->drawText(x, cy-fm.height()/2, markerW, fm.height(),
                      Qt::AlignLeft | Qt::AlignVCenter, marker);
    x+=markerW;

    const QString shown=fm.elidedText(
        name.isEmpty() ? QStringLiteral("…") : name,
        Qt::ElideRight, qMax(20, right-x-24));
    const int iconW=fm.height()*4/5;
    const int pillW=fm.horizontalAdvance(shown)+iconW+6+16;
    const int pillH=qMin(rowH-2, fm.height()+4);
    const qreal topEdge=qRound(cy-pillH/2.0);
    const QRectF pillRect(x, topEdge,
                          qMin<qreal>(pillW, right-x), topEdge+pillH-topEdge);
    const qreal pillRadius=(qRound(cy+pillH/2.0)-topEdge)/2;

    QColor fill=tomb ? QColor(0xEEEEEE) : pillBase;
    if(!tomb)
        fill.setAlpha(selected ? 255 : (hint ? 30 : 90));

    if(hint)
        fill=QColor(0, 0, 0, 0);

    painter->setBrush(fill);
    QPen pen(tomb || hint ? gray : pillBase, 1);
    pen.setWidth(0);
    if(tomb || hint)
        pen.setStyle(Qt::DashLine);
    if(!tomb && !hint && isBranch && !selected)
        pen=QPen(QColor(Qt::black), 1);
    painter->setPen(pen);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->drawRoundedRect(pillRect, pillRadius, pillRadius);

    const int iconH=pillH-8;
    const int iconX=pillRect.left()+6;
    const int iconY=cy-iconH/2;
    if(isBranch)
    {
        const int tabW=iconW*45/100, tabH=iconH*32/100;
        painter->setBrush(QColor(0xFFC107));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(QRectF(iconX, iconY, tabW, tabH+2), 2, 2);
        painter->drawRoundedRect(QRectF(iconX, iconY+tabH*0.5,
            iconW, iconH-tabH*0.5), 2, 2);
    }
    else
    {
        const int f=iconW*30/100;
        QPolygonF page;
        page << QPointF(iconX, iconY)
             << QPointF(iconX+iconW-f, iconY)
             << QPointF(iconX+iconW, iconY+f)
             << QPointF(iconX+iconW, iconY+iconH)
             << QPointF(iconX, iconY+iconH);
        painter->setBrush(QColor(Qt::white));
        painter->setPen(Qt::NoPen);
        painter->drawPolygon(page);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(QColor(0x9E9E9E), 1));
        painter->drawPolygon(page);
    }
    painter->setRenderHint(QPainter::Antialiasing, false);

    QFont nameFont=font;
    nameFont.setStrikeOut(tomb);
    nameFont.setBold(isBranch);
    painter->setFont(nameFont);
    painter->setPen(selected ? QColor(Qt::white)
                            : (tomb ? gray : QColor(0x212121)));
    painter->drawText(QRect(pillRect.left()+6+iconW+6, pillRect.top(),
        pillRect.width()-6-iconW-6-8, pillH),
        Qt::AlignLeft | Qt::AlignVCenter, shown);

    if(warn)
    {
        painter->setFont(font);
        painter->setPen(QColor(0xC00000));
        painter->drawText(QRect(pillRect.right()+2, pillRect.top(),
            20, pillH), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("⚠"));
    }

    Q_UNUSED(right);
}

} // namespace


void SyncMeldDelegate::paint(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    const int itemIdx=(m_dialog!=nullptr)
        ? m_dialog->m_meldModel->itemIndex(index) : -1;
    Q_UNUSED(itemIdx);

    // Фон, фокус и выделение — штатно, без текста
    QStyleOptionViewItem o(option);
    initStyleOption(&o, index);
    o.text.clear();

    QStyle *style=o.widget!=nullptr ? o.widget->style()
                                    : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &o, painter, o.widget);

    if(m_dialog==nullptr)
        return;

    // Важно: работаем именно с этим индексом (с родителем),
    // а не со строкой верхнего уровня — иначе дети рисуются
    // чужими данными
    const int previewIdx=m_dialog->m_meldModel->itemIndex(index);

    // Заголовок группы (ветка-контекст): имя + счётчик разбора.
    // Стрелок нет — bulk делается меню/кнопками «всё поддерево»
    if(previewIdx<0)
    {
        if(index.column()!=0)
            return;

        const QFontMetrics fm(o.font);
        const QString name=index.data(Qt::DisplayRole).toString();
        const int done=index.data(MeldHeaderDoneRole).toInt();
        const int total=index.data(MeldHeaderTotalRole).toInt();

        QFont f=o.font;
        f.setBold(true);
        painter->setFont(f);
        painter->setPen(o.palette.color(QPalette::Disabled,
                                        QPalette::Text));

        QString text=name;

        if(total>0)
            text+=QStringLiteral(" · %1/%2").arg(done).arg(total);

        painter->drawText(o.rect.adjusted(4, 0, -4, 0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          fm.elidedText(text, Qt::ElideRight,
                                       o.rect.width()-8));

        if(total>0 && done==total)
        {
            painter->setPen(QColor(0x66BB6A));
            painter->drawText(o.rect.adjusted(4, 0, -4, 0),
                              Qt::AlignRight | Qt::AlignVCenter,
                              QStringLiteral("✓"));
        }

        return;
    }

    if(previewIdx>=m_dialog->meldPreviewData().items.size())
        return;

    const Item &item=m_dialog->m_preview.items.at(previewIdx);
    const MeldCellState st=meldCellState(m_dialog, previewIdx);
    const bool selected=(o.state & QStyle::State_Selected)!=0;

    // Стороны: 0 — своя справа у центра, 2 — ихняя слева у центра.
    // Иерархия видна штатно (ветки-группы раскрываются), поэтому
    // дополнительного сдвига пилюль по глубине нет — все жмутся к центру
    const int indent=0;

    // Свежая сторона («здесь будет»): исход важнее прошлого,
    // поэтому зелёный, а не цвет категории (красный у удалений врал бы).
    // Маркер операции оставляем — видно, откуда воскрешение/заливка
    const QColor freshGreen(0x66BB6A);

    if(index.column()==0 || index.column()==2)
    {
        const bool mine=(index.column()==0);
        const QString name=mine ? st.leftName : st.rightName;
        const MeldCellState::SideLook look=
            mine ? st.leftLook : st.rightLook;
        const QString hint=mine ? st.leftHint : st.rightHint;

        // Подсказка состояния ДО: пустая сторона + живая стрелка —
        // серый пунктир «сюда может приехать»
        if(look==MeldCellState::SideLook::Missing && !hint.isEmpty())
        {
            const QColor gray(0x9E9E9E);
            paintMeldPill(painter, o.font, o.rect,
                          meldMarker(item), gray,
                          hint, gray,
                          item.isBranch, false,
                          false, selected,
                          mine, indent, true);
            return;
        }

        if(look==MeldCellState::SideLook::Missing)
        {
            painter->setFont(o.font);
            painter->setPen(QColor(0xBDBDBD));
            if(mine)
                painter->drawText(
                    QRect(o.rect.right()-24, o.rect.top(), 20,
                          o.rect.height()),
                    Qt::AlignRight | Qt::AlignVCenter,
                    QStringLiteral("—"));
            else
                painter->drawText(
                    QRect(o.rect.left()+4+indent, o.rect.top(), 20,
                          o.rect.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    QStringLiteral("—"));
            return;
        }

        // Fresh = твёрдое «здесь будет»: та же обычная пилюля,
        // только зелёная (маркер операции — цвета исхода)
        const bool tomb=(look==MeldCellState::SideLook::Tomb);
        const QColor base=(look==MeldCellState::SideLook::Fresh)
            ? freshGreen : QColor(meldCategoryColor(item.category));

        paintMeldPill(painter, o.font, o.rect,
                      meldMarker(item), base,
                      name, base,
                      item.isBranch, tomb,
                      st.danger, selected,
                      mine, indent);
        return;
    }

    if(index.column()!=1)
        return;

    // Центр: → ← + diff. Цвета дуэлей дерева: вверх зелёный,
    // вниз синий, иначе дим. diff — линк, только у поединков
    const QColor green(0x66BB6A), blue(0x64B5F6);
    const QColor grayFg(0x777777), grayBd(0x555555);
    const QColor grayBg(0x777777, 0x777777, 0x777777, 0x28);
    const int radius=isLightTheme() ? 6 : 0;

    QRect rUp, rDown, rDiff;

    if(!cellLayout(option, index, rUp, rDown, rDiff))
        return;

    const bool hoverOn=(o.state & QStyle::State_MouseOver)!=0
        && m_hover==index && m_hoverPart!=0;

    QFont glyphFont=o.font;
    glyphFont.setBold(true);
    if(glyphFont.pointSize()>0)
        glyphFont.setPointSize(qMax(8, glyphFont.pointSize()-1));
    else
        glyphFont.setPixelSize(qMax(8, glyphFont.pixelSize()-1));

    auto paintArrow=[&](const QRect &r, const QString &glyph,
                        const QColor &solid, bool active,
                        bool enabled, bool hovered) {
        painter->setBrush(active ? solid : grayBg);
        painter->setPen(QPen(active ? solid
                                   : (hovered ? solid : grayBd), 1.5));
        if(!enabled)
            painter->setOpacity(0.3);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->drawRoundedRect(QRectF(r), radius, radius);
        painter->setPen(active ? QColor(Qt::white) : grayFg);
        painter->setFont(glyphFont);
        painter->drawText(r, Qt::AlignCenter, glyph);
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setOpacity(1.0);
    };

    paintArrow(rUp, QStringLiteral("→"), green,
               st.upOn, st.upEnabled, hoverOn && m_hoverPart==1);
    paintArrow(rDown, QStringLiteral("←"), blue,
               st.downOn, st.downEnabled, hoverOn && m_hoverPart==2);

    if(st.duel)
    {
        QFont linkFont=o.font;
        if(hoverOn && m_hoverPart==3)
            linkFont.setUnderline(true);
        painter->setFont(linkFont);
        painter->setPen(o.palette.color(QPalette::Text));
        painter->drawText(rDiff, Qt::AlignCenter, QStringLiteral("diff"));
    }
}


QSize SyncMeldDelegate::sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    Q_UNUSED(index);
    // Тонкие строки, как ужатые в дереве
    return QSize(0, option.fontMetrics.height()+12);
}


bool SyncMeldDelegate::cellLayout(const QStyleOptionViewItem &option,
                                 const QModelIndex &index,
                                 QRect &up, QRect &down, QRect &diff) const
{
    const int previewIdx=(m_dialog!=nullptr)
        ? m_dialog->m_meldModel->itemIndex(index) : -1;

    if(previewIdx<0)
        return false;

    const int btnW=34, btnH=24, gap=6;
    const int totalW=btnW*2+gap*2+44; // + diff-линк
    int x=option.rect.left()
        + qMax(0, (option.rect.width()-totalW)/2);
    const int cy=option.rect.top()+option.rect.height()/2;

    up=QRect(x, cy-btnH/2, btnW, btnH);
    x+=btnW+gap;
    down=QRect(x, cy-btnH/2, btnW, btnH);
    x+=btnW+gap;
    diff=QRect(x, cy-btnH/2, 44, btnH);
    return true;
}


int SyncMeldDelegate::hitTest(const QModelIndex &index,
                              const QPoint &viewportPos) const
{
    if(m_view==nullptr || m_dialog==nullptr)
        return 0;

    QStyleOptionViewItem option;
    option.initFrom(m_view);
    option.rect=m_view->visualRect(index);
    option.font=m_view->font();

    QRect rUp, rDown, rDiff;

    if(!cellLayout(option, index, rUp, rDown, rDiff))
        return 0;

    const int previewIdx=m_dialog->m_meldModel->itemIndex(index);
    const MeldCellState st=meldCellState(m_dialog, previewIdx);

    if(st.upEnabled && rUp.contains(viewportPos))
        return 1;

    if(st.downEnabled && rDown.contains(viewportPos))
        return 2;

    if(st.duel && rDiff.contains(viewportPos))
        return 3;

    return 0;
}


bool SyncMeldDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index)
{
    if(event->type()!=QEvent::ToolTip)
        return false;

    const int part=hitTest(index, event->pos());
    QString tip;

    if(part==1)
        tip=m_dialog!=nullptr ? m_dialog->meldArrowTip(
            m_dialog->m_meldModel->itemIndex(index), true) : QString();
    else if(part==2)
        tip=m_dialog!=nullptr ? m_dialog->meldArrowTip(
            m_dialog->m_meldModel->itemIndex(index), false) : QString();
    else if(part==3)
        tip=SyncPreviewDialog::tr("Показать diff");

    if(tip.isEmpty())
        return false;

    QToolTip::showText(event->globalPos(), tip, view);
    Q_UNUSED(option);
    return true;
}


// --- SyncMeldRing ---

SyncMeldRing::SyncMeldRing(SyncPreviewDialog *dialog, QWidget *parent)
    : QWidget(parent),
      m_dialog(dialog)
{
    setFixedSize(64, 64);
    setToolTip(tr("К первому нерешенному"));
    setCursor(Qt::PointingHandCursor);
}


void SyncMeldRing::setProgress(int done, int total)
{
    if(m_done==done && m_total==total)
        return;

    m_done=done;
    m_total=total;
    update();
}


QSize SyncMeldRing::sizeHint(void) const
{
    return QSize(64, 64);
}


void SyncMeldRing::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int side=qMin(width(), height());
    const QRectF bounds(5, 5, side-10, side-10);

    QPen track(QColor(0xD5D8E0), 7);
    track.setCapStyle(Qt::RoundCap);
    painter.setPen(track);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(bounds);

    const int rest=m_total-m_done;

    if(m_total>0)
    {
        // Красный сектор — корнем, а не линейно: на тысячах строк
        // линейный остаток в доли градуса не виден вообще, а корень
        // держит его заметным (1 из 1000 — ~11°). Плюс пол, чтобы
        // единичный остаток читался и на 100k. Зелёный — всё остальное
        const double frac=static_cast<double>(rest)
            / static_cast<double>(qMax(1, m_total));
        double redAngle=(rest<=0 || frac<=0.0)
            ? 0.0 : 360.0*qSqrt(qBound(0.0, frac, 1.0));

        if(rest>0 && redAngle < 6.0)
            redAngle=6.0;

        if(redAngle>0.0)
        {
            QPen red(QColor(0xEF5350), 7);
            red.setCapStyle(Qt::RoundCap);
            painter.setPen(red);
            painter.drawArc(bounds, 90*16, -static_cast<int>(redAngle*16));
        }

        if(redAngle < 360.0)
        {
            QPen green(QColor(0x66BB6A), 7);
            green.setCapStyle(Qt::RoundCap);
            painter.setPen(green);
            painter.drawArc(bounds, static_cast<int>((90.0-redAngle)*16),
                            -static_cast<int>((360.0-redAngle)*16));
        }
    }

    // Текст вписываем в круг уменьшением кегля (384/384 и 100000/100000
    // должны влезать без обрезки)
    const QString label=QStringLiteral("%1/%2").arg(m_done).arg(m_total);
    QFont f=painter.font();
    f.setBold(true);

    for(int pt=11; pt>=6; --pt)
    {
        f.setPointSize(pt);

        if(QFontMetrics(f).horizontalAdvance(label) <= side-18)
            break;
    }

    painter.setFont(f);
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(rect(), Qt::AlignCenter, label);
}


void SyncMeldRing::mousePressEvent(QMouseEvent *event)
{
    if(event->button()==Qt::LeftButton && m_dialog!=nullptr)
        m_dialog->onJumpClicked();

    QWidget::mousePressEvent(event);
}
