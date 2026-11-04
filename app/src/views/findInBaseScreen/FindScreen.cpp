#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QToolButton>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QWidget>
#include <QProgressDialog>
#include <QLabel>
#include <QCheckBox>
#include <QFrame>
#include <QMessageBox>
#include <QTextDocument>
#include <QApplication>
#include <QRegularExpression>

#include <QCompleter>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QTimer>
#include <QScopedPointer>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QDebug>

#include <algorithm>

#include "views/mainWindow/MainWindow.h"
#include "FindScreen.h"
#include "FindTableWidget.h"
#include "models/tree/KnowTreeModel.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppConfigStorage.h"
#include "models/tree/TreeItem.h"
#include "libraries/GlobalParameters.h"
#include "libraries/FtsTextIndex.h"
#include "views/tree/TreeScreen.h"
#include "libraries/MtComboBox.h"
#include "views/tree/KnowTreeView.h"
#include "models/recordTable/Record.h"
#include "models/attachTable/AttachTableData.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/CssHelper.h"
#include "libraries/AppContext.h"
#include "models/appConfig/AppConfigUi.h"
#include "models/appConfig/AppConfigFind.h"

FindScreen::FindScreen(QWidget *parent) : QWidget(parent),
    exactTagMode(false)
{
    setupFindTextAndButton();
    assemblyFindTextAndButton();

    setupCloseButton();
    assemblyCloseButton();

    setupComboOption();
    assemblyComboOption();

    setupWhereFindLine();
    assemblyWhereFindLine();

    setupUI();
    assembly();

    setupSignals();

    setupFieldCompleter();
}

FindScreen::~FindScreen(void)
{

}

// Текст поиска и кнопка "Поиск"
void FindScreen::setupFindTextAndButton(void)
{
    // Поле текста для поиска
    findText=new QLineEdit();

    // Кнопка "Поиск"
    findStartButton=new QPushButton(this);
    findStartButton->setText(tr("Find"));
    findStartButton->setDefault(true);
    findStartButton->setEnabled(false);

    // Кнопка разворачивания инструментов
    toolsExpand=new QToolButton(this);
    toolsExpand->setIcon(QIcon(":/resource/pic/find_in_base_expand_tools.svg"));
    toolsExpand->setEnabled(true);
}

// Текст поиска и кнопка "Поиск"
void FindScreen::assemblyFindTextAndButton(void)
{
    toolsAreaFindTextAndButton=new QHBoxLayout();
    toolsAreaFindTextAndButton->addWidget(findText);
    toolsAreaFindTextAndButton->addWidget(findStartButton);
    toolsAreaFindTextAndButton->addWidget(toolsExpand);

    // Слева устанавливается отступ, чтобы поле ввода текста для поиска
    // не прижималось к левому краю
    int leftMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin)/2;
    toolsAreaFindTextAndButton->setContentsMargins(leftMargin, 0, 0, 0);
}

// Набор опций поиска в виде выпадающих списков
void FindScreen::setupComboOption(void)
{
    // Выбор "Любое слово" - "Все слова"
    wordRegard=new MtComboBox();
    wordRegard->addItem(QIcon(":/resource/pic/find_in_base_any.svg"), tr("Any word"));
    wordRegard->addItem(QIcon(":/resource/pic/find_in_base_all.svg"), tr("All words"));
    wordRegard->setCurrentIndex(AppContext::instance().appConfig()->findSection()->get_findscreen_wordregard());

    // Выбор "Только целые слова" - "Подстрока"
    howExtract=new MtComboBox();
    howExtract->addItem(QIcon(":/resource/pic/find_in_base_separate.svg"), tr("Whole words"));
    howExtract->addItem(QIcon(":/resource/pic/find_in_base_substring.svg"), tr("Substring"));
    howExtract->setCurrentIndex(AppContext::instance().appConfig()->findSection()->get_findscreen_howextract());

    // Выбор "Во всей базе" - "В текущей ветке"
    treeSearchArea=new MtComboBox();
    treeSearchArea->addItem(QIcon(":/resource/pic/find_in_base_search_all.svg"), tr("Entire base")); // Вся база
    treeSearchArea->addItem(QIcon(":/resource/pic/find_in_base_search_branch.svg"), tr("Current tree item")); // Текущая ветка
    treeSearchArea->setCurrentIndex(AppContext::instance().appConfig()->findSection()->getFindScreenTreeSearchArea());

    if(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="mobile")
    {
        // wordRegard->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        // howExtract->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        // treeSearchArea->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);

        // wordRegard->showMinimized();
        // howExtract->showMinimized();
        // treeSearchArea->showMinimized();

        wordRegard->setMinimumContentsLength(1);
        wordRegard->setMaximumWidth(CssHelper::getCalculateIconSizePx()*2);
        wordRegard->setMinimumWidth(CssHelper::getCalculateIconSizePx()*2);
        wordRegard->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);

        howExtract->setMinimumContentsLength(1);
        howExtract->setMaximumWidth(CssHelper::getCalculateIconSizePx()*2);
        howExtract->setMinimumWidth(CssHelper::getCalculateIconSizePx()*2);
        howExtract->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);

        treeSearchArea->setMinimumContentsLength(1);
        treeSearchArea->setMaximumWidth(CssHelper::getCalculateIconSizePx()*2);
        treeSearchArea->setMinimumWidth(CssHelper::getCalculateIconSizePx()*2);
        treeSearchArea->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    }
}

// Набор опций поиска в виде выпадающих списков
void FindScreen::assemblyComboOption(void)
{
    toolsAreaComboOption=new QHBoxLayout();
    toolsAreaComboOption->addWidget(wordRegard);
    toolsAreaComboOption->addWidget(howExtract);
    toolsAreaComboOption->addWidget(treeSearchArea);
    toolsAreaComboOption->addStretch();
}

void FindScreen::setupCloseButton(void)
{
    // Кнопка закрытия виджета: квадратный QPushButton с иконкой диалога,
    // стороной в высоту поля ввода. Размер ставится в fixCloseButtonSize
    // при показе: до раскладки высота поля неизвестна, а собственная
    // геометрия кнопки здесь еще мусор
    closeButton=new QPushButton(this);
    closeButton->setVisible(true);
    closeButton->setIcon(this->style()->standardIcon(QStyle::SP_DialogCloseButton));
    closeButton->setObjectName("findbarSquareButton");
    closeButton->setToolTip(tr("Close find widget"));
}

// Квадратная кнопка в размер поля ввода. Значку оставляется запас
// на внутренние отступы, иначе стиль его обрежет
void FindScreen::fixCloseButtonSize(void)
{
    int side=findText->sizeHint().height();

    if(side<=0)
      return;

    closeButton->setFixedSize(side, side);

    int iconSide=qMax(side-8, 12);
    closeButton->setIconSize(QSize(iconSide, iconSide));
}

void FindScreen::assemblyCloseButton(void)
{
    // Вертикальная область с кнопкой закрытия и распорками
    // с обеих сторон: кнопка центрируется по высоте строки,
    // как поле ввода в соседней области. Без верхней распорки
    // кнопка прижималась к верху и выходила на 1-2 пикселя выше
    toolsAreaCloseButton=new QVBoxLayout();
    toolsAreaCloseButton->addStretch();
    toolsAreaCloseButton->addWidget(closeButton);
    toolsAreaCloseButton->addStretch();

    // Справа устанавливается отступ, чтобы кнопка не прижималось сильно к правому краю
    int rightMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin)/2;
    toolsAreaCloseButton->setContentsMargins(0, 0, rightMargin, 0);
}

// Обновление прямых текстов новым языком. Индексы комбобоксов
// не трогаются: setItemText выбор не меняет
void FindScreen::retranslateUi(void)
{
    findStartButton->setText(tr("Find"));

    wordRegard->setItemText(0, tr("Any word"));
    wordRegard->setItemText(1, tr("All words"));

    howExtract->setItemText(0, tr("Whole words"));
    howExtract->setItemText(1, tr("Substring"));

    treeSearchArea->setItemText(0, tr("Entire base"));
    treeSearchArea->setItemText(1, tr("Current tree item"));

    whereFindLabel->setText(tr("Find in: "));

    findInName->setText(tr("Title"));
    findInAuthor->setText(tr("Author(s)"));
    findInUrl->setText(tr("Url"));
    findInTags->setText(tr("Tags"));
    findInText->setText(tr("Text"));
    findInNameItem->setText(tr("Name tree item"));
    findInAttach->setText(tr("Attach files"));
    jumpToNote->setText(tr("Jump to note text"));
    jumpFirstWord->setText(tr("Jump by first word only"));

    findTable->retranslateUi();
}

void FindScreen::setupWhereFindLine(void)
{
    whereFindLabel=new QLabel(tr("Find in: "));

    findInName=new QCheckBox(tr("Title"));
    findInName->setChecked(AppContext::instance().appConfig()->findSection()->get_findscreen_find_in_field("name"));

    findInAuthor=new QCheckBox(tr("Author(s)"));
    findInAuthor->setChecked(AppContext::instance().appConfig()->findSection()->get_findscreen_find_in_field("author"));

    findInUrl=new QCheckBox(tr("Url"));
    findInUrl->setChecked(AppContext::instance().appConfig()->findSection()->get_findscreen_find_in_field("url"));

    findInTags=new QCheckBox(tr("Tags"));
    findInTags->setChecked(AppContext::instance().appConfig()->findSection()->get_findscreen_find_in_field("tags"));

    findInText=new QCheckBox(tr("Text"));
    findInText->setChecked(AppContext::instance().appConfig()->findSection()->get_findscreen_find_in_field("text"));

    findInNameItem=new QCheckBox(tr("Name tree item"));
    findInNameItem->setChecked(AppContext::instance().appConfig()->findSection()->get_findscreen_find_in_field("nameItem"));

    findInAttach=new QCheckBox(tr("Attach files"));
    findInAttach->setChecked(AppContext::instance().appConfig()->findSection()->get_findscreen_find_in_field("attach"));

    jumpToNote=new QCheckBox(tr("Jump to note text"));
    jumpToNote->setChecked(AppContext::instance().appConfig()->findSection()->get_findscreen_find_in_field("jumpToNote"));

    jumpFirstWord=new QCheckBox(tr("Jump by first word only"));
    jumpFirstWord->setChecked(AppContext::instance().appConfig()->findSection()->get_findscreen_find_in_field("jumpFirstWord"));
    jumpFirstWord->setEnabled(jumpToNote->isChecked());
}

void FindScreen::assemblyWhereFindLine(void)
{
    whereFindLine=new QHBoxLayout();

    if(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="desktop")
        whereFindLine->addWidget(whereFindLabel);

    if(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="mobile")
        whereFindLabel->hide();

    whereFindLine->addWidget(findInName);
    whereFindLine->addWidget(findInAuthor);
    whereFindLine->addWidget(findInUrl);
    whereFindLine->addWidget(findInTags);
    whereFindLine->addWidget(findInText);
    whereFindLine->addWidget(findInNameItem);
    whereFindLine->addWidget(findInAttach);

    QFrame *jumpSeparator=new QFrame(this);
    jumpSeparator->setFrameShape(QFrame::VLine);
    jumpSeparator->setFrameShadow(QFrame::Sunken);
    whereFindLine->addWidget(jumpSeparator);

    whereFindLine->addWidget(jumpToNote);
    whereFindLine->addWidget(jumpFirstWord);

    whereFindLine->addStretch();

    whereFindLine->setContentsMargins(3,0,0,0); // Устанавливаются границы
}

void FindScreen::setupSignals(void)
{
    // При каждом изменении текста в строке запроса
    connect(findText, &QLineEdit::textChanged,
            this,     &FindScreen::enableFindButton);

    // При каждом изменении текста извне может вырабатыватся этот сигнал
    // Он вырабатывается в слоте setFindText()
    connect(this, &FindScreen::textChangedFromAnother,
            this, &FindScreen::enableFindButton);

    // При нажатии Enter в строке запроса
    connect(findText, &QLineEdit::returnPressed,
            this,     &FindScreen::findClicked);

    // При нажатии кнопки Find
    connect(findStartButton, &QPushButton::clicked,
            this,            &FindScreen::findClicked);

    // При нажатии кнопки разворачивания/сворачивания инструментов поиска
    connect(toolsExpand, &QToolButton::clicked,
            this,        &FindScreen::toolsExpandClicked);

    // После установки текста извне, вырабатывается этот сигнал
    connect(this, &FindScreen::findClickedAfterAnotherTextChanged,
            this, &FindScreen::findClicked);

    // При нажатии кнопки закрытия
    connect(closeButton, &QPushButton::clicked,
            this,        &FindScreen::widgetHide);

    // Сигналы для запоминания состояния интерфейса
    connect(wordRegard, qOverload<int>(&MtComboBox::currentIndexChanged),
            this,       &FindScreen::changedWordRegard);

    connect(howExtract, qOverload<int>(&MtComboBox::currentIndexChanged),
            this,       &FindScreen::changedHowExtract);

    connect(treeSearchArea, qOverload<int>(&MtComboBox::currentIndexChanged),
            this,           &FindScreen::changedTreeSearchArea);

    connect(findInName, &QCheckBox::stateChanged,
            this,       &FindScreen::changedFindInName);

    connect(findInAuthor, &QCheckBox::stateChanged,
            this,         &FindScreen::changedFindInAuthor);

    connect(findInUrl, &QCheckBox::stateChanged,
            this,      &FindScreen::changedFindInUrl);

    connect(findInTags, &QCheckBox::stateChanged,
            this,       &FindScreen::changedFindInTags);

    connect(findInText, &QCheckBox::stateChanged,
            this,       &FindScreen::changedFindInText);

    connect(findInNameItem, &QCheckBox::stateChanged,
            this,           &FindScreen::changedFindInNameItem);

    connect(findInAttach, &QCheckBox::stateChanged,
            this,         &FindScreen::changedFindInAttach);

    connect(jumpToNote, &QCheckBox::stateChanged,
            this,       &FindScreen::changedJumpToNote);

    connect(jumpFirstWord, &QCheckBox::stateChanged,
            this,         &FindScreen::changedJumpFirstWord);
}

void FindScreen::setupUI(void)
{
    findTable=new FindTableWidget();
}

void FindScreen::assembly(void)
{
    centralDesktopLayout=new QVBoxLayout();

    if(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="desktop")
    {
        toolsLine=new QHBoxLayout();
        toolsLine->addLayout(toolsAreaFindTextAndButton);
        toolsLine->addLayout(toolsAreaComboOption);
        toolsLine->addLayout(toolsAreaCloseButton);

        centralDesktopLayout->addLayout(toolsLine);
    }

    if(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="mobile")
    {
        toolsGrid=new QGridLayout();
        toolsGrid->addLayout(toolsAreaFindTextAndButton, 0, 0);
        toolsGrid->addLayout(toolsAreaCloseButton,       0, 1);
        toolsGrid->addLayout(toolsAreaComboOption,       1, 0);

        centralDesktopLayout->addLayout(toolsGrid);
    }

    centralDesktopLayout->addLayout(whereFindLine);
    centralDesktopLayout->addWidget(findTable, 10);
    // Верх 4px: отступ между ручкой сплиттера и строкой поиска,
    // иначе поиск липнет к разделителю
    centralDesktopLayout->setContentsMargins(0,4,0,0); // Границы убираются
    centralDesktopLayout->setSizeConstraint(QLayout::SetNoConstraint);

    // whereFindLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    // findTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    this->setLayout(centralDesktopLayout);

    switchToolsExpand(AppContext::instance().appConfig()->uiSection()->getFindInBaseExpand());
}

void FindScreen::enableFindButton(const QString &text)
{
    findStartButton->setEnabled(!text.isEmpty());
}

// Слот, с помощью которого другие виджеты могут устанавливать
// текст для поиска
void FindScreen::setFindText(QString text)
{
    findText->setText(text);

    emit textChangedFromAnother(text);
    emit findClickedAfterAnotherTextChanged();
}

// Поиск кликнутого тега только в поле тегов.
// Галки пользователя запоминаются, на время поиска остается только
// "Теги", потом все возвращается. Сигналы чекбоксов блокируются:
// иначе временное переключение затрет сохраненные настройки поиска.
// Сам поиск синхронный (setFindText дергает findClicked напрямую),
// поэтому возврат после него безопасен
void FindScreen::setFindTag(QString tag)
{
    bool savedName=findInName->isChecked();
    bool savedAuthor=findInAuthor->isChecked();
    bool savedUrl=findInUrl->isChecked();
    bool savedTags=findInTags->isChecked();
    bool savedText=findInText->isChecked();
    bool savedNameItem=findInNameItem->isChecked();
    bool savedAttach=findInAttach->isChecked();

    findInName->blockSignals(true);
    findInAuthor->blockSignals(true);
    findInUrl->blockSignals(true);
    findInTags->blockSignals(true);
    findInText->blockSignals(true);
    findInNameItem->blockSignals(true);
    findInAttach->blockSignals(true);

    findInName->setChecked(false);
    findInAuthor->setChecked(false);
    findInUrl->setChecked(false);
    findInTags->setChecked(true);
    findInText->setChecked(false);
    findInNameItem->setChecked(false);
    findInAttach->setChecked(false);

    // Строгое совпадение: ищется тег целиком, а не подстрока.
    // Взводится до синхронного поиска, снимается после него
    exactTagMode=true;
    exactTag=tag;

    setFindText(tag);

    exactTagMode=false;
    exactTag.clear();

    findInName->blockSignals(true);
    findInAuthor->blockSignals(true);
    findInUrl->blockSignals(true);
    findInTags->blockSignals(true);
    findInText->blockSignals(true);
    findInNameItem->blockSignals(true);
    findInAttach->blockSignals(true);

    findInName->setChecked(savedName);
    findInAuthor->setChecked(savedAuthor);
    findInUrl->setChecked(savedUrl);
    findInTags->setChecked(savedTags);
    findInText->setChecked(savedText);
    findInNameItem->setChecked(savedNameItem);
    findInAttach->setChecked(savedAttach);

    findInName->blockSignals(false);
    findInAuthor->blockSignals(false);
    findInUrl->blockSignals(false);
    findInTags->blockSignals(false);
    findInText->blockSignals(false);
    findInNameItem->blockSignals(false);
    findInAttach->blockSignals(false);
}

// Разбить поле тегов записи на отдельные теги: разделители запятая
// и точка с запятой, пробелы по краям отбрасываются.
// Регулярка статическая: иначе компиляция паттерна на каждую
// из тысяч записей (поиск, панель тегов, комплитер)
QStringList FindScreen::splitRecordTags(const QString &tagsField)
{
    static const QRegularExpression tagSeparator(QStringLiteral("[,;]+"));

    QStringList tags=tagsField.split(tagSeparator, Qt::SkipEmptyParts);

    for(int i=0; i<tags.size(); ++i)
        tags[i]=tags[i].trimmed();

    tags.removeAll(QString(""));

    return tags;
}

// Точное совпадение тега без выделения списка: проход по полю
// без аллокаций QStringList на каждую запись. Нужен только факт
// наличия, сам список в строгом режиме нигде не используется
bool FindScreen::recordHasTag(const QString &tagsField, const QString &exactTag)
{
    if(tagsField.isEmpty() || exactTag.isEmpty())
        return false;

    int start=0;
    const int len=tagsField.length();

    while(start<len)
    {
        int end=start;

        while(end<len && tagsField.at(end)!=',' && tagsField.at(end)!=';')
            ++end;

        int left=start;
        int right=end;

        while(left<right && tagsField.at(left).isSpace())
            ++left;

        while(right>left && tagsField.at(right-1).isSpace())
            --right;

        if(right>left)
        {
            const int tokenLen=right-left;

            if(tokenLen==exactTag.length() &&
               tagsField.midRef(left, tokenLen).compare(exactTag, Qt::CaseInsensitive)==0)
                return true;
        }

        start=end+1;
    }

    return false;
}

// Слова запроса текущего поиска для моста в редактор
QStringList FindScreen::getSearchWordList(void) const
{
    return searchWordList;
}

// Включен ли прыжок в текст заметки для моста в редактор
bool FindScreen::isJumpToNoteEnabled(void) const
{
    return jumpToNote->isChecked();
}

// Прыгать только по первому слову для моста в редактор
bool FindScreen::isJumpFirstWordOnly(void) const
{
    return jumpFirstWord->isChecked();
}

// Слот, срабатывающий при нажатии на кнопку начала поиска
void FindScreen::findClicked(void)
{
    // Поля, где нужно искать (Заголовок, текст, теги...)
    searchArea["name"]    =findInName->isChecked();
    searchArea["author"]  =findInAuthor->isChecked();
    searchArea["url"]     =findInUrl->isChecked();
    searchArea["tags"]    =findInTags->isChecked();
    searchArea["text"]    =findInText->isChecked(); // Поиск в тексте записи
    searchArea["nameItem"]=findInNameItem->isChecked(); // Поиск по именам веток
    searchArea["attach"]  =findInAttach->isChecked(); // Поиск по именам прикрепленных файлов

    // Проверяется, установлено ли хоть одно поле для поиска
    int findEnableFlag=0;
    foreach (bool value, searchArea)
        if(value==true)findEnableFlag=1;

    // Если не отмечены поля для поиска
    if(findEnableFlag==0)
    {
        QMessageBox messageBox(this);
        messageBox.setWindowTitle(tr("Cannot start find process"));
        messageBox.setText(tr("Verify that you selected fields for search for starting find process."));
        messageBox.addButton(tr("OK"),QMessageBox::AcceptRole);
        messageBox.exec();
        return;
    }

    // Выясняется список слов, которые нужно искать.
    // В строгом режиме тега слова не нужны: совпадение проверяется
    // целиком, пустой список не мешает (тег из одних разделителей
    // все равно ищется строго)
    searchWordList=textDelimiterDecompose(findText->text());

    if(searchWordList.size()==0 && !exactTagMode)
    {
        QMessageBox messageBox(this);
        messageBox.setWindowTitle(tr("Can not start find process"));
        messageBox.setText(tr("The search request is too short. Enter at least one word."));
        messageBox.addButton(tr("OK"),QMessageBox::AcceptRole);
        messageBox.exec();
        return;
    }

    findStart();
}

// Есть ли шифрованные записи в поддереве поиска: их отсутствие
// в FTS-индексе требует линейного прохода для корректности.
// Обход только по флагам, без чтения файлов текстов
static bool scopeHasCrypt(const TreeItem *startItem)
{
    if(startItem==nullptr)
        return false;

    QList<const TreeItem *> stack;
    stack.append(startItem);

    while(!stack.isEmpty())
    {
        const TreeItem *item=stack.takeLast();

        if(item->getField(QStringLiteral("crypt"))==QStringLiteral("1"))
            return true;

        if(item->recordtableGetRowCount() > 0)
        {
            const RecordTableData *recordTable=item->recordtableGetTableData();

            for(int i=0; i<static_cast<int>(recordTable->size()); i++)
                if(recordTable->getField(QStringLiteral("crypt"), i)==QStringLiteral("1"))
                    return true;
        }

        for(int i=0; i<item->childCount(); i++)
            stack.append(item->child(i));
    }

    return false;
}


// Снапшот поддерева в плоский вектор в порядке линейного обхода:
// ветка, ее записи, подветки. Только плоские строки: потоки модель
// не трогают. Шифра здесь быть не должно: параллельный путь включается
// только при чистом от шифра scope (см. setupFtsSearch)
void FindScreen::collectSnaps(const TreeItem *curritem,
                              QVector<RecSnap> &snaps,
                              QVector<SnapBranch> &branches,
                              const QHash<QString, QString> &plains,
                              const QSet<QString> &candidates,
                              int &branchSeq)
{
    if(curritem==nullptr || cancelFlag==1)
        return;

    const int myBranch=branchSeq++;

    SnapBranch br;
    br.name=curritem->getField("name");
    br.path=curritem->getPath();
    br.branchId=curritem->getField("id");
    br.matched=false;
    br.firstSnap=snaps.size();

    if(searchArea.value("nameItem", false))
        br.matched=findInTextProcess(br.name);

    branches.append(br);

    if(curritem->recordtableGetRowCount() > 0)
    {
        const RecordTableData *recordTable=curritem->recordtableGetTableData();

        for(int i=0; i<static_cast<int>(recordTable->size()); i++)
        {
            if(cancelFlag==1)
                return;

            RecSnap snap;
            snap.branchSeq=myBranch;
            snap.id=recordTable->getField("id", i);
            snap.name=recordTable->getField("name", i);
            snap.tags=recordTable->getField("tags", i);

            if(searchArea.value("author", false))
                snap.author=recordTable->getField("author", i);

            if(searchArea.value("url", false))
                snap.url=recordTable->getField("url", i);

            if(searchArea.value("attach", false))
            {
                Record *record=const_cast<RecordTableData *>(recordTable)->getRecord(i);

                if(record!=nullptr)
                {
                    AttachTableData *attachTable=record->getAttachTablePointer();

                    QStringList attachNames;

                    for(int a=0; a<attachTable->size(); ++a)
                        attachNames << attachTable->getFileName(a);

                    snap.attachNames=attachNames.join(" ");
                }
            }

            if(searchArea.value("text", false))
            {
                snap.inCandidates=candidates.contains(snap.id);

                if(snap.inCandidates)
                    snap.plain=plains.value(snap.id);
            }

            snaps.append(snap);

            if((snaps.size() % 4096)==0)
            {
                progress->setValue(snaps.size());
                qApp->processEvents();

                if(progress->wasCanceled())
                {
                    cancelFlag=1;
                    return;
                }
            }
        }
    }

    for(int i=0; i<curritem->childCount(); i++)
    {
        collectSnaps(curritem->child(i), snaps, branches, plains, candidates, branchSeq);

        if(cancelFlag==1)
            return;
    }
}


// Параллельный путь поиска при активном FTS: снапшот области,
// оценка чанками в пуле потоков, хиты в таблицу по порядку.
// Прогресс и отмена как в линейном пути
void FindScreen::findParallel(const TreeItem *startItem)
{
    // Параметры поиска фиксируются до потоков: комбобоксы дальше не трогаем
    const bool wholeWord=(howExtract->currentIndex()==0);
    const bool matchAll=(wordRegard->currentIndex()!=0);

    const bool checkName=searchArea.value("name", false);
    const bool checkAuthor=searchArea.value("author", false);
    const bool checkUrl=searchArea.value("url", false);
    const bool checkTags=searchArea.value("tags", false);
    const bool checkAttach=searchArea.value("attach", false);
    const bool checkText=searchArea.value("text", false);

    const QStringList words=searchWordList;

    // Плоские тексты кандидатов одной пачкой вместо тысяч prepare.
    // Нет строки = нет в кеше: проверка чтением файла как раньше
    // (флаг в снапшоте, файл читает GUI-поток при мерже)
    QHash<QString, QString> plains=AppContext::instance().ftsTextIndex()->plainBulk(ftsCandidates);

    // Снапшот области в порядке дерева. Шифра нет (см. setupFtsSearch),
    // отмены из линейки достаточно: дальше только чтение снапшота
    QVector<RecSnap> snaps;
    QVector<SnapBranch> branches;
    int branchSeq=0;
    collectSnaps(startItem, snaps, branches, plains, ftsCandidates, branchSeq);

    if(cancelFlag==1)
        return;

    // Оценка чанками: ожидание с прокачкой и проверкой отмены,
    // хиты мержатся сразу (частичные строки при отмене как в легаси)
    const int chunkSize=4096;
    const int total=snaps.size();

    progress->setRange(0, total);
    progress->setValue(0);

    int done=0;
    int mergeBranch=0;

    for(int from=0; from<total && cancelFlag==0; from+=chunkSize)
    {
        const int count=qMin(chunkSize, total-from);

        QVector<RecSnap> chunk;
        chunk.reserve(count);

        for(int i=0; i<count; i++)
            chunk.append(snaps.at(from+i));

        // Один таск на чанк: меньше оверхеда чем per-item mapped,
        // лямбда для run выводится без result_type
        QFuture< QVector<HitSnap> > future=QtConcurrent::run(
            [chunk, words, wholeWord, matchAll,
             checkName, checkAuthor, checkUrl, checkTags, checkAttach, checkText]
            () -> QVector<HitSnap>
        {
            QVector<HitSnap> out;
            out.reserve(chunk.size());

            for(int i=0; i<chunk.size(); i++)
            {
                const RecSnap &snap=chunk.at(i);

                HitSnap hit;
                hit.branchSeq=snap.branchSeq;

                if(matchSnapRecord(snap, words, wholeWord, matchAll,
                                   checkName, checkAuthor, checkUrl, checkTags,
                                   checkAttach, checkText))
                {
                    hit.id=snap.id;
                    hit.name=snap.name;
                    hit.tags=snap.tags;
                }

                out.append(hit);
            }

            return out;
        });

        QFutureWatcher< QVector<HitSnap> > watcher;
        watcher.setFuture(future);

        while(!watcher.isFinished())
        {
            qApp->processEvents();

            if(progress->wasCanceled())
            {
                watcher.cancel();
                cancelFlag=1;
                break;
            }
        }

        watcher.waitForFinished();

        if(cancelFlag==1)
            break;

        // Мерж чанка: хиты идут в порядке снапшота, ветки по порядку.
        // Ветка печатается перед первой своей записью (знак <=),
        // совпавшая без хитов тоже печатается (как в линейном пути)
        QVector<HitSnap> hits=future.result();

        for(int i=0; i<hits.size(); i++)
        {
            const HitSnap &hit=hits.at(i);

            while(mergeBranch<=hit.branchSeq && mergeBranch<branches.size())
            {
                const SnapBranch &br=branches.at(mergeBranch);

                if(br.matched)
                    findTable->addRow(br.name,
                                      tr("[Tree item]"),
                                      "",
                                      br.path,
                                      br.branchId,
                                      true);

                ++mergeBranch;
            }

            if(!hit.id.isEmpty())
            {
                const SnapBranch &br=branches.at(hit.branchSeq);

                findTable->addRow(hit.name,
                                  br.name,
                                  hit.tags,
                                  br.path,
                                  hit.id);
            }
        }

        done+=count;
        progress->setValue(done);
        qApp->processEvents();

        if(progress->wasCanceled())
            cancelFlag=1;
    }

    // Хвостовые ветки без хитов после последнего чанка
    if(cancelFlag==0)
        while(mergeBranch<branches.size())
        {
            const SnapBranch &br=branches.at(mergeBranch);

            if(br.matched)
                findTable->addRow(br.name,
                                  tr("[Tree item]"),
                                  "",
                                  br.path,
                                  br.branchId,
                                  true);

            ++mergeBranch;
        }
}


void FindScreen::findStart(void)
{
    // Сохраняется текущая редактируемая запись, чтобы и в ней
    // были найдены введенные перед нажатием Find данные, если они есть
    find_object<MainWindow>("mainwindow")->saveTextarea();

    // Очищается таблица результата поиска
    findTable->clearAll();

    // Очищается надпись поверх результата поиска
    findTable->setOverdrawMessage("");

    // Выясняется ссылка на модель дерева данных
    KnowTreeModel *searchModel=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());

    // Выясняется стартовый элемент в дереве, с которого будет начат поиск
    // Выясняется сколько всего конечных записей
    TreeItem const *startItem=0;
    int totalRec=0;
    if(AppContext::instance().appConfig()->findSection()->getFindScreenTreeSearchArea()==0) // Если нужен поиск во всем дереве
    {
        // Корневой элемент дерева
        startItem=searchModel->getRootItem(); // searchModel->rootItem

        // Количество конечных записей во всем дереве
        totalRec=searchModel->getAllRecordCount();
    }
    else if (AppContext::instance().appConfig()->findSection()->getFindScreenTreeSearchArea()==1) // Если нужен поиск в текущей ветке
    {
        // Индекс текущей выбранной ветки
        QModelIndex currentItemIndex=find_object<TreeScreen>("treeScreen")->getCurrentItemIndex();

        // Текущая ветка
        startItem=searchModel->getItem(currentItemIndex);

        // Количество конечных записей в текущей ветке и всех подветках
        totalRec=searchModel->getRecordCountForItem(startItem);
    }

    qDebug() << "Start finding in " << totalRec << " records";

    // Если стартовый элемент не был установлен
    if(startItem==0)
    {
        QMessageBox messageBox(this);
        messageBox.setWindowTitle(tr("Cannot start find process"));
        messageBox.setText(tr("Starting position for a tree searching is not set."));
        messageBox.addButton(tr("OK"),QMessageBox::AcceptRole);
        messageBox.exec();
        return;
    }

    // FTS5-кандидаты текстов: готовятся до линейки поиска.
    // Неудача (нет индекса, короткие слова, шифр в области)
    // молча дает линейный поиск
    ftsActive=false;
    ftsCandidates.clear();

    if(searchArea.value("text", false) && !exactTagMode)
        ftsActive=setupFtsSearch(searchModel, startItem);

    // Динамически создается виджет линейки наполяемости
    progress=new QProgressDialog(this);

    // Корректировка регрессии в Qt 5.5 - 5.6.x QTBUG-47042 QTBUG-47049
    // QProgressDialog показывает сам себя через 4 секунды (точнее, спустя minimumDuration() сек.) после отработки конструктора
    // метод hide() не может скрыть такое окно
    progress->cancel();

    // Показывается виджет линейки наполняемости
    progress->reset();
    progress->setLabelText(tr("Search..."));
    progress->setRange(0,totalRec);
    progress->setModal(true);
    progress->setMinimumDuration(0);
    progress->show(); // Эта команда под вопросом, возможно она не нужна

    // Обнуляется счетчик обработанных конечных записей
    totalProgressCounter=0;
    cancelFlag=0;
    isUnsearchCryptBranchPresent=false;

    //Вызывается рекурсивный поиск в дереве
    if(ftsActive)
    {
        // Параллельный путь: снапшот области и пул потоков.
        // Легаси-обход ниже не выполняется
        findParallel(startItem);
    }
    else
        this->findRecurse( startItem );
    // После вставки всех данных подгоняется ширина колонок
    findTable->updateColumnsWidth();

    // Линейка наполяемости удаляется
    delete progress;

    // Если ничего не было найдено
    if(findTable->getRowCount()==0)
    {
        // Если были непросмотренные ветки по причине того, что они зашифрованы, а пароль не введен
        if(isUnsearchCryptBranchPresent)
            findTable->setOverdrawMessage(tr("No search results.\nHint: encrypted items has been detected. For searching in encrypted items you have to enter the password.")); // Ничего не найдено. При поиске обнаружены зашифрованные ветки. Однако пароль не был введен, поэтому поиск в них не проводился

        // Иначе если поиск проводился не во всей базе
        else if(AppContext::instance().appConfig()->findSection()->getFindScreenTreeSearchArea()!=0)
            findTable->setOverdrawMessage(tr("No search results.\nHint: search produced in current tree item.\nTry to search for entire database.")); // Ничего не найдено. Поиск производился внутри одной ветки. Попробуйте искать во всей базе.
    }

}

// FTS5-индекс текстов: подготовка кандидатного множества перед поиском.
// Возвращает true если кандидаты активны. Пустой индекс, короткие слова
// или шифрованные записи в области поиска дают false: идет линейный поиск
bool FindScreen::setupFtsSearch(KnowTreeModel *searchModel, const TreeItem *startItem)
{
    // Слова короче 3 букв/цифр FTS не берет: только линейный поиск.
    // Проверка та же что в запросе: расхождение дало бы пустых кандидатов
    foreach(const QString &word, searchWordList)
        if(FtsTextIndex::wordGrams(word).isEmpty())
            return false;

    FtsTextIndex *fts=AppContext::instance().ftsTextIndex();

    const int cryptCount=FtsTextIndex::cryptRecordCount(searchModel);

    // Шифрованные записи не индексируются никогда: их наличие
    // в области поиска требует линейного прохода для корректности
    if(cryptCount>0 && scopeHasCrypt(startItem))
        return false;

    // Свежесть индекса против текущей базы
    QString tetradir=AppContext::instance().appConfig()->storageSection()->get_tetradir();
    QFileInfo xmlInfo(QDir(tetradir).absoluteFilePath(QStringLiteral("mytetra.xml")));

    QString reason;

    if(!fts->isFresh(tetradir,
                     xmlInfo.size(),
                     xmlInfo.lastModified().toMSecsSinceEpoch(),
                     searchModel->getAllRecordCount(),
                     cryptCount,
                     &reason))
    {
        // Несвежий или пустой индекс: пересборка с отменой.
        // Отмена молча дает линейный поиск
        if(!rebuildFtsWithProgress(searchModel))
            return false;
    }

    // ИЛИ для wordRegard=0 (любое слово), И для остальных (все слова)
    const bool matchAll=(wordRegard->currentIndex()!=0);

    bool ok=false;
    QSet<QString> candidates=fts->search(searchWordList, matchAll, &ok);

    if(!ok)
        return false;

    ftsCandidates=candidates;

    return true;
}


// Пересборка FTS-индекса с отменяемым диалогом. false при отмене:
// поиск идет линейно
bool FindScreen::rebuildFtsWithProgress(KnowTreeModel *searchModel)
{
    QProgressDialog rebuilding(tr("Building text index..."), tr("Cancel"), 0, 0, this);
    rebuilding.setWindowTitle(tr("Find in base"));
    rebuilding.setMinimumDuration(1000);
    rebuilding.setModal(true);
    rebuilding.show();

    FtsTextIndex *fts=AppContext::instance().ftsTextIndex();

    bool completed=fts->rebuildFull(searchModel, [&](int done, int total) {
        if(rebuilding.maximum()!=total)
            rebuilding.setMaximum(total);

        rebuilding.setValue(done);
        qApp->processEvents();

        return !rebuilding.wasCanceled();
    });

    return completed;
}


void FindScreen::findRecurse(const TreeItem* curritem)
{
    // Если была нажата отмена поиска
    if(cancelFlag==1)return;

    // Если ветка зашифрована, и пароль не был введен
    if(curritem->getField("crypt")=="1" &&
            AppContext::instance().globalParameters()->getCryptKey().length()==0)
    {
        isUnsearchCryptBranchPresent=true;
        return;
    }

    // Имя ветки и путь одни на всю ветку: кэшируются здесь для
    // проверки имени ветки и ниже для строк результатов, чтобы не
    // дергать getField/getPath (с расшифровкой) на каждом хите
    QString branchName;
    bool branchNameReady=false;
    QStringList branchPath;
    bool branchPathReady=false;

    // Проверка имени ветки
    if(searchArea.value("nameItem", false))
    {
        branchName=curritem->getField("name");
        branchNameReady=true;
        bool findItem = findInTextProcess(branchName);
        if(findItem)
        {
            // QString path = curritem->getPathAsNameWithDelimeter(" ");
            // qDebug() << "Find branch succesfull " << path;
            // В таблицу результатов добавляется запись о найденой ветке
            branchPath=curritem->getPath();
            branchPathReady=true;
            findTable->addRow(branchName,
                              tr("[Tree item]"),
                              "",
                              branchPath,
                              curritem->getField("id"),
                              true);
        }
    }

    // Если в ветке присутсвует таблица конечных записей
    if(curritem->recordtableGetRowCount() > 0)
    {
        // Обработка таблицы конечных записей

        // Выясняется ссылка на таблицу конечных записей
        const RecordTableData *searchRecordTable=curritem->recordtableGetTableData();

        // Перебираются записи таблицы
        for(int i=0; i<static_cast<int>( searchRecordTable->size() ); i++)
        {
            ++totalProgressCounter;

            // Линейка и события обновляются пачкой: processEvents()
            // на каждой из тысяч записей дороже самого сравнения строк.
            // Отмена проверяется там же, задержка реакции доли секунды
            if((totalProgressCounter & 15)==0)
            {
                progress->setValue(totalProgressCounter);
                qApp->processEvents();

                if(progress->wasCanceled())
                {
                    cancelFlag=1;
                    return;
                }
            }

            // Совпадение хоть в одном поле: ранний выход, остальные поля
            // (включая чтение файла текста) не трогаются.
            // Имя и теги кэшируются: addRow ниже переиспользует их
            // вместо повторного getField с расшифровкой
            bool found=false;

            QString cachedName;
            QString cachedTags;
            bool cachedNameReady=false;
            bool cachedTagsReady=false;

            // Строгий режим тега: тег целиком без учета регистра.
            // Список не выделяется: нужен только факт наличия
            if(exactTagMode)
            {
                cachedTags=searchRecordTable->getField("tags", i);
                cachedTagsReady=true;
                found=recordHasTag(cachedTags, exactTag);
            }
            else
            {
                // Дешевые поля первыми: совпадение в имени избавляет
                // от чтения файла текста и имен аттачей
                if(searchArea.value("name", false))
                {
                    cachedName=searchRecordTable->getField("name", i);
                    cachedNameReady=true;
                    found=findInTextProcess(cachedName);
                }

                if(!found && searchArea.value("author", false))
                    found=findInTextProcess(searchRecordTable->getField("author", i));

                if(!found && searchArea.value("url", false))
                    found=findInTextProcess(searchRecordTable->getField("url", i));

                if(!found && searchArea.value("tags", false))
                {
                    cachedTags=searchRecordTable->getField("tags", i);
                    cachedTagsReady=true;
                    found=findInTextProcess(cachedTags);
                }

                if(!found && searchArea.value("attach", false))
                {
                    // Поиск по именам прикрепленных файлов записи.
                    // Таблица берется указателем: копирование AttachTableData
                    // по значению оставляет висячие ссылки.
                    // Константность снимается: имена только читаются
                    Record *record=const_cast<RecordTableData *>(searchRecordTable)->getRecord(i);

                    if(record!=nullptr)
                    {
                        AttachTableData *attachTable=record->getAttachTablePointer();

                        QStringList attachNames;

                        for(int a=0; a<attachTable->size(); ++a)
                            attachNames << attachTable->getFileName(a);

                        found=findInTextProcess(attachNames.join(" "));
                    }
                }

                if(!found && searchArea.value("text", false))
                {
                    // Поиск в тексте записи
                    if(ftsActive)
                    {
                        // Кандидаты FTS: вхождение проверяется множеством
                        // без чтения файлов. Совпадение подтверждается обычной
                        // проверкой: семантика та же что в линейном поиске
                        const QString recId=searchRecordTable->getField("id", i);

                        if(ftsCandidates.contains(recId))
                        {
                            // Плоский текст из кеша индекса: без чтения файла
                            // и разбора HTML. Нет в кеше: чтение файла как раньше
                            bool plainOk=false;
                            QString cachedPlain=AppContext::instance().ftsTextIndex()->plainText(recId, &plainOk);

                            if(plainOk)
                                found=findInTextProcess(cachedPlain);
                            else
                            {
                                QString inspectText=searchRecordTable->getText(i);
                                QTextDocument textdoc;
                                textdoc.setHtml(inspectText);
                                found=findInTextProcess(textdoc.toPlainText());
                            }
                        }
                    }
                    else
                    {
                        QString inspectText=searchRecordTable->getText(i);
                        QTextDocument textdoc;
                        textdoc.setHtml(inspectText);
                        found=findInTextProcess(textdoc.toPlainText());
                    }
                }
            }

            // Если запись найдена
            if(found)
            {
                // В таблицу результатов поиска добавляются данные
                // Имя записи
                // Имя ветки
                // Теги
                // Путь к ветке
                // ID записи в таблице конечных записей
                if(!cachedNameReady)
                    cachedName=searchRecordTable->getField("name", i);

                if(!cachedTagsReady)
                    cachedTags=searchRecordTable->getField("tags", i);

                if(!branchNameReady)
                {
                    branchName=curritem->getField("name");
                    branchNameReady=true;
                }

                if(!branchPathReady)
                {
                    branchPath=curritem->getPath();
                    branchPathReady=true;
                }

                findTable->addRow(cachedName,
                                  branchName,
                                  cachedTags,
                                  branchPath,
                                  searchRecordTable->getField("id", i));
            }

        } // Закрылся цикл перебора записей в таблице конечных записей
    } // Закрылось условие что в ветке есть таблица конечных записей

    // Рекурсивная обработка каждой подчиненной ветки
    for(int i=0; i<curritem->childCount(); i++)
        findRecurse(curritem->child(i));

}

// Поиск в переданном тексте
// Учитываются состояния переключателей wordregard и howextract
// Тонкая обертка над общим ядром matchWords (оно же в потоках)
bool FindScreen::findInTextProcess(const QString& text)
{
    return matchWords(text,
                      searchWordList,
                      howExtract->currentIndex()==0,
                      wordRegard->currentIndex()!=0);
}


// Чистая проверка текста по словам запроса. Общее ядро для
// линейного пути и потоков: поведение одно, дублирования нет
bool FindScreen::matchWords(const QString &text,
                            const QStringList &words,
                            bool wholeWord,
                            bool matchAll)
{
    int findWordCount=0;
    int findFlag=0;

    // Токенизация один раз на проверку: раньше текст разбивался
    // на слова под каждое слово запроса, на 108k записях это
    // кратно числу слов в запросе
    const QStringList textWords=wholeWord ? textDelimiterDecompose(text)
                                          : QStringList();

    // Перебираются искомые слова
    for(int i=0; i< words.size(); ++i)
    {
        findFlag=0;

        // Если надо найти совпадение целого слова
        if(wholeWord)
        {
            // Текст уже разбит выше: проверяется, есть ли в полученном
            // списке текущее слово
            if(textWords.contains(words.at(i), Qt::CaseInsensitive))
                findFlag=1;
        }
        else
        {
            // Если надо найти слово как подстроку
            if(text.contains(words.at(i), Qt::CaseInsensitive))
                findFlag=1;
        }

        // Если слово было найдено, количество найденных слов увеличивается
        if(findFlag==1)findWordCount++;

        // Если ищется хотя бы одно совпадение
        if(findFlag==1 && !matchAll)
            return true; // То при первом же совпадении цикл прекращается
    }

    // Искалось хотя бы одно совпадение, но не было найдено
    if(!matchAll) return false;
    else
    {
        // Иначе требовалось найти все слова в запросе
        if( findWordCount==words.size() )
            return true;
        else
            return false;
    }

}


// Проверка записи из снапшота в потоке: только чтение своих полей.
// Порядок полей и ранний выход как в линейном findRecurse
bool FindScreen::matchSnapRecord(const RecSnap &snap,
                                 const QStringList &words,
                                 bool wholeWord,
                                 bool matchAll,
                                 bool checkName,
                                 bool checkAuthor,
                                 bool checkUrl,
                                 bool checkTags,
                                 bool checkAttach,
                                 bool checkText)
{
    bool found=false;

    if(!found && checkName)
        found=matchWords(snap.name, words, wholeWord, matchAll);

    if(!found && checkAuthor)
        found=matchWords(snap.author, words, wholeWord, matchAll);

    if(!found && checkUrl)
        found=matchWords(snap.url, words, wholeWord, matchAll);

    if(!found && checkTags)
        found=matchWords(snap.tags, words, wholeWord, matchAll);

    if(!found && checkAttach)
        found=matchWords(snap.attachNames, words, wholeWord, matchAll);

    if(!found && checkText && snap.inCandidates)
        found=matchWords(snap.plain, words, wholeWord, matchAll);

    return found;
}

void FindScreen::changedWordRegard(int pos)
{
    AppContext::instance().appConfig()->findSection()->set_findscreen_wordregard(pos);
}

void FindScreen::changedHowExtract(int pos)
{
    AppContext::instance().appConfig()->findSection()->set_findscreen_howextract(pos);
}

void FindScreen::changedTreeSearchArea(int pos)
{
    AppContext::instance().appConfig()->findSection()->setFindScreenTreeSearchArea(pos);
}

void FindScreen::changedFindInName(int state)
{
    changedFindInField("name",state);
}

void FindScreen::changedFindInAuthor(int state)
{
    changedFindInField("author",state);
}

void FindScreen::changedFindInUrl(int state)
{
    changedFindInField("url",state);
}

void FindScreen::changedFindInTags(int state)
{
    changedFindInField("tags",state);
}

void FindScreen::changedFindInText(int state)
{
    changedFindInField("text",state);
}

void FindScreen::changedFindInNameItem(int state)
{
    changedFindInField("nameItem",state);
}

void FindScreen::changedFindInAttach(int state)
{
    changedFindInField("attach",state);
}

void FindScreen::changedJumpToNote(int state)
{
    changedFindInField("jumpToNote",state);

    // Зависимая галка активна только при включенном прыжке
    jumpFirstWord->setEnabled(jumpToNote->isChecked());
}

void FindScreen::changedJumpFirstWord(int state)
{
    changedFindInField("jumpFirstWord",state);
}

void FindScreen::changedFindInField(QString fieldname, int state)
{
    bool i;
    if(state==Qt::Checked) i=true;
    else i=false;

    AppContext::instance().appConfig()->findSection()->set_findscreen_find_in_field(fieldname,i);

    // Набор полей для подсказок зависит от галочек
    refreshFieldCompleter();
}

void FindScreen::widgetShow(void)
{
    AppContext::instance().appConfig()->findSection()->set_findscreen_show(true);
    this->show();

    fixCloseButtonSize();

    // При появлении виджета курсор должен сразу стоять на поле ввода
    findText->setFocus();
}

void FindScreen::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    fixCloseButtonSize();

    // Словарь подсказок собирается лениво и только по грязи:
    // теги и названия могли измениться, но пересборка на каждый
    // показ стоила секунд на 100k+ базе.
    // Сборка откладывается за пределы showEvent: showEvent при старте
    // приходит изнутри win.show() (обход showChildren), а сборка
    // создает модальный диалог и качает события - реентрантный обход
    // роняет дерево виджетов (см. 500). Тик задержки незаметен
    if(!completerBuilt || completerDirty)
    {
        completerDirty=true;
        QTimer::singleShot(0, this, [this]() { ensureCompleter(); });
        return;
    }

    ensureCompleter();
}

// Полное сокрытие виджета
void FindScreen::widgetHide(void)
{
    // Запоминается размер сплиттера перед скрытием виджета
    QSplitter *findSplitterRel=find_object<QSplitter>("findsplitter");
    AppContext::instance().appConfig()->uiSection()->set_findsplitter_size_list(findSplitterRel->sizes());

    // Виджет скрывается
    AppContext::instance().appConfig()->findSection()->set_findscreen_show(false);
    this->close();
}

// Слот, срабатывающий при клике на кнопку expand
void FindScreen::toolsExpandClicked(void)
{
    // Если нужно сомкнуть инструменты
    if(findInName->isVisible())
    {
        switchToolsExpand(false);
        AppContext::instance().appConfig()->uiSection()->setFindInBaseExpand(false);
    }
    else
    {
        switchToolsExpand(true);
        AppContext::instance().appConfig()->uiSection()->setFindInBaseExpand(true);
    }
}

void FindScreen::switchToolsExpand(bool flag)
{
    // toolsAreaComboOption->setVisible(flag);
    // whereFindLine->setVisible(flag);

    // Выпадающие списки скрываются в мобильном интерфейсе, так как они на отдельной строке
    if(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="mobile")
    {
        wordRegard->setVisible(flag);
        howExtract->setVisible(flag);
        treeSearchArea->setVisible(flag);
    }

    // Надпись Find in видна и управляется только в desktop режиме интерфейса
    if(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="desktop")
        whereFindLabel->setVisible(flag);

    // Флаги поиска скрываются для любого интерфейса, так как они всегда находятся на отдельной строке
    findInName->setVisible(flag);
    findInAuthor->setVisible(flag);
    findInUrl->setVisible(flag);
    findInTags->setVisible(flag);
    findInText->setVisible(flag);
    findInNameItem->setVisible(flag);
    findInAttach->setVisible(flag);
}

// Устаревшая функция, простое обнаружение токенов для поиска
/*
QStringList FindScreen::textDelimiterDecompose(QString text)
{
 text.replace('"',' ');
 text.replace("'"," ");
 text.replace('.',' ');
 text.replace(',',' ');
 text.replace(';',' ');
 text.replace(':',' ');
 text.replace('-',' ');
 text.replace('?',' ');
 text.replace('!',' ');

 QStringList list = text.split(QRegExp("\\W+"), QString::SkipEmptyParts);

 return list;
}
*/

QStringList FindScreen::textDelimiterDecompose(QString text)
{
    const int len=text.length();

    QStringList list;
    QString buf;
    bool quoted=false;

    QString delimiter=" '.,;:-?!`";
    delimiter.append(QChar::Tabulation);
    delimiter.append(QChar::Nbsp);

    for(int i=0; i<len; i++)
    {
        // Если обнаружен разделитель
        if(delimiter.contains( text[i] ))
        {
            if( !quoted )
            {
                if(buf.length() > 0)
                {
                    list.append(buf);
                    buf = "";
                }
            }
            else
                buf += text[i];
        }
        else if(text[i] == '"')
            quoted = !quoted;
        else
            buf += text[i];
    }

    if(buf.length() > 0)
        list.append(buf);

    qDebug() << "Find split list:" << list;

    return list;
}

// Создание подсказки автодополнения для строки запроса.
// Словарь подставится позже в refreshFieldCompleter
void FindScreen::setupFieldCompleter(void)
{
    // Пауза в наборе перед подсказкой: дополнять на каждую букву
    // на 100k+ базе значит вешать ввод, подсказка только если
    // пользователь остановился на 1000мс
    completerPauseTimer=new QTimer(this);
    completerPauseTimer->setSingleShot(true);
    completerPauseTimer->setInterval(1000);

    connect(completerPauseTimer, &QTimer::timeout,
            this,                &FindScreen::onCompleterPause);

    treeMetadataConnected=false;

    // Словарь подсказок: регистр не важен, модель отсортирована
    // для быстрого поиска. Выпадашка показывает не больше десяти строк.
    // Модель с иконками типов: иконки из ресурсов рисуются везде
    fieldCompleterModel=new QStandardItemModel(this);

    fieldCompleter=new QCompleter(this);
    fieldCompleter->setModel(fieldCompleterModel);
    fieldCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    fieldCompleter->setCompletionMode(QCompleter::PopupCompletion);
    fieldCompleter->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    fieldCompleter->setMaxVisibleItems(10);

    // Совпадение подстрокой а не с начала слова.
    // Словарь уже собран, фильтрация по нему копеечная
    fieldCompleter->setFilterMode(Qt::MatchContains);

    // Только привязка к виджету для позиционирования выпадашки.
    // setCompleter не используется: иначе QLineEdit ищет совпадение
    // всей строки и подсказка после пробела не появляется.
    // Привод полностью ручной из onFindTextEdited
    fieldCompleter->setWidget(findText);
    fieldCompleterEnabled=false;

    connect(findText, &QLineEdit::textEdited,
            this,     &FindScreen::onFindTextEdited);

    connect(fieldCompleter, qOverload<const QString &>(&QCompleter::activated),
            this,          &FindScreen::onFieldCompletion);
}

// Метаданные дерева изменились: словарь помечается грязным,
// пересборка лениво при следующем показе или подсказке
void FindScreen::onTreeMetadataSaved(void)
{
    // Только пометка: словарь пересоберется лениво при следующем
    // показе или подсказке, а не через 300мс посреди пакетной операции.
    // Если панель открыта и текст уже набран - переподсказать через
    // ту же паузу, что при наборе
    completerDirty=true;

    if(isVisible() && findText->hasFocus() && findText->text().trimmed().length()>=2)
        completerPauseTimer->start();
}

// Словарь актуален или пересобирается лениво: при показе панели
// и перед подсказкой. На 100k+ базе сборка это секунды, поэтому
// только по грязи, а не на каждый показ
// Прокачка событий разрешена только когда главное окно уже показано:
// на старте (восстановление состояния до первого show()) прокачка
// из недостроенного UI роняет дерево виджетов (см. 500).
// На старте и так висит стартовый прогресс, крутилка не нужна
static bool canPumpEvents(QWidget *widget)
{
    if(widget==nullptr)
        return false;

    QWidget *win=widget->window();

    return win!=nullptr && win->isVisible();
}


void FindScreen::ensureCompleter(void)
{
    if(completerBuilt && !completerDirty)
        return;

    refreshFieldCompleter();
}


// Прокачка событий во время долгой сборки: крутилке minimumDuration
// нужно успевать показаться, иначе вместо нее висит мертвое окно.
// Только когда главное окно уже показано: на старте (восстановление
// состояния до первого show()) прокачка из недостроенного UI
// роняет дерево виджетов - там свой стартовый прогресс,
// крутилка подсказок не нужна
void FindScreen::pumpWhileBuilding(void)
{
    // См. canPumpEvents выше
    if(canPumpEvents(this))
        qApp->processEvents();
}

// Пересборка словаря подсказок по всем значениям полей,
// отмеченных галочками. Полнотекстовое поле Text не участвует:
// дополнять среди всего текста заметок бессмысленно.
// Долгая операция на 100k+: идет под крутилкой, которая
// показывается только если превышен 1с (minimumDuration)
void FindScreen::refreshFieldCompleter(void)
{
    // Подписка на изменения дерева делается один раз и только когда
    // дерево уже собрано: иначе find_object() на отсутствующем объекте
    // завершает программу. Пока виджет скрыт, пересборку откладываем:
    // словарь и так обновится при показе из showEvent
    if(!treeMetadataConnected)
    {
        TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

        treeMetadataConnected=true;

        connect(treeScreen, &TreeScreen::treeMetadataSaved,
                this,       &FindScreen::onTreeMetadataSaved);
    }

    if(isVisible()==false)
        return;

    // Окно еще не показано (восстановление состояния до первого
    // win.show()): сборку откладываем за старт. Модальный диалог
    // на скрытом родителе и прокачка событий из недостроенного UI
    // роняют дерево виджетов (см. 500). Там и так висит стартовый
    // прогресс, повторный вызов придет из showEvent после показа
    if(!canPumpEvents(this))
    {
        completerDirty=true;
        QTimer::singleShot(0, this, [this]() { ensureCompleter(); });
        return;
    }

    // Защита от реентрантного входа: сборка ниже качает события,
    // повторный ensureCompleter (пауза, метаданные, показ) ждет
    // конца текущей. Иначе выход из программы посреди сборки
    // оставляет висячие указатели модели
    if(completerBuilding)
        return;

    completerBuilding=true;

    // Крутилка на случай долгой сборки: показывается только если
    // превышен 1с, мелкие базы ее не видят. Кнопки отмены нет и
    // крестик окна убран: закрыть недособранный словарь нельзя,
    // сборка конечна (секунды)
    QScopedPointer<QProgressDialog> busy;

    if(canPumpEvents(this))
    {
        busy.reset(new QProgressDialog(tr("Updating suggestions..."), QString(), 0, 0, this));
        busy->setCancelButton(nullptr);
        busy->setWindowFlags(busy->windowFlags() & ~Qt::WindowCloseButtonHint);
        busy->setMinimumDuration(1000);
        busy->setModal(true);
        busy->setWindowTitle(tr("Find in base"));
        busy->show();
    }

    // Словарь собирается по всему дереву: значения из других веток
    // в подсказке безвредны, зато словарь всегда полный и свежий.
    // Модель может отсутствовать если виджет показался раньше
    // готовности дерева: тогда пустой словарь, достроится позже
    QMap<QString, QStringList> dictionaries;
    QSet<QString> seen;

    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

    // Счетчик прокачки событий сквозь рекурсию collectBranchValues
    int pumpCounter=0;

    KnowTreeModel *searchModel=(treeView!=nullptr)
        ? static_cast<KnowTreeModel *>(treeView->model())
        : nullptr;

    if(searchModel!=nullptr && searchModel->getRootItem()!=nullptr)
        collectBranchValues(searchModel->getRootItem(), dictionaries, seen, pumpCounter, this);

    // Подсказка показывает объединение словарей отмеченных полей.
    // Тип значения виден по иконке слева, текст значения чистый
    // без префиксов: дополняется и ищется как есть
    QMap<QString, QStringList> enabledFields;

    if(findInName->isChecked())
        enabledFields["name"]=dictionaries.value("name");

    if(findInAuthor->isChecked())
        enabledFields["author"]=dictionaries.value("author");

    if(findInUrl->isChecked())
        enabledFields["url"]=dictionaries.value("url");

    if(findInTags->isChecked())
        enabledFields["tags"]=dictionaries.value("tags");

    if(findInNameItem->isChecked())
        enabledFields["nameItem"]=dictionaries.value("nameItem");

    if(findInAttach->isChecked())
        enabledFields["attach"]=dictionaries.value("attach");

    fieldCompleterModel->clear();

    // Пары иконка-значение для глобальной сортировки: completer
    // с CaseInsensitivelySortedModel требует отсортированный источник
    QList< QPair<QString, QString> > pairs;

    QMapIterator<QString, QStringList> fieldIt(enabledFields);
    while(fieldIt.hasNext())
    {
        fieldIt.next();

        foreach(QString word, fieldIt.value())
            pairs << qMakePair(fieldIt.key(), word);
    }

    std::sort(pairs.begin(), pairs.end(),
              [](const QPair<QString, QString> &a, const QPair<QString, QString> &b)
              {
                  return a.second.compare(b.second, Qt::CaseInsensitive)<0;
              });

    // Пачка processEvents чтобы крутилка не замирала на сортировке.
    // Внутри sort качать нельзя: компаратор обязан быть чистым.
    // Только когда окно уже показано (см. canPumpEvents выше)
    pumpWhileBuilding();

    int fillCounter=0;

    foreach(auto pair, pairs)
    {
        QStandardItem *item=new QStandardItem(completionTypeIcon(pair.first), pair.second);
        item->setEditable(false);
        fieldCompleterModel->appendRow(item);

        if((++fillCounter % 32768)==0)
            pumpWhileBuilding();
    }

    // Подсказывать нечего: ручной привод выключается
    fieldCompleterEnabled=(fieldCompleterModel->rowCount()>0);

    if(fieldCompleterModel->rowCount()==0)
        fieldCompleter->popup()->hide();

    completerBuilt=true;
    completerDirty=false;
    completerBuilding=false;

    // Словарь обновился под уже набранный запрос (ветка или заметка
    // созданы после ввода): переподсказать без перепечатывания, но только
    // если фокус в поле ввода, иначе выпадашка всплывет над чужой работой.
    // Напрямую дополнением, а не через onFindTextEdited: пауза уже
    // выдержана, повторный таймер не нужен
    if(fieldCompleterEnabled && findText->hasFocus() && !findText->text().trimmed().isEmpty())
        doComplete(findText->text());
}

// Рекурсивный сбор значений полей ветки и всех подветок в словари.
// Ключи словарей совпадают с именами полей поиска
void FindScreen::collectBranchValues(const TreeItem *curritem,
                                     QMap<QString, QStringList> &dictionaries,
                                     QSet<QString> &seen,
                                     int &pumpCounter,
                                     QWidget *pumpWidget)
{
    if(curritem==nullptr)
        return;

    // Зашифрованная ветка без введенного пароля недоступна
    // так же как для самого поиска
    if(curritem->getField("crypt")=="1" &&
       AppContext::instance().globalParameters()->getCryptKey().length()==0)
        return;

    // Имя самой ветки
    addDictionaryWord(dictionaries, seen, "nameItem", curritem->getField("name"));

    // Значения полей всех записей ветки
    if(curritem->recordtableGetRowCount() > 0)
    {
        const RecordTableData *recordTable=curritem->recordtableGetTableData();

        for(int i=0; i<static_cast<int>(recordTable->size()); i++)
        {
            addDictionaryWord(dictionaries, seen, "name", recordTable->getField("name", i));
            addDictionaryWord(dictionaries, seen, "author", recordTable->getField("author", i));
            addDictionaryWord(dictionaries, seen, "url", recordTable->getField("url", i));

            QStringList recordTags=splitRecordTags(recordTable->getField("tags", i));

            for(int t=0; t<recordTags.size(); t++)
                addDictionaryWord(dictionaries, seen, "tags", recordTags.at(t));

            Record *record=const_cast<RecordTableData *>(recordTable)->getRecord(i);

            if(record!=nullptr)
            {
                AttachTableData *attachTable=record->getAttachTablePointer();

                for(int a=0; a<attachTable->size(); ++a)
                    addDictionaryWord(dictionaries, seen, "attach", attachTable->getFileName(a));
            }

            // Долгая сборка качает цикл событий чтобы крутилка
            // minimumDuration успела показаться и не висело окно.
            // Только когда окно уже показано: на старте прокачка
            // из недостроенного UI роняет дерево виджетов (см. 500)
            if((++pumpCounter % 2048)==0 && canPumpEvents(pumpWidget))
                qApp->processEvents();
        }
    }

    // Рекурсивный обход подчиненных веток
    for(int i=0; i<curritem->childCount(); i++)
        collectBranchValues(curritem->child(i), dictionaries, seen, pumpCounter, pumpWidget);
}

// Добавить слово в словарь поля. Пустые значения отбрасываются,
// повторы без учета регистра тоже: пишется первое встречное
// написание. Дедуп в пределах поля: одно и то же слово из разных
// полей показывается с каждой своей меткой типа
void FindScreen::addDictionaryWord(QMap<QString, QStringList> &dictionaries,
                                   QSet<QString> &seen,
                                   const QString &field,
                                   const QString &word)
{
    QString trimmed=word.trimmed();

    if(trimmed.isEmpty())
        return;

    QString key=field+trimmed.toLower();

    if(seen.contains(key))
        return;

    seen.insert(key);
    dictionaries[field].append(trimmed);
}

// Иконка типа значения для выпадашки. Рисуется из ресурсов,
// от шрифтов системы не зависит
QIcon FindScreen::completionTypeIcon(const QString &field)
{
    // Пути иконок типов фиксированы: всего шесть штук.
    // Кеш обязателен: раньше иконка собиралась и растеризовывалась
    // проверкой на каждую из 200k+ строк словаря, что и давало
    // секунды сборки на большой базе
    static QMap<QString, QString> fieldIconPaths;
    static QMap<QString, QIcon> iconCache;
    static bool pathsReady=false;

    if(!pathsReady)
    {
        fieldIconPaths["tags"]=":/resource/pic/tag.svg";
        fieldIconPaths["name"]=":/resource/icons/Flat/color_icons8_flat_document.svg";
        fieldIconPaths["url"]=":/resource/icons/Flat/color_icons8_flat_link.svg";
        fieldIconPaths["nameItem"]=":/resource/icons/Flat/color_icons8_flat_opened_folder.svg";
        fieldIconPaths["author"]=":/resource/icons/Flat/color_icons8_flat_portrait_mode.svg";
        fieldIconPaths["attach"]=":/resource/pic/attach_is_file.svg";
        pathsReady=true;
    }

    if(!fieldIconPaths.contains(field))
        return QIcon();

    if(iconCache.contains(field))
        return iconCache.value(field);

    QIcon icon(fieldIconPaths.value(field));

    // Ресурс не найден или не отрисовался: молчать нельзя, иначе
    // в выпадашке пустые места без понятной причины.
    // Проверка один раз на тип, а не на каждую строку
    if(icon.isNull() || icon.pixmap(16, 16).isNull())
    {
        qWarning() << "FindScreen::completionTypeIcon: bad icon resource" << fieldIconPaths.value(field);
        return QIcon();
    }

    iconCache.insert(field, icon);

    return icon;
}

// Дополняется весь ввод целиком: фишка подсказки найти полное совпадение
// с именем заметки. Выпадашка появляется начиная с двух букв,
// при отсутствии совпадений прячется а не висит пустой
void FindScreen::onFindTextEdited(const QString &text)
{
    if(!fieldCompleterEnabled)
    {
        // Словарь еще ни разу не собирался (первый показ):
        // взвести сборку через паузу, а не вешать ввод
        if(findText->text().trimmed().length()>=2)
            completerPauseTimer->start();

        return;
    }

    if(text.trimmed().length()<2)
    {
        fieldCompleter->popup()->hide();
        completerPauseTimer->stop();
        return;
    }

    // Ввод продолжается: подсказка только после паузы 1000мс.
    // На каждую букву дополнять нельзя: прогон фильтра по 200k+
    // строк вешает набор текста на большой базе
    completerPauseTimer->start();
}


// Пауза в наборе кончилась: словарь свежий (лениво) и подсказать
void FindScreen::onCompleterPause(void)
{
    const QString text=findText->text();

    // Фокус ушел (поиск уже запущен кнопкой): выпадашка поверх
    // чужой работы не нужна
    if(!findText->hasFocus())
        return;

    if(text.trimmed().length()<2)
    {
        fieldCompleter->popup()->hide();
        return;
    }

    ensureCompleter();
    doComplete(text);
}


void FindScreen::doComplete(const QString &text)
{
    if(!fieldCompleterEnabled)
        return;

    QString prefix=text.trimmed();

    fieldCompleter->setCompletionPrefix(prefix);

    if(fieldCompleter->completionCount()==0)
    {
        fieldCompleter->popup()->hide();
        return;
    }

    fieldCompleter->complete();
}

void FindScreen::onFieldCompletion(const QString &completion)
{
    // Выбранное дополнение заменяет весь ввод: в модели лежит
    // чистое значение с иконкой типа, вставляется как есть,
    // дальше сразу запускается поиск
    findText->setText(completion);

    findClicked();
}
