#include <QDebug>
#include <QtGlobal>

#include "EditorFontFamilyComboBox.h"


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

    previousIndex=index;
}


// Недавние шрифты сверху списка: после ""-маркера (индекс 0 занят им,
// логика onCurrentIndexChanged на него завязана) идут до 5 шрифтов
// и разделитель. Отсутствующие в системе пропускаются в показе,
// но место в конфиге держат. Перестройка без сигналов, текущий
// шрифт возвращается обратно, previousIndex правится под сдвиг
void EditorFontFamilyComboBox::reloadRecentFonts(const QStringList &recent, const QString &currentFamily)
{
    this->blockSignals(true);

    // Снос прошлой группы (шрифты; разделитель всегда сразу за ними)
    for(int i=0; i<recentRows; ++i)
        this->removeItem(1);

    if(this->count()>1 && this->itemText(1).isEmpty())
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
        this->setItemData(row, QFont(family), Qt::FontRole);
        row++;
        recentRows++;
    }

    if(recentRows>0)
        this->insertSeparator(row);

    // Текущий шрифт обратно (мог съехать из-за вставки строк)
    const int currentIndex=this->findText(currentFamily);

    if(currentIndex>=0)
    {
        this->setCurrentIndex(currentIndex);
        previousIndex=currentIndex;
    }

    this->blockSignals(false);
}



