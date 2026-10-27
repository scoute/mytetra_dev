#include <QDebug>
#include <QtGlobal>
#include <QPainter>
#include <QStyledItemDelegate>

#include "EditorFontFamilyComboBox.h"


// Делегат всплывающего списка: пустые строки штатный делегат
// QFontComboBox рисует мусором (буква вместо пустого места).
// Обертка рисует сама только их: строка-разделитель — линию,
// ""-маркер смешанных шрифтов — пусто. Остальное отдается
// штатному делегату без изменений
class FontListDelegate : public QStyledItemDelegate
{
public:
    explicit FontListDelegate(QAbstractItemDelegate *base, QObject *parent=nullptr)
        : QStyledItemDelegate(parent), baseDelegate(base), separatorRow(-1) {}

    void setSeparatorRow(int row) { separatorRow=row; }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const bool isEmptyRow=index.data(Qt::DisplayRole).toString().isEmpty();

        if(!isEmptyRow)
        {
            baseDelegate->paint(painter, option, index);
            return;
        }

        if(option.state & QStyle::State_Selected)
            painter->fillRect(option.rect, option.palette.highlight());

        if(index.row()==separatorRow)
        {
            painter->save();
            const int y=option.rect.center().y();
            painter->setPen(option.palette.color(QPalette::Mid));
            painter->drawLine(option.rect.left()+6, y,
                              option.rect.right()-6, y);
            painter->restore();
        }
        // ""-маркер: рисуется пустым (только подсветка выше)
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        return baseDelegate->sizeHint(option, index);
    }

private:
    QAbstractItemDelegate *baseDelegate;
    int separatorRow;
};


EditorFontFamilyComboBox::EditorFontFamilyComboBox(QWidget *parent) :
    QFontComboBox(parent),
    toolPseudoButton(this)
{
    Q_UNUSED(parent)

    isProgrammChanged=false;

    // Пустой пункт в конце списка шрифтов, используется для обозначения что в
    // выделенном тексте несколько шрифтов
    this->insertItem(0, "");
    this->setItemIcon(0, QIcon(":/resource/pic/edit_font_many.svg")); // Установка иконки в QFontComboBox не работает, но пока оставлено

    previousIndex=this->currentIndex();

    recentRows=0;

    // Обертка над штатным делегатом вьюхи: пустые строки рисуются
    // вручную (линия разделителя, пустой маркер), остальное как было
    listDelegate=new FontListDelegate(this->view()->itemDelegate(), this->view());
    this->view()->setItemDelegate(listDelegate);

    QRegularExpression rx(".+"); // 1 или больше символов (т. е. не пустая строка)
    QValidator *validator = new QRegularExpressionValidator(rx, this);
    this->setValidator(validator);

    this->setupSignals();
}


EditorFontFamilyComboBox::~EditorFontFamilyComboBox()
{

}


void EditorFontFamilyComboBox::setupSignals()
{
    connect(this, qOverload<int>(&EditorFontFamilyComboBox::currentIndexChanged),
            this, &EditorFontFamilyComboBox::onCurrentIndexChanged);
}


void EditorFontFamilyComboBox::setIsProgrammChanged(bool flag)
{
    isProgrammChanged=flag;
}


// Данный слот срабатывает уже после того, как в объекте выставлен индекс
void EditorFontFamilyComboBox::onCurrentIndexChanged(int index)
{
    // Устанавливать неизвестный размер шрифта можно только программно
    if(!isProgrammChanged && index==0) {
        qDebug() << "EditorFontFamilyComboBox::onCurrentIndexChanged. Disable manual set empty font.";
        this->setCurrentIndex(previousIndex);
        return;
    }

    // Строка-разделитель после недавних — не выбор, откат назад
    if(!isProgrammChanged && recentRows>0 && index==recentRows+1) {
        qDebug() << "EditorFontFamilyComboBox::onCurrentIndexChanged. Separator row is not selectable.";
        this->setCurrentIndex(previousIndex);
        return;
    }

    previousIndex=index;
}


// Недавние шрифты сверху списка: после ""-маркера (индекс 0 занят им,
// логика onCurrentIndexChanged на него завязана) идут до 10 шрифтов
// и разделитель. Отсутствующие в системе пропускаются в показе,
// но место в конфиге держат. Перестройка без сигналов, текущий
// шрифт возвращается обратно, previousIndex правится под сдвиг
void EditorFontFamilyComboBox::reloadRecentFonts(const QStringList &recent, const QString &currentFamily)
{
    this->blockSignals(true);

    // Снос прошлой группы (шрифты; линия-разделитель всегда сразу за ними)
    for(int i=0; i<recentRows; ++i)
        this->removeItem(1);

    if(recentRows>0 && this->count()>1)
        this->removeItem(1);

    recentRows=0;

    // Новые сверху: обход в прямом порядке со вставкой на growing row
    int row=1;

    foreach(const QString &family, recent)
    {
        // В основном списке шрифта нет (удален из системы) — пропуск
        if(this->findText(family)==-1)
            continue;

        this->insertItem(row, family);
        row++;
        recentRows++;
    }

    if(recentRows>0)
        this->insertSeparator(row);

    // Разделитель известен делегату: рисует линию вместо мусора
    if(listDelegate!=nullptr)
        listDelegate->setSeparatorRow(recentRows>0 ? recentRows+1 : -1);

    // Текущий шрифт обратно (мог съехать из-за вставки строк).
    // Если он есть среди недавних — выделяется строка сверху,
    // а не дубль из общего списка
    int currentIndex=recentRowForFamily(currentFamily);

    if(currentIndex<0)
        currentIndex=this->findText(currentFamily);

    if(currentIndex>=0)
    {
        this->setCurrentIndex(currentIndex);
        previousIndex=currentIndex;
    }

    this->blockSignals(false);
}


// Строка шрифта в группе недавних (после ""-маркера) или -1.
// Сравнение строгое: имена в группе — те же family-строки,
// что приходят с курсора и из конфига
int EditorFontFamilyComboBox::recentRowForFamily(const QString &family) const
{
    if(family.isEmpty() || recentRows<=0)
        return -1;

    for(int row=1; row<=recentRows; ++row)
        if(this->itemText(row)==family)
            return row;

    return -1;
}



