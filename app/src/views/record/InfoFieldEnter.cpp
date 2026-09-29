#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGridLayout>
#include <QToolButton>
#include <QSizePolicy>
#include <QTimer>

#include <QCompleter>
#include <QStringListModel>
#include <QShowEvent>

#include "InfoFieldEnter.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/helpers/DebugHelper.h"
#include "libraries/helpers/ObjectHelper.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "views/tree/KnowTreeView.h"
#include "views/findInBaseScreen/FindScreen.h"

extern AppConfig mytetraConfig;

// Виджет ввода инфополей


InfoFieldEnter::InfoFieldEnter(QWidget *parent) : QWidget(parent)
{
    setup_ui();
    setup_signals();
    assembly();

    setupTagsCompleter();
}

InfoFieldEnter::~InfoFieldEnter()
{

}

void InfoFieldEnter::setup_ui(void)
{
    // Элементы для запроса названия записи
    recordNameLabel = new QLabel(this);
    recordNameLabel->setText(tr("Title"));
    recordName = new QLineEdit(this);
    recordName->setMinimumWidth(500);

    // Элементы для запроса автора (авторов)
    recordAuthorLabel = new QLabel(this);
    recordAuthorLabel->setText(tr("Author(s)"));
    recordAuthor = new QLineEdit(this);

    // Элементы для запроса Url источника
    recordUrlLabel = new QLabel(this);
    recordUrlLabel->setText(tr("Url"));
    recordUrl = new QLineEdit(this);

    // Элементы для запроса текстовых меток
    recordTagsLabel = new QLabel(this);
    recordTagsLabel->setText(tr("Tags"));
    recordTags = new QLineEdit(this);

    // Кнопка раскрытия или закрытия полей author, url, tags...
    // Она в два раза меньше обычного размера
    expandInfo = new QToolButton(this);
    expandInfo->setObjectName("infoFieldEnterExpandInfo");
    expandInfo->setVisible(true);
    int w = expandInfo->geometry().width();
    int h = expandInfo->geometry().height();
    int x = qMin(w,h)/2;
    expandInfo->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed, QSizePolicy::ToolButton));
    expandInfo->setMinimumSize(x,x);
    expandInfo->setMaximumSize(x,x);
    expandInfo->resize(x,x);
    if (mytetraConfig.get_addnewrecord_expand_info()=="0")
    {
        expandInfo->setIcon(QIcon(":/resource/pic/triangl_dn.svg"));
        // expandInfo->setIcon(this->style()->standardIcon(QStyle::SP_ArrowDown));
    }
    else
    {
        expandInfo->setIcon(QIcon(":/resource/pic/triangl_up.svg"));
        // expandInfo->setIcon(this->style()->standardIcon(QStyle::SP_ArrowUp));
    }
}


void InfoFieldEnter::setup_signals(void)
{
    connect(expandInfo, &QToolButton::pressed, this, &InfoFieldEnter::expandInfoClick);
}


void InfoFieldEnter::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    // Словарь подсказок свежий при каждом показе: теги базы
    // могли измениться с прошлого раза
    refreshTagsCompleter();
}


// Создание подсказки автодополнения для поля тегов.
// Словарь подставится позже в refreshTagsCompleter
void InfoFieldEnter::setupTagsCompleter(void)
{
    // Словарь подсказок: регистр не важен, модель отсортирована
    // для быстрого поиска. Выпадашка показывает не больше десяти строк
    tagsCompleterModel=new QStringListModel(this);

    tagsCompleter=new QCompleter(this);
    tagsCompleter->setModel(tagsCompleterModel);
    tagsCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    tagsCompleter->setCompletionMode(QCompleter::PopupCompletion);
    tagsCompleter->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    tagsCompleter->setMaxVisibleItems(10);

    // Совпадение подстрокой а не с начала слова: rnet находит internet.
    // Словарь уже собран, фильтрация по нему копеечная
    tagsCompleter->setFilterMode(Qt::MatchContains);

    // Только привязка к виджету для позиционирования выпадашки.
    // setCompleter не используется: иначе QLineEdit ищет совпадение
    // всей строки и подсказка после запятой не появляется.
    // Привод полностью ручной из onTagsEdited
    tagsCompleter->setWidget(recordTags);
    tagsCompleterEnabled=false;

    connect(recordTags, &QLineEdit::textEdited,
            this,        &InfoFieldEnter::onTagsEdited);

    connect(recordTags, &QLineEdit::returnPressed,
            this,        &InfoFieldEnter::onTagsReturn);

    connect(tagsCompleter, qOverload<const QString &>(&QCompleter::activated),
            this,          &InfoFieldEnter::onTagCompletion);
}


// Пересборка словаря подсказок по всем тегам базы
void InfoFieldEnter::refreshTagsCompleter(void)
{
    QMap<QString, QStringList> dictionaries;
    QSet<QString> seen;

    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

    if(treeView!=nullptr)
    {
        KnowTreeModel *treeModel=static_cast<KnowTreeModel*>(treeView->model());

        const TreeItem *rootItem=treeModel->getRootItem();

        if(rootItem!=nullptr)
            FindScreen::collectBranchValues(rootItem, dictionaries, seen);
    }

    QStringList tags=dictionaries.value("tags");
    tags.sort(Qt::CaseInsensitive);

    tagsCompleterModel->setStringList(tags);

    // Подсказывать нечего: ручной привод выключается
    tagsCompleterEnabled=!tags.isEmpty();

    if(tags.isEmpty())
        tagsCompleter->popup()->hide();
}


// Последний недопечатанный тег: хвост после крайней запятой
// или точки с запятой. Разделитель в конце значит тег допечатан
QString InfoFieldEnter::lastTagToken(const QString &text)
{
    int end=text.length();

    while(end>0 && text.at(end-1).isSpace())
        end--;

    if(end==0)
        return QString("");

    if(text.at(end-1)==',' || text.at(end-1)==';')
        return QString("");

    // Тег может содержать пробелы, граница только по разделителям
    int start=end;

    while(start>0 && text.at(start-1)!=',' && text.at(start-1)!=';')
        start--;

    while(start<end && text.at(start).isSpace())
        start++;

    return text.mid(start, end-start);
}


// Подставить выбранное дополнение вместо последнего тега.
// В конец добавляется запятая с пробелом для следующего тега
QString InfoFieldEnter::applyTagCompletion(const QString &text, const QString &completion)
{
    int end=text.length();

    while(end>0 && text.at(end-1).isSpace())
        end--;

    // Тег допечатан: дополнение дописывается как есть,
    // пробелы после разделителя сохраняются
    if(end>0 && (text.at(end-1)==',' || text.at(end-1)==';'))
        return text+completion+", ";

    int start=end;

    while(start>0 && text.at(start-1)!=',' && text.at(start-1)!=';')
        start--;

    // Пробелы после разделителя сохраняются: "work, ho" дает "work, home, "
    while(start<end && text.at(start).isSpace())
        start++;

    return text.left(start)+completion+", ";
}


// Срезать висячий хвост разделителей в конце строки
QString InfoFieldEnter::trimTagTail(const QString &text)
{
    QString trimmed=text;
    trimmed.replace(QRegExp("[,;\\s]+$"), "");

    return trimmed;
}


// Дополняется только последний недопечатанный тег. Выпадашка
// появляется начиная с двух букв, при отсутствии совпадений
// прячется а не висит пустой
void InfoFieldEnter::onTagsEdited(const QString &text)
{
    if(!tagsCompleterEnabled)
        return;

    QString token=lastTagToken(text);

    if(token.length()<2)
    {
        tagsCompleter->popup()->hide();
        return;
    }

    tagsCompleter->setCompletionPrefix(token);

    if(tagsCompleter->completionCount()==0)
    {
        tagsCompleter->popup()->hide();
        return;
    }

    tagsCompleter->complete();
}


void InfoFieldEnter::onTagCompletion(const QString &completion)
{
    recordTags->setText(applyTagCompletion(recordTags->text(), completion));
}


// Enter в поле тегов срезает висячий хвост разделителей.
// Сохранение через другие пути чистится в getField
void InfoFieldEnter::onTagsReturn(void)
{
    recordTags->setText(trimTagTail(recordTags->text()));
}


// Элементы собираются в размещалку
void InfoFieldEnter::assembly(void)
{
    // Размещалка элементов
    infoFieldLayout=new QGridLayout(); // Попробовать this
    infoFieldLayout->setMargin(8);
    infoFieldLayout->setSpacing(10);

    int y = -1;

    infoFieldLayout->addWidget(recordNameLabel,++y,0);
    infoFieldLayout->addWidget(recordName,y,1);

    infoFieldLayout->addWidget(expandInfo,y,2);

    infoFieldLayout->addWidget(recordAuthorLabel,++y,0);
    infoFieldLayout->addWidget(recordAuthor,y,1);

    infoFieldLayout->addWidget(recordUrlLabel,++y,0);
    infoFieldLayout->addWidget(recordUrl,y,1);

    infoFieldLayout->addWidget(recordTagsLabel,++y,0);
    infoFieldLayout->addWidget(recordTags,y,1);

    // Устанавливается видимость или невидимость полей author, url, tags...
    expandInfoOnDisplay( mytetraConfig.get_addnewrecord_expand_info() );

    // Полученый набор элементов устанавливается для текущего виджета
    setLayout(infoFieldLayout);

    // Границы убираются, так как данный объект будет использоваться
    // как виджет
    QLayout *lt;
    lt = layout();
    lt->setContentsMargins(0,0,0,0);

    // setSizePolicy(QSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum));
}


void InfoFieldEnter::expandInfoOnDisplay(QString expand)
{
    bool i;

    if (expand=="0")
        i=false;
    else
        i=true;

    recordAuthorLabel->setVisible(i);
    recordAuthor->setVisible(i);

    recordUrlLabel->setVisible(i);
    recordUrl->setVisible(i);

    recordTagsLabel->setVisible(i);
    recordTags->setVisible(i);
}


void InfoFieldEnter::expandInfoClick(void)
{
    // Если в данный момент информация "свернута"
    if (mytetraConfig.get_addnewrecord_expand_info()=="0")
    {
        // Надо информацию развернуть
        expandInfoOnDisplay("1");

        mytetraConfig.set_addnewrecord_expand_info("1");

        expandInfo->setIcon(QIcon(":/resource/pic/triangl_up.svg"));
    }
    else
    {
        // Надо информацию свернуть
        expandInfoOnDisplay("0");

        mytetraConfig.set_addnewrecord_expand_info("0");

        expandInfo->setIcon(QIcon(":/resource/pic/triangl_dn.svg"));
    }
}


void InfoFieldEnter::setFocusToStart(void)
{
    recordName->setFocus(Qt::TabFocusReason);

    // Сброс автоматического выделения текста,
    // произошедший при переключении фокуса
    // Используется QTimer, чтобы переместить курсор
    // после обработки всех текущих событий, так как данный метод вызывается
    // в assemly(), когда цикл событий виджета еще не запущен
    QTimer::singleShot(0, [this]() {
        recordName->setCursorPosition( 0 );
    });
}


bool InfoFieldEnter::checkFieldName(QString name)
{
    if (name=="name" ||
            name=="author" ||
            name=="url" ||
            name=="tags")
        return true;
    else
        return false;
}


QString InfoFieldEnter::getField(QString name)
{
    if (checkFieldName(name))
    {
        if (name=="name")  return  recordName->text();
        if (name=="author")return  recordAuthor->text();
        if (name=="url")   return  recordUrl->text();
        // При возврате значения, для тегов обязательно нужно убирать переносы строк, если они есть.
        // Висячий хвост разделителей тоже срезается: остается от дополнения через ", "
        if (name=="tags")  return  trimTagTail( recordTags->text().simplified() );
    }
    else
        criticalError("Can not get field "+name+" in InfoFieldEnter method get_field");

    return QString();
}


void InfoFieldEnter::setField(QString name,QString value)
{
    if (checkFieldName(name))
    {
        if (name=="name")  recordName->setText(value);
        if (name=="author")recordAuthor->setText(value);
        if (name=="url")   recordUrl->setText(value);
        if (name=="tags")  recordTags->setText(value.simplified()); // При внешней установке значения, для тегов обязательно нужно убирать переносы строк, если они есть
    }
    else
        criticalError("Can not set field "+name+" in InfoFieldEnter method set_field");
}


void InfoFieldEnter::setReadOnly(bool state)
{
    recordName->setReadOnly(state);
    recordAuthor->setReadOnly(state);
    recordUrl->setReadOnly(state);
    recordTags->setReadOnly(state);
}


bool InfoFieldEnter::isReadOnly()
{
    return recordName->isReadOnly();
}
