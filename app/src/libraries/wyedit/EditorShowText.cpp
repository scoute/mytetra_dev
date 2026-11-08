#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QAction>
#include <QDebug>
#include <QIcon>
#include <QCloseEvent>
#include <QHideEvent>
#include <QShowEvent>

#include "EditorShowText.h"
#include "Editor.h"
#include "EditorTextArea.h"
#include "EditorFindBar.h"
#include "EditorToolBarAssistant.h"
#include "EditorIndentSliderAssistant.h"
#include "indentslider/IndentSlider.h"
#include "EditorShowTextDispatcher.h"
#include "libraries/helpers/DebugHelper.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/GlobalParameters.h"
#include "libraries/ShortcutManager.h"
#include "models/appConfig/AppConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"
#include "views/tree/KnowTreeView.h"
#include "views/record/MetaEditor.h"

extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;
extern ShortcutManager shortcutManager;


EditorShowText::EditorShowText(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f)
{
    setupUi();
    setupSignals();
    assembly();
}


EditorShowText::~EditorShowText()
{

}


// Открыть запись во внутреннем редакторе: тот же движок WyEdit
// что в главном окне, только без тулбаров. Дальше окном владеет
// запись: сейвы, ссылки, голос, оглавление, картинки, поиск - все штатное
void EditorShowText::openNote(const QString &noteId)
{
    mNoteId=noteId;

    // Редактор создается лениво: до первого openNote окно пустое
    ensureEditor();

    // Выясняется ссылка на модель дерева данных
    KnowTreeModel *dataModel=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());

    // Если в дереве нет записи с таким идентификатором, открыть нечего
    if(!dataModel->isRecordIdExists(noteId))
        return;

    QStringList path=dataModel->getRecordPath(noteId);

    if(path.isEmpty())
        return;

    TreeItem *branch=dataModel->getItemById(path.last());

    if(branch==nullptr)
        return;

    RecordTableData *table=branch->recordtableGetTableData();

    if(table==nullptr)
        return;

    const int pos=table->getPosById(noteId);

    if(pos<0)
        return;

    // Перед открытием попытка получения текста записи: создаст файл,
    // если его еще нет (как при клике по записи в главном окне)
    table->getText(pos);

    const QString dir=table->getField(QStringLiteral("dir"), pos);
    const QString file=table->getField(QStringLiteral("file"), pos);
    const QString crypt=table->getField(QStringLiteral("crypt"), pos);

    mBlocked=(table->getField(QStringLiteral("block"), pos)==QStringLiteral("1"));

    const QString fullDir=mytetraConfig.get_tetradir()+
                          QStringLiteral("/base/")+dir;

    // Редактору задаются директория, файл и колбеки чтения-записи -
    // ровно как главному окну при клике по записи
    mEditor->setWorkDirectory(fullDir);
    mEditor->setFileName(file);

    mEditor->setMiscField(QStringLiteral("id"), noteId);
    mEditor->setMiscField(QStringLiteral("title"), table->getField(QStringLiteral("name"), pos));
    mEditor->setMiscField(QStringLiteral("crypt"), crypt);
    mEditor->setMiscField(QStringLiteral("detached"), QStringLiteral("1"));

    mEditor->setSaveCallback(RecordTableData::editorSaveCallback);
    mEditor->setLoadCallback(RecordTableData::editorLoadCallback);

    // Шифрованная при закрытой базе: нечего показывать, редактор
    // просто пустой (как главное окно в том же состоянии)
    if(crypt==QStringLiteral("1") &&
       globalParameters.getCryptKey().length()==0)
        mEditor->setDirFileEmptyReaction(Editor::DIRFILEEMPTY_REACTION_SUPPRESS_ERROR);

    refreshEditability();

    mEditor->loadTextarea();

    setWindowTitle(table->getField(QStringLiteral("name"), pos));

    applyNoteColors();
}


Editor *EditorShowText::editor(void) const
{
    return mEditor;
}


// Создать внутренний редактор без привязки к записи.
// В бою вызывается из openNote, в тестах напрямую
void EditorShowText::ensureEditor(const QString &configFileName)
{
    if(mEditor!=nullptr)
        return;

    mEditor=new Editor(this);

    // Штатная инициализация WyEdit, но без штатной сборки:
    // тулбары в открепленном окне не нужны, слой собирается ниже
    mEditor->initEnableAssembly(false);
    mEditor->initConfigFileName(configFileName.isEmpty()
        ? globalParameters.getWorkDirectory()+QStringLiteral("/editorconf.ini")
        : configFileName);
    mEditor->initEnableRandomSeed(false);
    mEditor->initDisableToolList(QStringList());
    mEditor->init(Editor::WYEDIT_DESKTOP_MODE);

    // Тулбары созданы, но в слой не кладутся и не показываются:
    // хоткеи кнопок мертвы, форматирование только через меню
    mEditor->editorToolBarAssistant->hide();
    mEditor->indentSliderAssistant->setVisible(false);
    mEditor->indentSliderAssistant->getIndentSlider()->hide();

    // Хоткеи которым не хватило кнопок: сохранить и поиск
    saveAction=new QAction(this);
    shortcutManager.initAction(QStringLiteral("editor-save"), saveAction);
    connect(saveAction, &QAction::triggered, this, &EditorShowText::saveDetached);
    addAction(saveAction);

    findAction=new QAction(this);
    shortcutManager.initAction(QStringLiteral("editor-findText"), findAction);
    connect(findAction, &QAction::triggered, this, &EditorShowText::onFindRequested);
    addAction(findAction);

    // Своя сборка вместо штатной Editor::assembly: только полоска
    // поиска и поле текста (друг класса, доступ к textArea прямой)
    QVBoxLayout *editLayout=new QVBoxLayout(mEditor);
    editLayout->setContentsMargins(0, 0, 0, 0);
    editLayout->addWidget(mEditor->findBarWidget());
    editLayout->addWidget(mEditor->textArea);

    mLayout->addWidget(mEditor);

    applyNoteColors();
}


QString EditorShowText::noteId(void) const
{
    return mNoteId;
}


void EditorShowText::setupUi()
{
    setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint); // Скрывается кнопка с вопросом

    // Задается иконка окна
    QIcon icon = QIcon(":/resource/pic/detach_window_icon.svg");
    this->setWindowIcon( icon );

    // Мини-кнопка скрепки "поверх всех окон": переключаемая,
    // состояние видно по вдавленности. По умолчанию выключена
    pinButton=new QToolButton(this);
    // Штатная скрепка из набора Essential: монохром, как остальные
    // кнопки тулбаров, видно и на светлой, и на темной теме
    pinButton->setIcon(QIcon(QStringLiteral(":/resource/icons/Essential/nocolor_essential_77_Essential_Icons_Pin.svg")));
    pinButton->setCheckable(true);
    pinButton->setChecked(false);
    pinButton->setToolTip(tr("Always on top"));
    pinButton->setFixedSize(22, 22);
    pinButton->setIconSize(QSize(16, 16));
}


void EditorShowText::setupSignals()
{
    connect(pinButton, &QToolButton::toggled,
            this, &EditorShowText::onPinToggled);
}


void EditorShowText::assembly()
{
    mLayout=new QVBoxLayout(this);
    mLayout->setContentsMargins(0, 0, 0, 0);

    // Полоса со скрепкой справа вверху, редактор встанет ниже в openNote
    QHBoxLayout *topBarLayout=new QHBoxLayout();
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    topBarLayout->addStretch(1);
    topBarLayout->addWidget(pinButton);

    mLayout->addLayout(topBarLayout);
}


// Сохранить правки в файл записи и подтянуть главное окно с индексами.
// Тихий выход без правок и в read-only: звать можно всегда
void EditorShowText::saveDetached(void)
{
    if(mEditor==nullptr || !mEditable)
        return;

    if(!mEditor->getTextareaModified())
        return;

    // Записи уже нет в дереве (удалена, пришла синхронизация): файл
    // не воскрешаем, правки уходят вместе с окном. Проверка дешевая,
    // только по дереву, без чтения содержимого
    KnowTreeModel *dataModel=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());

    if(dataModel==nullptr || !dataModel->isRecordIdExists(mNoteId))
        return;

    mEditor->saveTextarea();

    // Файл обновился: индексы, статистика и главное окно подтягиваются.
    // Свой сейв окно не перечитывает (инициатор пропускается)
    EditorShowTextDispatcher::instance()->refreshAfterDetachedSave(mNoteId);
}


bool EditorShowText::isModified(void) const
{
    return mEditor!=nullptr && mEditor->getTextareaModified();
}


bool EditorShowText::isEditable(void) const
{
    return mEditable;
}


// Переприменить read-only: заблокированная запись или глобальный
// режим только чтения. Формула один в один как у главного окна
void EditorShowText::refreshEditability(void)
{
    mEditable=false;

    if(mEditor!=nullptr && !mNoteId.isEmpty())
        mEditable=!mBlocked &&
                  !mytetraConfig.get_readOnly();

    if(mEditor!=nullptr)
        mEditor->setTextareaEditable(mEditable);
}


// Каталог записи переименован на диске: перевести внутренний
// редактор на новый путь из живой таблицы
void EditorShowText::refreshWorkDirectory(void)
{
    if(mEditor==nullptr || mNoteId.isEmpty())
        return;

    KnowTreeModel *dataModel=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());

    if(dataModel==nullptr)
        return;

    const QString freshDir=dataModel->recordDirPath(mNoteId);

    if(!freshDir.isEmpty())
        mEditor->refreshWorkDirectory(freshDir);
}


int EditorShowText::getTextVerticalScroll(void) const
{
    if(mEditor==nullptr)
        return 0;

    return mEditor->getScrollBarPosition();
}


void EditorShowText::setTextVerticalScroll(int vScroll)
{
    if(mEditor==nullptr)
        return;

    mEditor->setScrollBarPosition(vScroll);
}


int EditorShowText::cursorPosition(void) const
{
    if(mEditor==nullptr)
        return 0;

    return mEditor->getCursorPosition();
}


void EditorShowText::setCursorPosition(int pos)
{
    if(mEditor==nullptr)
        return;

    mEditor->setCursorPosition(pos);
}


void EditorShowText::closeEvent(QCloseEvent *event)
{
    // Правки уходят в файл записи, дальше главное окно и индексы
    saveDetached();

    emit editorShowTextClose( mNoteId );

    event->accept();
}


void EditorShowText::hideEvent(QHideEvent *event)
{
    int x=this->x();
    int y=this->y();
    int w=this->width();
    int h=this->height();

    qDebug() << "Hide event of EditorShowText dialog, window X " << x << " Y " << y << " W " << w << " H " << h;

    // Если геометрия допустимая
    if( x>=0 && y>=0 && w>0 && h>0 )
    {
        // Запоминается геометрия
        mGeomX=x;
        mGeomY=y;
        mGeomW=w;
        mGeomH=h;
    }

    QWidget::hideEvent(event);
}


void EditorShowText::showEvent(QShowEvent *event)
{
    int x=this->x();
    int y=this->y();
    int w=this->width();
    int h=this->height();

    qDebug() << "Show event of EditorShowText dialog";

    // Бумага по текущей теме: окно живет дольше смены темы,
    // поэтому цвета обновляются при каждом показе
    applyNoteColors();

    // Если была запомнена геометрия окна, устанавливается прежняя геометрия
    if(!(mGeomX==0 && mGeomY==0 && mGeomW==0 && mGeomH==0))
    {
        // Установка геометрии происходит только если текущяя геометрия не совпадает с запомненной
        if(x!=mGeomX ||
           y!=mGeomY ||
           w!=mGeomW ||
           h!=mGeomH)
        {
            this->move(mGeomX, mGeomY);
            this->resize(mGeomW, mGeomH);
        }
    }
    else
    {
        qDebug() << "Previos geometry of EditorShowText dialog is not setted";
        qDebug() << "Strange geometry X " << mGeomX << " Y " << mGeomY << " W " << mGeomW << " H " << mGeomH;
    }

    QWidget::showEvent(event);
}


// Скрепка "поверх всех окон". Смена флагов требует пересоздания
// нативного окна: прячем, меняем флаг, показываем заново.
// Геометрия при этом не теряется: hideEvent/showEvent ее помнят
void EditorShowText::onPinToggled(bool checked)
{
    mPinned=checked;

    hide();
    setWindowFlag(Qt::WindowStaysOnTopHint, checked);
    show();
    raise();
    activateWindow();
}


void EditorShowText::onFindRequested(void)
{
    if(mEditor!=nullptr)
        mEditor->showFindPanel();
}


bool EditorShowText::isPinned(void) const
{
    return mPinned;
}


// Бумага и шрифт как в основном редакторе: те же объявления,
// но прямо на поле внутреннего редактора (друг класса)
void EditorShowText::applyNoteColors(void)
{
    if(mEditor==nullptr)
        return;

    // Без инициализированного конфига (голые headless-пробы)
    // читать нечего: поле полностью отдается теме
    if(!mytetraConfig.is_init())
    {
        mEditor->textArea->setStyleSheet(QString());
        return;
    }

    QStringList declarations=MetaEditor::notePaperDeclarations();

    if(declarations.isEmpty())
        mEditor->textArea->setStyleSheet(QString());
    else
        mEditor->textArea->setStyleSheet(declarations.join(" "));
}
