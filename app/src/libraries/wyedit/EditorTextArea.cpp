#include <QPainter>
#include <QRect>
#include <QTextCursor>
#include <QtGlobal>
#include <QScrollBar>
#include <QUrl>
#include <QEvent>
#include <QAbstractTextDocumentLayout>
#include <QFontMetrics>
#include <QGestureEvent>
#include <QTextDocumentFragment>
#include <QMimeData>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <QPaintEvent>
#include <QTapAndHoldGesture>
#include <QDebug>
#include <QApplication>

#include "EditorTextArea.h"
#include "Editor.h"
#include "../TraceLogger.h"
#include "libraries/GlobalParameters.h"
#include "libraries/VoiceNoteHelper.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "libraries/helpers/ObjectHelper.h"
#include "EditorConfig.h"


extern GlobalParameters globalParameters;


EditorTextArea::EditorTextArea(QWidget *parent) : QTextEdit(parent)
{
    m_flagShowFormatting=false;

    m_flagShowIndentEdge=false;
    m_posIndentEdge=0;

    // Включается генерация событий мышки при ее перемещении, а не только при кликах
    // Эти события нужны, чтобы менять форму курсора при наведении на ссылку при нажатой клавише Ctrl
    m_mouseCursorOverriden=false;
    setMouseTracking(true);

    // Устанавливается фильтр на событие показа или скрытия области прокрутки
    this->verticalScrollBar()->installEventFilter(this);

    // this->setStyleSheet("QTextEdit::table {border:1px solid maroon; border-collapse:collapse;}");

    // Разрешение принимать жест QTapAndHoldGesture
    grabGesture(Qt::TapAndHoldGesture);
}


EditorTextArea::~EditorTextArea(void)
{

}


bool EditorTextArea::getShowFormatting(void)
{
    return m_flagShowFormatting;
}


void EditorTextArea::setShowFormatting(bool i)
{
    m_flagShowFormatting=i;

    viewport()->update();
}


// Обработчик событий, нужен только для QTapAndHoldGesture (долгое нажатие)
bool EditorTextArea::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture)
    {
        // qDebug() << "In gesture event(): " << event << " Event type: " << event->type();
        return gestureEvent(static_cast<QGestureEvent*>(event));
    }

    return QTextEdit::event(event);
}


// Обработчик жестов
// Вызывается из обработчика событий
bool EditorTextArea::gestureEvent(QGestureEvent *event)
{
    // qDebug() << "In gestureEvent()" << event;

    if (QGesture *gesture = event->gesture(Qt::TapAndHoldGesture))
        tapAndHoldGestureTriggered(static_cast<QTapAndHoldGesture *>(gesture));

    return true;
}


// Обработчик жеста TapAndHoldGesture
// Вызывается из обработчика жестов
void EditorTextArea::tapAndHoldGestureTriggered(QTapAndHoldGesture *gesture)
{
    // qDebug() << "In tapAndHoldGestureTriggered()" << gesture;

    if(gesture->state()==Qt::GestureFinished)
        if(globalParameters.getTargetOs()=="android")
            emit tapAndHoldGestureFinished( mapFromGlobal(gesture->position().toPoint()) );
}


bool EditorTextArea::eventFilter(QObject *o, QEvent *e)
{
    // Обработка показа или скрытия области прокрутки редактора
    if(o==this->verticalScrollBar())
        if(e->type()==QEvent::Hide || e->type()==QEvent::Show)
            emit updateIndentlineGeometry();

    return QTextEdit::eventFilter(o, e); // Продолжать обработку событий дальше
}


// Cлот отлавливает нажатия клавиш внутри виджета когда он активен (в фокусе)
void EditorTextArea::keyPressEvent(QKeyEvent *event)
{
    // Если нажата клавиша Ctrl
    if( event->key() == Qt::Key_Control )
    {
        switchReferenceClickMode(true);

        QTextEdit::keyPressEvent(event);
    }
    else if( event->key() == Qt::Key_Tab && !(event->modifiers() & Qt::ShiftModifier) )
    {
        // Tab с многострочным выделением упирает весь блок,
        // иначе штатная вставка табуляции (в таблице — переход по ячейкам)
        if(!isReadOnly() && indentSelectedLines(true))
            return;

        QTextEdit::keyPressEvent(event);
    }
    else if( event->key() == Qt::Key_Backtab )
    {
        // Shift+Tab с многострочным выделением снимает отступ блока,
        // иначе прежний построчный unindent. В read-only правит
        // только штатный путь (то есть ничего)
        if(isReadOnly())
        {
            QTextEdit::keyPressEvent(event);
            return;
        }

        if(indentSelectedLines(false))
            return;

        unindentCurrentLine();
    }
    else
    {
        QTextEdit::keyPressEvent(event);
    }
}


// Shift+Tab снимает один уровень отступа в начале строки:
// символ табуляции либо пробелы шириной в один размер табуляции.
// Курсор и выделение возвращаются на место со сдвигом на снятое
void EditorTextArea::unindentCurrentLine(void)
{
    QTextCursor cursor=textCursor();
    int pos=cursor.position();
    int anchor=cursor.anchor();
    cursor.clearSelection();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    if(cursor.selectedText().startsWith('\t'))
    {
        cursor.removeSelectedText();
        cursor.setPosition(anchor-1);
        cursor.setPosition(pos-1, QTextCursor::KeepAnchor);
    }
    else
    {
        #if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
        qreal tabDistance=tabStopDistance();
        #else
        qreal tabDistance=tabStopWidth();
        #endif
        int spaceWidth=QFontMetrics(currentCharFormat().font()).averageCharWidth();
        int tabSpaces=(spaceWidth>0) ? qMax(1, qRound(tabDistance/spaceWidth)) : 4;
        cursor.clearSelection();
        cursor.movePosition(QTextCursor::StartOfLine);
        int removed=0;
        while(removed<tabSpaces)
        {
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            if(cursor.selectedText().endsWith(' '))
            {
                removed++;
            }
            else
            {
                break;
            }
        }
        if(removed>0)
        {
            cursor.removeSelectedText();
            cursor.setPosition(anchor-removed);
            cursor.setPosition(pos-removed, QTextCursor::KeepAnchor);
        }
        else
        {
            cursor.setPosition(anchor);
            cursor.setPosition(pos, QTextCursor::KeepAnchor);
        }
    }
    setTextCursor(cursor);
}


// Tab/Shift+Tab с многострочным выделением: отступ всего блока.
// Каждая затронутая строка получает (или лишается) один таб в начале.
// Выделение сохраняется поверх того же текста. Одна строка или нет
// выделения — возвращается false, вызывающий идёт штатным путём
bool EditorTextArea::indentSelectedLines(bool indent)
{
    QTextCursor cursor=textCursor();

    if(!cursor.hasSelection())
        return false;

    QTextDocument *doc=document();
    const int selStart=cursor.selectionStart();
    const int selEnd=cursor.selectionEnd();

    QTextBlock first=doc->findBlock(selStart);
    QTextBlock last=doc->findBlock(selEnd);

    // Конец ровно на начале строки: эта строка не тронута выделением
    if(last.position()==selEnd && selEnd>selStart)
        last=last.previous();

    if(!last.isValid() || first==last)
        return false;

    // Затронутые блоки от последнего к первому: позиции ранних строк
    // не съезжают при правке поздних
    QList<QTextBlock> blocks;

    for(QTextBlock block=last; block.isValid() && block.position()>=first.position(); block=block.previous())
        blocks.append(block);

    // Размер таба в пробелах — как в unindentCurrentLine
    #if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    qreal tabDistance=tabStopDistance();
    #else
    qreal tabDistance=tabStopWidth();
    #endif
    int spaceWidth=QFontMetrics(currentCharFormat().font()).averageCharWidth();
    int tabSpaces=(spaceWidth>0) ? qMax(1, qRound(tabDistance/spaceWidth)) : 4;

    cursor.beginEditBlock();

    // Сдвиги границ выделения: начало едет только от первой строки
    // (остальные начинаются позже), конец — от всех затронутых
    int deltaStart=0;
    int deltaEnd=0;

    foreach(const QTextBlock &block, blocks)
    {
        QTextCursor line(block);
        int removed=0;

        if(indent)
        {
            line.insertText(QStringLiteral("\t"));
            removed=-1;
        }
        else
        {
            line.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);

            if(line.selectedText().startsWith('\t'))
            {
                line.removeSelectedText();
                removed=1;
            }
            else
            {
                line.clearSelection();

                int spaces=0;

                while(spaces<tabSpaces)
                {
                    line.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);

                    if(line.selectedText().endsWith(' '))
                        spaces++;
                    else
                        break;
                }

                if(spaces>0)
                {
                    line.removeSelectedText();
                    removed=spaces;
                }
            }
        }

        if(removed==0)
            continue;

        // Вставка: каждая строка двигает конец вправо. Начало едет
        // только если вставка строго до него (первая строка, курсор
        // в начале строки — вставка до якоря, якорь едет)
        // Снятие: симметрично, но удаление ровно на якоре якорь не двигает
        if(indent)
        {
            if(block.position()<=selStart)
                deltaStart+=1;

            deltaEnd+=1;
        }
        else
        {
            if(block.position()<selStart)
                deltaStart-=removed;

            deltaEnd-=removed;
        }
    }

    cursor.endEditBlock();

    cursor.setPosition(selStart+deltaStart);
    cursor.setPosition(selEnd+deltaEnd, QTextCursor::KeepAnchor);
    setTextCursor(cursor);

    return true;
}


// Cлот отлавливает отжатия клавиш внутри виджета когда он активен (в фокусе)
void EditorTextArea::keyReleaseEvent(QKeyEvent *event)
{
    // Если отжата клавиша Ctrl
    if( event->key() == Qt::Key_Control )
        switchReferenceClickMode(false);

    QTextEdit::keyReleaseEvent(event);
}


// Слот принимает глобальные нажатия клавиш. Это позволяет отслеживать нажатия даже если виджет неактивени (не в фокусе)
void EditorTextArea::onGlobalPressKey(int key)
{
    // Если нажата клавиша Ctrl
    if( key == Qt::Key_Control )
        switchReferenceClickMode(true);
}


// Слот принимает глобальные отжатия клавиш. Это позволяет отслеживать отжатия даже если виджет неактивени (не в фокусе)
void EditorTextArea::onGlobalReleaseKey(int key)
{
    // Если отжата клавиша Ctrl
    if( key == Qt::Key_Control )
        switchReferenceClickMode(false);
}


void EditorTextArea::switchReferenceClickMode(bool flag)
{
    if(flag)
    {
        // Сразу нужно проверить, не наведен ли курсор на ссылку, и если наведен, то поменять его вид
        QString href=anchorAt(m_currentMousePosition);
        if( !(href.isEmpty()) )
        {
            qApp->setOverrideCursor(QCursor(Qt::PointingHandCursor));
            m_mouseCursorOverriden = true;

            globalParameters.getStatusBar()->showMessage(href);
            qDebug() << "Cursor href in key event: " << href;
        }
    }
    else
    {
        // Вид курсора сбрасывается на основной. Нужно для того, чтобы курсор поменялся,
        // если мышка в момент отжатия клавиши была наведена на ссылку и курсор был с указательным пальцем
        qApp->restoreOverrideCursor();

        m_mouseCursorOverriden = false;

        globalParameters.getStatusBar()->showMessage("");
    }

    qDebug() << "switchReferenceClickMode: " << flag;
}


void EditorTextArea::mouseMoveEvent(QMouseEvent *event)
{
    m_currentMousePosition=event->pos();

    // Если движение мышкой происходит при нажатой клавише Ctrl
    if( QApplication::keyboardModifiers() & Qt::ControlModifier )
    {
        QString href=anchorAt(m_currentMousePosition);

        // Если курсор мыши находится над ссылкой
        if(!href.isEmpty())
        {
            if(!m_mouseCursorOverriden)
            {
                qApp->setOverrideCursor(QCursor(Qt::PointingHandCursor)); // Меняется форма курсора на указатель клика по ссылке
                m_mouseCursorOverriden = true;
                globalParameters.getStatusBar()->showMessage(href); // Ссылка отображается в строке статуса
                qDebug() << "Cursor href in mouse event: " << href;
            }
        }
        else
        {
            if(m_mouseCursorOverriden)
            {
                qApp->restoreOverrideCursor(); // Воостанавливается обычный курсор
                m_mouseCursorOverriden = false;
                globalParameters.getStatusBar()->showMessage("");
            }
        }
    }
    else
    {
        // Без Ctrl указатель-рука только над картинкой: подсказка
        // что ее можно открыть, клик при этом обычный
        QString imageName=imageAt(m_currentMousePosition);

        // Бабл голосовой заметки: рука и имя файла в статусе,
        // клик играет на месте
        QString voiceHref;

        if(imageName.isEmpty())
            voiceHref=VoiceNoteHelper::bubbleHrefAt(
                document(), cursorForPosition(m_currentMousePosition).position());

        if(!imageName.isEmpty())
        {
            if(!m_mouseCursorOverriden)
            {
                qApp->setOverrideCursor(QCursor(Qt::PointingHandCursor));
                m_mouseCursorOverriden = true;
                globalParameters.getStatusBar()->showMessage(imageName);
            }
        }
        else if(!voiceHref.isEmpty())
        {
            if(!m_mouseCursorOverriden)
            {
                qApp->setOverrideCursor(QCursor(Qt::PointingHandCursor));
                m_mouseCursorOverriden = true;
                globalParameters.getStatusBar()->showMessage(
                    VoiceNoteHelper::fileNameFromHref(voiceHref));
            }
        }
        else if(m_mouseCursorOverriden)
        {
            qApp->restoreOverrideCursor();
            m_mouseCursorOverriden = false;
            globalParameters.getStatusBar()->showMessage("");
        }
    }

    QTextEdit::mouseMoveEvent(event);
}


void EditorTextArea::mousePressEvent(QMouseEvent *event)
{
    // Обычный клик по баблу голосовой заметки играет на месте.
    // Курсор при этом ставится как обычно, дальше сигнал
    if(event->type()==QEvent::MouseButtonPress &&
       event->button()==Qt::LeftButton &&
       (QApplication::keyboardModifiers() & Qt::ControlModifier)==0)
    {
        // Координаты документа: скролл прибавляется, как в imageAt
        const QPointF docPoint(event->pos().x()+horizontalScrollBar()->value(),
                               event->pos().y()+verticalScrollBar()->value());
        const QString voiceHref=VoiceNoteHelper::bubbleHrefAt(
            document(), cursorForPosition(event->pos()).position());

        if(!voiceHref.isEmpty())
        {
            QTextEdit::mousePressEvent(event);

            emit clickedOnVoiceBubble(voiceHref, docPoint);
            return;
        }
    }

    // Если клик происходит вместе с клавишей-модификатором Ctrl
    if( event->type()==QEvent::MouseButtonPress && (QApplication::keyboardModifiers() & Qt::ControlModifier) )
    {
        QString href = this->anchorAt(event->pos());
        if(!href.isEmpty())
        {
            emit clickedOnReference(href);

            // Cобытие игнорируется, чтобы текстовый курсор не переместился в место где сделан клик
            event->ignore();
            return;
        }

        // Ссылки нет: Ctrl+клик по картинке открывает ее во внешней
        // программе. Курсор ставится на картинку чтобы openImage ее нашел,
        // дальше клик обрабатывается как обычный
        if(!imageAt(event->pos()).isEmpty())
        {
            QTextEdit::mousePressEvent(event);

            emit clickOnImage();
            return;
        }

        // Cобытие игнорируется, чтобы текстовый курсор не переместился в место где сделан клик
        event->ignore();
        return;
    }
    else
    {
        qApp->restoreOverrideCursor();
    }

    QTextEdit::mousePressEvent(event);
}


void EditorTextArea::mouseDoubleClickEvent(QMouseEvent *event)
{
    // Если происходит двойной клик по картинке
    if(this->currentCharFormat().isImageFormat()) {
        emit doubleClickOnImage();
    }

    QTextEdit::mouseDoubleClickEvent(event);
}


void EditorTextArea::paintEvent(QPaintEvent *event)
{
    // Отрисовка родительского класса
    QTextEdit::paintEvent(event);

    // В заблокированном виджете ничего дополнительного не рисуется
    if(isEnabled()==false)
        return;


    // Если нужно нарисовать линию какого-нибудь отступа
    if(m_flagShowIndentEdge==true)
    {
        QPainter p(viewport());
        p.setRenderHint(QPainter::Antialiasing,false);
        p.setRenderHint(QPainter::HighQualityAntialiasing,false);

        QPen pen(Qt::darkGray);
        pen.setStyle(Qt::SolidLine);
        p.setPen(pen);

        p.drawLine(m_posIndentEdge, 1,
                   m_posIndentEdge, this->height()-1);
    }


    // Если символы форматирования не нужно рисовать
    if(m_flagShowFormatting==false)
        return;

    // Начало области поиска
    int documentStartFrameSearch=0;

    // Конец области поиска
    QTextCursor cur=textCursor();
    cur.movePosition(QTextCursor::End);
    int documentEndFrameSearch=cur.position();

    int textLenght=documentEndFrameSearch;
    // qDebug() << "Document lenght " << text_lenght;

    int analysePoint=0;
    int startFrameIterationCount=0;

    while(true)
    {
        // Новая проверяемая позиция курсора устанавливается в середину области поиска
        analysePoint=(documentStartFrameSearch+documentEndFrameSearch)/2;
        cur.setPosition(analysePoint);

        // qDebug() << "Start " << document_start_frame_search << " End " << document_end_frame_search << " Analyse position " << analyse_point;

        // Дополнительный курсор смещается на символ влево
        QTextCursor dopCursor=cur;
        bool previosResult=dopCursor.movePosition(QTextCursor::PreviousCharacter);

        // Если смещаться было некуда, значит начало документа
        // и начало видимой области совпадают
        if(previosResult==false)
            break;

        QRect dopCursorRectangle=cursorRect(dopCursor);
        QRect cursorRectangle=cursorRect(cur);

        // Если текущая позиция является границей начала видимой области
        if(dopCursorRectangle.top()<0 && cursorRectangle.top()>=0)
            break;

        // Нужно решить куда переносить границы поиска
        // Если координаты курсора положительные, значит курсор
        // находится "ниже" начала области обзора
        if(cursorRectangle.top()>0)
            documentEndFrameSearch=analysePoint; // Нижняя граница двигается вверх
        else
            documentStartFrameSearch=analysePoint; // Верхняя граница двигается вниз

        // Если границы сомкнулись
        if((documentStartFrameSearch+1)==documentEndFrameSearch ||
                documentStartFrameSearch==documentEndFrameSearch)
        {
            // qDebug() << "Edge eqivalent";
            cur.setPosition(documentStartFrameSearch);
            break;
        }

        startFrameIterationCount++;
    }

    // qDebug() << "Found vision area at position " << cur.position() << " Iterations " << start_frame_iteration_count;

    // Поверх рисуются элементы разметки
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing,false);
    p.setRenderHint(QPainter::HighQualityAntialiasing,false);

    QPen pen(Qt::darkGray);
    pen.setStyle(Qt::SolidLine);
    p.setPen(pen);

    do {

        // Выясняется текущий обрабатываемый символ
        QChar c=document()->characterAt(cur.position());
        char c_ansi=c.toLatin1(); // Ранее было toAscii(), но этот метод теперь не поддерживается в Qt 5.2

        if(c_ansi==' ') // Пробел
        {
            QRect rect = cursorRect( cur );
            int y=(rect.top()+rect.bottom())/2;

            // Если координаты курсора вышли за размер области обзора
            if(y>viewport()->height())break; // Дальше рисовать не нужно

            // Рисуется точка на месте пробела
            p.drawRect(rect.left()+2,y,1,1);
        }
        else if(c_ansi=='\t') // Табуляция
        {
            QRect rect = cursorRect( cur );
            int y=(rect.top()+rect.bottom())/2;

            // Если координаты курсора вышли за размер области обзора
            if(y>viewport()->height())break; // Дальше рисовать не нужно

            // Рисуется стрелка на месте табуляции
            p.drawLine(rect.left()+2,y,rect.left()+10,y);
            p.drawLine(rect.left()+7,y-3,rect.left()+10,y);
            p.drawLine(rect.left()+10,y,rect.left()+7,y+3);
        }
        else if(c.unicode()==QChar::ParagraphSeparator) // Перевод строки (в виде границы между абзацами)
        {
            QRect rect = cursorRect( cur );
            int y1=rect.top()+3;
            int y2=rect.bottom()-3;
            int h=y2-y1;
            int w=h/2+2;

            // Если координаты курсора вышли за размер области обзора
            if(y1>viewport()->height())break; // Дальше рисовать не нужно

            // Рисуется пи на месте перевода строки
            p.drawLine(rect.left()+(w/8)+3,y1,rect.left()+(w/8)+3,y2);
            p.drawLine(rect.left()+w-(w/8)-1,y1,rect.left()+w-(w/8)-1,y2);

            p.drawLine(rect.left()+1,y1,rect.left()+w,y1);
            p.drawLine(rect.left()+1,y1,rect.left()+1,y1+h/4);
        }
        else if(c.unicode()==QChar::LineSeparator) // Перевод строки (в виде <br/>)
        {
            QRect rect = cursorRect( cur );
            int y1=rect.top();
            int y2=(rect.top()+rect.bottom())/2;
            int h=y2-y1;
            int w=h/2+1;

            // Если координаты курсора вышли за размер области обзора
            if(y1>viewport()->height())break; // Дальше рисовать не нужно

            // Рисуется изогнутая стрелка на месте перевода строки
            p.drawLine(rect.left(),   y2, rect.left()+w, y2);
            p.drawLine(rect.left()+w, y2, rect.left()+w, y1);

            p.drawLine(rect.left(), y2, rect.left()+w/2, y2-h/2);
            p.drawLine(rect.left(), y2, rect.left()+w/2, y2+h/2);
        }


        // Проверяется, можно ли сдвинуть дальше курсор
        // проверять нужно, так как movePosition иногда зависает при
        // перемещении курсора
        if(cur.position()>=textLenght) break;

        cur.movePosition(QTextCursor::NextCharacter);
    }
    while( !cur.isNull() );
}


void EditorTextArea::resizeEvent(QResizeEvent *event)
{
    // Отрисовка родительского класса
    QTextEdit::resizeEvent(event);

    // Надо обновить геометрию виджета настройки отступов
    emit updateIndentlineGeometry();
}


// Метод возвращает X-координату курсора в "нуливой" позиции слева
// Это значение используется для работы линейки отступов
int EditorTextArea::getIndentStartedLeft(void)
{
    return this->lineWidth() + (int)this->document()->documentMargin() + 1;
}


// Метод возвращает X-координату курсора в самой крайней правой позиции
// Это значение используется для работы линейки отступов
int EditorTextArea::getIndentStartedRight(void)
{
    return this->lineWidth() + this->viewport()->width() + 1 - (int)this->document()->documentMargin();
}


// Установка видимости линии настройки отступа
void EditorTextArea::showIndentEdge(bool i)
{
    m_flagShowIndentEdge=i;
    viewport()->update();
}


//! Установка X-координаты линии настройки отступа
void EditorTextArea::setIndentEdgePos(int i)
{
    m_posIndentEdge=i;
    viewport()->update();
}


// Мягкая перерисовка содержимого области редактирования
// Нужна для устранения бага QTextEdit, в рендере которого появляются
// пустая строка после изменения цвета фона таблиц или прилежащих к
// таблице областей
void EditorTextArea::softRedraw()
{
    QTextDocument *doc = this->document();

    // Сохраняем курсор и вертикальную прокрутку
    QTextCursor savedCursor = this->textCursor();
    int savedScroll = this->verticalScrollBar()->value();

    QTextCursor cursor(doc);

    // Воркароунд чтобы вставка "измененного" фрагмента в конце метода
    // не попадала в стек Undo, эта команда заменяет beginEditBlock.
    // Только что созданный курсор не имеет предыдущего EditBlock, поэтому
    // изменения будут попадать не в Undo, а игнорироваться
    cursor.joinPreviousEditBlock();

    // Выделяем весь документ
    cursor.select(QTextCursor::Document);

    // Делаем фрагмент из выделения (всего документа)
    QTextDocumentFragment originalFragment(cursor);

    // Создается временный документ из фрагмента
    QTextDocument temporaryDoc;
    temporaryDoc.setHtml(originalFragment.toHtml());


    // Редактируется временный документ
    QTextCursor temporaryCursor(&temporaryDoc);

    temporaryCursor.beginEditBlock();

    // ВСТАВЛЯЕМ фиктивный символ в конец документа
    // Символ должен быть "уникальным", но потом мы его удаляем.
    temporaryCursor.movePosition(QTextCursor::End);
    temporaryCursor.insertText(QString(QChar(0x200B)));   // ZERO-WIDTH SPACE

    // УДАЛЯЕТСЯ фиктивный символ
    temporaryCursor.deletePreviousChar();

    temporaryCursor.endEditBlock();


    // Создаем новый фрагмент из отредактированного документа
    QTextDocumentFragment newFragment(&temporaryDoc);

    // Заменяется выделение (весь документ) самим же фрагментом
    cursor.insertFragment(newFragment);

    cursor.endEditBlock();

    // Восстанавливается курсор и прокрутка
    this->setTextCursor(savedCursor); // Крсора не видно, возможно надо восстанавливать не курсор а позицию курсора
    this->verticalScrollBar()->setValue(savedScroll);

}


//! Метод, определяющий какие типы данных можно вставлять в текст
//! Нужен для того, чтоб сделать вставку картинок
bool EditorTextArea::canInsertFromMimeData(const QMimeData *source) const
{
    if(source->hasImage())
        return true;
    else
        return QTextEdit::canInsertFromMimeData(source); // Это не рекурсия, это вызов метода базового класса
}


// Переопределенная функция, срабатывает при вставке (paste) текста/объектов из буфера обмена
void EditorTextArea::insertFromMimeData(const QMimeData *source)
{
    MimeDataFormat mimeDataFormat=detectMimeDataFormat( source );

    // Голосовые вставки доделываются после асинхронной вставки HTML:
    // файлы докопируются, ссылки материализуются в баблы
    if(source!=nullptr && mimeDataFormat==MimeDataHtml)
    {
        if(source->hasFormat(VoiceNoteHelper::voiceMimeTypeName()))
            pendingVoiceMimes.append(
                source->data(VoiceNoteHelper::voiceMimeTypeName()));
        else if(source->hasHtml() && source->html().contains(QStringLiteral("voicenote://")))
            pendingVoiceMimes.append(QByteArray());
    }

    QTextCursor cursor = this->textCursor();
    QTextDocument *document = this->document();

    // Вставка обычного текста
    if(mimeDataFormat==MimeDataText)
    {
        QString text=source->text();
        cursor.insertText(text);
        return;
    }

    // Вставка HTML-кода
    if(mimeDataFormat==MimeDataHtml)
    {
        QString html=source->html();

        html=QString( html.toUtf8() );

        QByteArray byteArray=html.toUtf8();
        if(byteArray.contains( static_cast<char>(0) ))
        {
            qDebug() << "Problem in HTML code. It has null symbol.";
            qDebug() << "Null symbol pos: " << byteArray.indexOf( static_cast<char>(0) ) << " Text len: " << byteArray.size();
        }

        // Вызов процесса загрузки картинок
        // В конце процесса скачивания будет вызван слот EditorTextArea::onDownloadImagesSuccessfull()
        // в котором будут добавляться к документу скачанные картики и будет вставляться текст
        emit downloadImages(html);

        return;
    }

    // Вставка единичной картинки
    if(mimeDataFormat==MimeDataImage)
    {
        QImage image=qvariant_cast<QImage>(source->imageData());

        // Имя ресурса по настройке формата; картинки с прозрачностью
        // всегда PNG (у JPG нет альфа-канала)
        EditorConfig *editorConfig=find_object<EditorConfig>("editorconfig");
        const QString imageExt=(editorConfig!=nullptr) ? editorConfig->imageExtensionFor(image)
                                                       : QStringLiteral("png");
        QString imageName=getUniqueImageName(imageExt);

        document->addResource(QTextDocument::ImageResource, QUrl(imageName), image);
        cursor.insertImage(imageName);
        return;
    }
}


EditorTextArea::MimeDataFormat EditorTextArea::detectMimeDataFormat(const QMimeData *source)
{
    qDebug() << "DetectMimeDataFormat";

    // Однозначно текст
    if(source->hasText() && !source->hasHtml() && !source->hasImage())
        return MimeDataText;

    // Однозначно HTML
    if(!source->hasText() && source->hasHtml() && !source->hasImage())
        return MimeDataHtml;

    // Однозначно картинка
    if(!source->hasText() && !source->hasHtml() && source->hasImage())
        return MimeDataImage;

    // Есть текст, есть HTML, нет картинки = HTML
    if(source->hasText() && source->hasHtml() && !source->hasImage())
        return MimeDataHtml;

    // Есть текст, есть HTML и есть картинка = HTML (таблица из Calc, Excel)
    if(source->hasText() && source->hasHtml() && source->hasImage())
        return MimeDataHtml;

    // Нет текста, есть HTML и есть картинка = картинка
    if(!source->hasText() && source->hasHtml() && source->hasImage())
        return MimeDataImage;

    // В оставшихся случаях
    return MimeDataHtml;
}


// Обработка момента после загрузки всех картинок при вставке HTML-кода
void EditorTextArea::onDownloadImagesSuccessfull(const QString html,
                                                 const QMap<QString, QByteArray> referencesAndMemoryFiles,
                                                 const QMap<QString, QString> referencesAndInternalNames)
{
    QString fixedHtml=html;

    // К документу добавляются скачанные картинки в виде ресурсов
    foreach (QString imageReference, referencesAndMemoryFiles.keys())
    {
        QImage image;
        bool result=image.loadFromData( referencesAndMemoryFiles.value(imageReference) );

        // Если данные с картинкой правильные, и с ней может работать Qt
        if(result)
        {
            QString internalName=referencesAndInternalNames.value(imageReference);

            // Скачанная картинка с прозрачностью под jpg-именем:
            // имя меняется на png прямо в HTML, иначе прозрачность
            // потерялась бы при сохранении
            if(!internalName.endsWith(QStringLiteral(".png")) &&
               !image.isNull() && image.hasAlphaChannel())
            {
                const QString pngName=internalName.section('.', 0, -2)+QStringLiteral(".png");

                fixedHtml.replace(internalName, pngName);
                internalName=pngName;
            }

            // Картинка добавляется в ресурсы документа со внутренним именем
            this->document()->addResource(QTextDocument::ImageResource,
                                          QUrl( internalName ),
                                          QVariant(image));
        }
    }

    // Вставляется HTML-код
    qDebug() << "Insert source HTML text: " << fixedHtml;

    // Код вставляется не напрямую, а пропускается через временный документ,
    // чтобы вставить более понятный для QTextEdit HTML-код.
    // Шрифт по умолчанию у временного документа выставляется как у редактора:
    // иначе расхождение базовых шрифтов роняет семейство шрифта
    // вставляемого фрагмента (баг: вставка меняла шрифт на системный)
    QTextDocument htmlFilterDoc;
    htmlFilterDoc.setDefaultFont(document()->defaultFont());
    htmlFilterDoc.setHtml(fixedHtml);

    qDebug() << "Insert filtered HTML text: " << htmlFilterDoc.toHtml();

    this->textCursor().insertHtml( htmlFilterDoc.toHtml() );

    // Доделка голосовой вставки: файлы из другой заметки,
    // материализация баблов
    if(!pendingVoiceMimes.isEmpty())
        finishVoicePaste(pendingVoiceMimes.takeFirst());
}


QStringList EditorTextArea::voiceHrefsInRange(int from, int to) const
{
    QStringList hrefs;

    if(from>to)
        qSwap(from, to);

    QTextDocument *doc=document();

    if(doc==nullptr)
        return hrefs;

    for(QTextBlock block=doc->firstBlock(); block.isValid(); block=block.next())
        for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
        {
            const QTextFragment fragment=it.fragment();

            if(!fragment.isValid())
                continue;

            const int start=fragment.position();
            const int end=start+fragment.length();

            if(end<=from || start>=to)
                continue;

            const QTextCharFormat format=fragment.charFormat();
            QString href;

            if(VoiceNoteHelper::isVoiceHref(format.anchorHref()))
                href=format.anchorHref();
            else if(VoiceNoteHelper::isBubbleFormat(format))
                href=format.property(VoiceNoteHelper::BubblePropHref).toString();

            if(!href.isEmpty() && !hrefs.contains(href))
                hrefs.append(href);
        }

    return hrefs;
}


QString EditorTextArea::voiceWorkDirectory(void) const
{
    const Editor *editor=qobject_cast<const Editor*>(parent());

    if(editor==nullptr)
        return QString();

    return const_cast<Editor*>(editor)->getWorkDirectory();
}


QString EditorTextArea::uniqueVoiceFileName(const QString &dir, const QString &baseName)
{
    if(!QFileInfo(QDir(dir).absoluteFilePath(baseName)).isFile())
        return baseName;

    const int dotPos=baseName.lastIndexOf(QLatin1Char('.'));
    const QString stem=(dotPos>0) ? baseName.left(dotPos) : baseName;
    const QString ext=(dotPos>0) ? baseName.mid(dotPos) : QString();

    for(int attempt=2; attempt<1000; ++attempt)
    {
        const QString candidate=stem+QStringLiteral("-")+QString::number(attempt)+ext;

        if(!QFileInfo(QDir(dir).absoluteFilePath(candidate)).isFile())
            return candidate;
    }

    return QString();
}


QMimeData *EditorTextArea::createMimeDataFromSelection(void) const
{
    QTextCursor cursor=textCursor();

    if(!cursor.hasSelection())
        return QTextEdit::createMimeDataFromSelection();

    const QStringList hrefs=voiceHrefsInRange(cursor.selectionStart(),
                                              cursor.selectionEnd());

    if(hrefs.isEmpty())
        return QTextEdit::createMimeDataFromSelection();

    // Копия выделения во временный документ: баблы подменяются
    // ссылками, иначе в буфер уедет пустой плейсхолдер
    QTextDocument tmp;
    tmp.setDefaultFont(document()->defaultFont());

    QTextCursor tmpCursor(&tmp);
    tmpCursor.insertFragment(cursor.selection());
    VoiceNoteHelper::dematerializeBubbles(&tmp);

    QMimeData *mime=new QMimeData();
    mime->setHtml(tmp.toHtml("UTF-8"));
    mime->setText(tmp.toPlainText());

    // Абсолютные пути файлов для переноса в другую заметку
    const QString workDir=voiceWorkDirectory();
    QStringList lines;

    for(const QString &href : hrefs)
    {
        const QString absPath=VoiceNoteHelper::resolveFilePath(workDir, href);

        if(!absPath.isEmpty() && QFileInfo(absPath).isFile())
            lines << href << absPath;
    }

    if(!lines.isEmpty())
        mime->setData(VoiceNoteHelper::voiceMimeTypeName(),
                      lines.join(QStringLiteral("\n")).toUtf8());

    return mime;
}


void EditorTextArea::finishVoicePaste(const QByteArray &voiceMime)
{
    // Пары href-путь из буфера обмена
    QMap<QString, QString> srcPaths;
    const QStringList lines=QString::fromUtf8(voiceMime).split(
        QStringLiteral("\n"), Qt::SkipEmptyParts);

    for(int i=0; i+1<lines.size(); i+=2)
        srcPaths.insert(lines.at(i), lines.at(i+1));

    const QString workDir=voiceWorkDirectory();

    QTextCursor cursor=textCursor();
    cursor.beginEditBlock();

    if(!workDir.isEmpty() && QDir(workDir).exists() && !srcPaths.isEmpty())
    {
        // Позиции собираются заранее: правка форматов во время
        // итерации сбивает итератор и пропускает фрагменты
        struct VoiceLinkPos
        {
            int pos;
            int len;
            QString href;
        };

        QList<VoiceLinkPos> linkPositions;

        for(QTextBlock block=document()->firstBlock(); block.isValid(); block=block.next())
            for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
            {
                const QTextFragment fragment=it.fragment();

                if(!fragment.isValid())
                    continue;

                const QString href=fragment.charFormat().anchorHref();

                if(VoiceNoteHelper::isVoiceHref(href))
                    linkPositions.append({fragment.position(), fragment.length(), href});
            }

        // Файла нет в каталоге заметки значит приехал из другой:
        // докопировать. Имя занято чужим файлом значит дописать
        // со свободным именем и починить ссылку
        for(const VoiceLinkPos &link : linkPositions)
        {
            const QString href=link.href;
            const QString dstPath=VoiceNoteHelper::resolveFilePath(workDir, href);

            if(dstPath.isEmpty())
                continue;

            const QString srcPath=srcPaths.value(href);
            const bool dstExists=QFileInfo(dstPath).isFile();
            const bool srcExists=!srcPath.isEmpty() && QFileInfo(srcPath).isFile();

            if(dstExists)
            {
                // Свой файл на месте или источник неизвестен: делать нечего
                if(!srcExists)
                    continue;

                if(QFileInfo(srcPath).absoluteFilePath()==
                   QFileInfo(dstPath).absoluteFilePath())
                    continue;
            }
            else if(!srcExists)
                continue;

            QString fileName=VoiceNoteHelper::fileNameFromHref(href);

            if(dstExists)
            {
                // Чужой файл с тем же именем: свободное имя
                fileName=uniqueVoiceFileName(workDir, fileName);

                if(fileName.isEmpty())
                    continue;
            }

            if(!QFile::copy(srcPath, QDir(workDir).absoluteFilePath(fileName)))
                continue;

            if(dstExists)
            {
                // Ссылка правится на новое имя
                const QString newHref=VoiceNoteHelper::makeHref(fileName);

                QTextCharFormat linkFormat;
                linkFormat.setAnchor(true);
                linkFormat.setAnchorHref(newHref);

                QTextCursor fix(document());
                fix.setPosition(link.pos);
                fix.setPosition(link.pos+link.len, QTextCursor::KeepAnchor);
                fix.mergeCharFormat(linkFormat);
            }
        }
    }

    VoiceNoteHelper::materializeVoiceLinks(document());

    cursor.endEditBlock();
}


// Имя картинки под точкой документа. Координаты мыши переводятся
// в координаты документа с учетом прокрутки
QString EditorTextArea::imageAt(const QPoint &point)
{
    int scrollY=verticalScrollBar()->value();
    int scrollX=horizontalScrollBar()->value();
    QPoint documentPoint(point.x()+scrollX, point.y()+scrollY);

    return document()->documentLayout()->imageAt(documentPoint);
}

