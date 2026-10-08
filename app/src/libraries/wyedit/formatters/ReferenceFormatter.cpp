#include <QInputDialog>
#include <QDebug>
#include <QDesktopServices>
#include <QApplication>
#include <QClipboard>

#include "ReferenceFormatter.h"
#include "SecretFormatter.h"

#include "main.h"
#include "views/mainWindow/MainWindow.h"
#include "views/tree/KnowTreeView.h"
#include "models/tree/KnowTreeModel.h"
#include "models/recordTable/Record.h"
#include "libraries/FixedParameters.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/LinkHelper.h"
#include "../Editor.h"
#include "../EditorConfig.h"
#include "../EditorTextArea.h"
#include "../EditorToolBarAssistant.h"
#include "../EditorCursorPositionDetector.h"
#include "../../TraceLogger.h"


ReferenceFormatter::ReferenceFormatter()
{

}


// Редактирование ссылки
void ReferenceFormatter::onReferenceClicked(void)
{
    // TRACELOG

    QString href="";

    // Если курсор установлен на ссылке
    if(editor->cursorPositionDetector->isCursorOnReference())
        href=editor->cursorPositionDetector->referenceHref(); // Выясняется текст ссылки

    // Если имеется текст ссылки, надо выделить курсором область текста, где эта ссылка находится
    // в случае, если пользователь нажал на кнопку редактирования URL без предварительного выделения
    if(href.size()>0 && !textArea->textCursor().hasSelection()) {
        QTextCursor cursor=textArea->textCursor(); // Создается дополнительный курсор

        // Запоминается позиция курсора
        int cursorPosition=cursor.position();

        // Выясняется, надо ли вообще двигаться влево (не надо, если курсор стоит перед ссылкой, вот так: _|ссылка )
        if(cursor.charFormat().anchorHref()==href) {
            // Движение влево
            do {
                if(!cursor.movePosition(QTextCursor::PreviousCharacter)) {
                    break;
                }
            } while(cursor.charFormat().anchorHref()==href);
        }

        // Запоминается откуда началась ссылка
        int firstCursorPosition=cursor.position();

        // Курсор снова устанавливается на начальную позицию
        cursor.setPosition(cursorPosition);

        // Движение вправо
        bool isRightMoveBreak=false;
        do {
            if (!cursor.movePosition(QTextCursor::NextCharacter)) { // Если достигнут конец текста
                isRightMoveBreak=true;
                break;
            }
        } while(cursor.charFormat().anchorHref()==href);

        // Запоминается где закончилась ссылка
        int secondCursorPosition;
        if(isRightMoveBreak) {
            // Если это конец текста, нужно полное выделение чтобы захватился последний символ
            secondCursorPosition=cursor.position();
        } else {
            // Если это не конец текста, прерывания цикла не было, и нужно исключить последний символ,
            // так как он проверялся в цикле и на последней итерации достиг символа, где ссылки уже не было
            secondCursorPosition=cursor.position()-1;
        }

        // Происходит выделение дополнительным курсором
        cursor.setPosition(firstCursorPosition);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, secondCursorPosition-firstCursorPosition);
        textArea->setTextCursor(cursor); // Дополнительный курсор устанавливается как основной
    }

    // Создание виджета запроса URL с указанием редактора как родительского виджета
    QInputDialog inputDialog(editor);

    // Установка ширины виджета запроса URL
    int dialogWidth=int(0.8*(float)textArea->width());
    inputDialog.setMinimumWidth( dialogWidth );
    inputDialog.resize(inputDialog.size());

    inputDialog.setWindowTitle(tr("Reference or URL"));

    // Умная вставка: курсор не на ссылке, а в буфере внутренняя ссылка.
    // Подставить ее в поле и показать имя цели, чтобы было видно куда сошлемся.
    // Флаг запоминается до подстановки: сам href ниже перезапишется буфером
    const bool startedOnReference=!href.isEmpty();
    if(!startedOnReference)
    {
        const QString clipText=QApplication::clipboard()->text().trimmed();
        if(LinkHelper::isHrefInternal(clipText))
            href=clipText;
    }

    QString targetName;
    if(href.isEmpty())
        inputDialog.setLabelText(tr("Reference or URL"));
    else if(LinkHelper::isHrefInternal(href))
    {
        targetName=targetRecordName(href);
        if(targetName.isEmpty())
            inputDialog.setLabelText(tr("Reference or URL (record not found)"));
        else
            inputDialog.setLabelText(tr("Reference or URL")+" → "+targetName);
    }
    else
        inputDialog.setLabelText(tr("Reference or URL"));

    inputDialog.setTextValue(href);
    inputDialog.setTextEchoMode(QLineEdit::Normal);

    bool ok=inputDialog.exec();
    QString refereceUrl=inputDialog.textValue();

    if(!ok)
        return;

    // Вставка внутренней ссылки без выделения: titled-ссылка
    // с именем цели + пробел, чтобы не править поле вручную.
    // Старый путь (ссылка на выделение, снятие ссылки) ниже без изменений
    if(!startedOnReference &&
       !textArea->textCursor().hasSelection() &&
       LinkHelper::isHrefInternal(refereceUrl))
    {
        insertTitledInternalLink(refereceUrl, targetRecordName(refereceUrl));
        return;
    }

    // Устанавливается текст ссылки
    QTextCharFormat charFormat;
    charFormat.setAnchorHref(refereceUrl);

    // Если текст ссылки задан
    if(refereceUrl.length()>0)
    {
        charFormat.setAnchor(true);
        charFormat.setForeground(QApplication::palette().color(QPalette::Link));
        charFormat.setFontUnderline(true);

        textArea->textCursor().mergeCharFormat(charFormat);
    }
    else
    {
        // Иначе текст ссылки пустой и ссылку надо убрать
        charFormat.setAnchor(false);

        textArea->textCursor().setCharFormat(charFormat);
    }
}


QString ReferenceFormatter::targetRecordName(const QString &internalHref) const
{
    const QString id=LinkHelper::getIdFromInternalHref(internalHref);
    if(id.isEmpty())
        return QString();

    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");
    if(treeView==nullptr)
        return QString();

    Record *record=static_cast<KnowTreeModel *>(treeView->model())->getRecord(id);
    if(record==nullptr)
        return QString();

    return record->getField(QStringLiteral("name"));
}


void ReferenceFormatter::insertTitledInternalLink(const QString &internalHref,
                                                 const QString &targetName)
{
    // Без имени (удалена, шифр): вставляется голый href как раньше,
    // молчаливой вставки мусора нет, видно что именно вставилось
    const QString title=targetName.isEmpty() ? internalHref : targetName;

    QTextCharFormat linkFormat;
    linkFormat.setAnchor(true);
    linkFormat.setAnchorHref(internalHref);
    linkFormat.setForeground(QApplication::palette().color(QPalette::Link));
    linkFormat.setFontUnderline(true);

    QTextCursor cursor=textArea->textCursor();
    cursor.insertText(title, linkFormat);

    // Пробел-разделитель шрифтом окружения без якоря и подчеркивания:
    // рядом стоящие ссылки иначе воспринимаются как одна
    QTextCharFormat spaceFormat=textArea->textCursor().charFormat();
    spaceFormat.setAnchor(false);
    spaceFormat.setAnchorHref(QString());
    spaceFormat.clearForeground();
    spaceFormat.setFontUnderline(false);
    cursor.insertText(QStringLiteral(" "), spaceFormat);

    textArea->setTextCursor(cursor);
}


// Действия при выборе контекстного меню редактора "Перейти по ссылке"
void ReferenceFormatter::onContextMenuGotoReference()
{
    QString href=editor->cursorPositionDetector->referenceHref(); // Текст ссылки

    onClickedGotoReference(href);
}


void ReferenceFormatter::onClickedGotoReference(QString href)
{
    // Клик по спойлеру не навигация: выделение и так показывает содержимое.
    // Метка живет в том же anchor-канале что ссылки
    if(href==SecretFormatter::secretHref())
      return;

    LinkHelper::gotoReference(href);
}


// Слот используется для "открепления" от ссылки, то есть чтобы при нажатии пробела после ссылки, ссылка не продолжала "тянуться"
void ReferenceFormatter::onTextChanged(void)
{
    // TRACELOG

    // Создается дополнительный курсор как копия основного курсора
    QTextCursor cursor=textArea->textCursor();

    // Запоминается его позиция
    int cursorPosition=cursor.position();

    // Выделяется символ слева от курсора
    cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
    QString prevCharacterAsString=cursor.selectedText();
    QChar prevCharacter;
    if(prevCharacterAsString.length()==1)
        prevCharacter=prevCharacterAsString[0];

    // qDebug() << "Prev char: [" << prevCharacter << "]";

    // Если последний символ не является пробелом или символом-разделителем
    if( !prevCharacter.isSpace() )
        return;


    // Дополнительный курсор снова устанавливается на начальную позицию
    cursor.setPosition(cursorPosition);
    cursor.movePosition(QTextCursor::PreviousCharacter); // И смещается на одну позицию назад

    // Наличие форматирования ссылкой у предыдущего, текущего и последующего символа
    bool anchorPrevious=false;
    bool anchorCurrent=false;
    bool anchorNext=false;

    // Выясняется форматирование предыдущего символа
    anchorPrevious=cursor.charFormat().isAnchor();

    // Выясняется форматирование текущего символа
    cursor.movePosition(QTextCursor::NextCharacter);
    anchorCurrent=cursor.charFormat().isAnchor();

    // Выясняется форматирование последующего символа (его может и не быть, если это конец текста)
    bool isNextCharExists=cursor.movePosition(QTextCursor::NextCharacter);
    anchorNext=cursor.charFormat().isAnchor();

    // Если предыдущий и текущий сивол имеют форматирование ссылки, а последующий - обычный
    if(anchorPrevious && anchorCurrent && (!anchorNext || !isNextCharExists))
    {
        QTextCharFormat charFormat;

        // Текущий символ становится обычным
        charFormat.setAnchor(false);
        charFormat.setAnchorHref("");
        charFormat.setForeground(QApplication::palette().color(QPalette::Text));
        charFormat.setFontUnderline(false);

        // Дополнительный курсор снова устанавливается на начальную позицию
        cursor.setPosition(cursorPosition);
        cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);

        cursor.mergeCharFormat(charFormat);
    }

}


// Временно не используется
/*
void ReferenceFormatter::onCursorPositionChanged(void)
{
  // TRACELOG

}


// При изменении документа
void ReferenceFormatter::onContentsChange(int position, int charsRemoved, int charsAdded)
{
  // TRACELOG

}
*/
