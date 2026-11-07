#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QDebug>
#include <QIcon>
#include <QScrollBar>
#include <QEvent>
#include <QCloseEvent>

#include "EditorShowText.h"
#include "EditorShowTextContextMenu.h"
#include "libraries/helpers/DebugHelper.h"
#include "libraries/helpers/ObjectHelper.h"
#include "models/tree/KnowTreeModel.h"
#include "views/tree/KnowTreeView.h"
#include "views/mainWindow/MainWindow.h"
#include "views/record/MetaEditor.h"
#include "models/appConfig/AppConfig.h"

extern AppConfig mytetraConfig;


EditorShowText::EditorShowText(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f)
{
    setupUi();
    setupSignals();
    assembly();
}


EditorShowText::~EditorShowText()
{

}


void EditorShowText::setupUi()
{
    QSizePolicy sizePolicy;
    sizePolicy.setHorizontalPolicy(QSizePolicy::Expanding);
    sizePolicy.setVerticalPolicy(QSizePolicy::Expanding);

    setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint); // Скрывается кнопка с вопросом

    // Задается иконка окна
    QIcon icon = QIcon(":/resource/pic/detach_window_icon.svg");
    this->setWindowIcon( icon );

    // Создается новая область редактора и запоминается указательна него
    mTextArea.reset(new QTextEdit(this));

    // Настраивается область редактора
    mTextArea.get()->setAcceptRichText(true);
    mTextArea.get()->setSizePolicy(sizePolicy);
    mTextArea.get()->setReadOnly(true); // Показываемый текст можно только просматривать
    mTextArea.get()->setContextMenuPolicy(Qt::CustomContextMenu); // Меню определяется в программе

    // Создается контекстное меню
    mContextMenu.reset( new EditorShowTextContextMenu(this) );

    // Мини-кнопка скрепки "поверх всех окон": переключаемая,
    // состояние видно по вдавленности. По умолчанию выключена
    pinButton=new QToolButton(this);
    pinButton->setIcon(QIcon(QStringLiteral(":/resource/pic/pin_always_on_top.svg")));
    pinButton->setCheckable(true);
    pinButton->setChecked(false);
    pinButton->setToolTip(tr("Always on top"));
    pinButton->setFixedSize(22, 22);
    pinButton->setIconSize(QSize(16, 16));
}


void EditorShowText::setupSignals()
{
    // Соединение сигнал-слот чтобы показать контекстное меню по правому клику в редакторе
    connect(mTextArea.data(), &QTextEdit::customContextMenuRequested,
            this, &EditorShowText::onCustomContextMenuRequested);

    connect(mContextMenu.data(), &EditorShowTextContextMenu::gotoNote,
            this, &EditorShowText::onGotoNote);

    connect(pinButton, &QToolButton::toggled,
            this, &EditorShowText::onPinToggled);
}


void EditorShowText::assembly()
{
    QVBoxLayout *mainLayout=new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Полоса со скрепкой справа вверху: пустое место отдается тексту
    QHBoxLayout *topBarLayout=new QHBoxLayout();
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    topBarLayout->addStretch(1);
    topBarLayout->addWidget(pinButton);

    mainLayout->addLayout(topBarLayout);

    // Добавляется область текста
    mainLayout->addWidget( mTextArea.get() );
}


void EditorShowText::setNoteId(const QString &noteId)
{
    mNoteId=noteId;
}


void EditorShowText::setHtml(QString text)
{
    mTextArea.get()->setHtml(text);
}


void EditorShowText::setDocument(QSharedPointer<QTextDocument> pDocument)
{
    // Очистка указателя внутри mTextArea на область документа, куда указывал mTextDocument
    // Данное действие необходимо производить перед вызовом mTextArea.get()->setDocument()
    // так как mTextArea является родителем mTextDocument, а согласно документации
    // в момент выполнения setDocument вначале происходит удаление предыдущего связанного
    // с QTextEdit документа, а потом установка нового документа.
    // Получается, что после удаления предыдущего связанного документа методом setDocument,
    // сырой указатель умного указателя начинает указывать на мусор, а потом в том же setDocument
    // этот сырой указатель используется для установки указателя на связываемый документ,
    // и это приводит к сегфолту. При установке nullptr, похоже, что предыдущий
    // документ не удаляется, и это используется для предотвращения порчи памяти
    // в последующем вызове setDocument(). Надо разобраться дальше
    mTextArea.get()->setDocument( nullptr );

    // Указатель на документ запоминается
    mTextDocument=pDocument;

    // Полученный документ устанавливается как содержимое области редактирования
    if(mTextArea.get()->document()!=mTextDocument.get())
    {
        mTextArea.get()->setDocument( mTextDocument.get() );
        mTextArea.get()->document()->setParent( mTextArea.get() );
    }
}


bool EditorShowText::hasTextSelection()
{
    return  mTextArea.data()->textCursor().hasSelection();
}


int EditorShowText::getTextVerticalScroll()
{
    return mTextArea.data()->verticalScrollBar()->value();
}


void EditorShowText::setTextVerticalScroll(int vScroll)
{
    mTextArea.data()->verticalScrollBar()->setValue(vScroll);
}


void EditorShowText::closeEvent(QCloseEvent *event)
{
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


void EditorShowText::onCustomContextMenuRequested(const QPoint &pos)
{
    qDebug() << "In EditorShowText onCustomContextMenuRequested";

    // Контекстное меню обновляется
    mContextMenu->update();

    // Контекстное меню запускается
    mContextMenu->exec( mTextArea.get()->viewport()->mapToGlobal(pos) );
}


// Переход на запись в интерфейсе MyTetra, текст которой содержится в открепляемом окне
void EditorShowText::onGotoNote()
{
    qDebug() << "Goto note: " << mNoteId;

    // Выясняется ссылка на модель дерева данных
    KnowTreeModel *knowTreeModel=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());

    // Нахождение ветки, в которой лежит запись с указанным идентификатором
    QStringList path=knowTreeModel->getRecordPath(mNoteId);

    find_object<MainWindow>("mainwindow")->setTreeAndRecordtablePositions(path, mNoteId);
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


bool EditorShowText::isPinned(void) const
{
    return mPinned;
}


void EditorShowText::applyNoteColors(void)
{
    // Без инициализированного конфига (голые headless-пробы)
    // читать нечего: поле полностью отдается теме
    if(!mytetraConfig.is_init())
    {
        mTextArea->setStyleSheet(QString());
        return;
    }

    QStringList declarations=MetaEditor::notePaperDeclarations();

    if(declarations.isEmpty())
        mTextArea->setStyleSheet(QString());
    else
        mTextArea->setStyleSheet(declarations.join(" "));
}


/*
// Говорят, что hideEvent/showEvent не относится к свертке/развертке, т.е. "свернутые" окна по-прежнему считаются видимыми.
// Поэтому надо ловить QWindowStateChangeEvent. Но это не подтвердилось в KDE.
// В нем нормально отрабатывают hideEvent/showEvent при сворачивании-разворачивании основного окна
// События hideEvent/showEvent не генерируются в Windows 7 (в других не проверено), но в нем решаемым этим кодом проблеммы нет
void EditorShowText::changeEvent(QEvent *event)
{
  qDebug() << "EditorShowText changeEvent: " << event->type();

  if(event->type() == QEvent::WindowStateChange) {
    QWindowStateChangeEvent* WindowStateChangeEvent = (QWindowStateChangeEvent*)event;
    if(!(WindowStateChangeEvent->oldState() & Qt::WindowMaximized) && isMaximized())
    {
      geomX=x();
      geomY=y();
      geomW=width();
      geomH=height();
    }
  }
}
*/
