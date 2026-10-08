#include <QDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>
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
#include "views/notePicker/NotePickerDialog.h"
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

    QString href=selectReferenceUnderCursor();

    // Защита от вставки в середину слова (см. snapWordUnderCursor)
    snapWordUnderCursor();

    // Диалог запроса ссылки: два поля, ссылка и текст ссылки.
    // Пустой текст = старое поведение (выделение как есть, иначе имя цели)
    QDialog linkDialog(editor);
    linkDialog.setWindowTitle(tr("Reference or URL"));

    QLineEdit *urlEdit=new QLineEdit(&linkDialog);
    QLineEdit *nameEdit=new QLineEdit(&linkDialog);

    QFormLayout *linkLayout=new QFormLayout(&linkDialog);
    QLabel *urlLabel=new QLabel(tr("Reference or URL"), &linkDialog);
    QLabel *nameLabel=new QLabel(tr("Link text"), &linkDialog);

    // Выбор заметки глобальным поиском: заполняет оба поля,
    // имя не затирается если уже введено вручную
    QHBoxLayout *urlRowLayout=new QHBoxLayout();
    urlRowLayout->addWidget(urlEdit);
    QPushButton *selectNoteButton=new QPushButton(tr("Browse..."), &linkDialog);
    urlRowLayout->addWidget(selectNoteButton);

    linkLayout->addRow(urlLabel, urlRowLayout);
    linkLayout->addRow(nameLabel, nameEdit);

    QObject::connect(selectNoteButton, &QPushButton::clicked,
                     &linkDialog, [this, &linkDialog, urlEdit, nameEdit]() {
      this->pickNoteIntoFields(&linkDialog, urlEdit, nameEdit);
    });

    QDialogButtonBox *linkButtons=new QDialogButtonBox(QDialogButtonBox::Ok |
                                                       QDialogButtonBox::Cancel,
                                                       Qt::Horizontal,
                                                       &linkDialog);
    QObject::connect(linkButtons, &QDialogButtonBox::accepted,
                     &linkDialog, &QDialog::accept);
    QObject::connect(linkButtons, &QDialogButtonBox::rejected,
                     &linkDialog, &QDialog::reject);
    linkLayout->addRow(linkButtons);

    // Ширина как у старого однострочного диалога
    const int dialogWidth=int(0.8*(float)textArea->width());
    linkDialog.setMinimumWidth(dialogWidth);
    linkDialog.resize(linkDialog.size());

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

    urlEdit->setText(href);

    if(textArea->textCursor().hasSelection())
    {
        // Текст ссылки по умолчанию: выделенное (для существующей ссылки
        // выделение уже натянуто на нее выше)
        QString selected=textArea->textCursor().selectedText();
        selected.replace(QChar(0x2029), QStringLiteral(" "));
        nameEdit->setText(selected);
    }
    else if(LinkHelper::isHrefInternal(href))
    {
        const QString targetName=targetRecordName(href);
        if(targetName.isEmpty())
            urlLabel->setText(tr("Reference or URL (record not found)"));
        else
            nameEdit->setText(targetName);
    }

    const bool ok=linkDialog.exec()==QDialog::Accepted;
    const QString refereceUrl=urlEdit->text().trimmed();
    const QString linkText=nameEdit->text();

    if(!ok)
        return;

    // Защита от ссылки на саму себя: в своей же заметке она не имеет смысла.
    // Спрашиваем, отказ по умолчанию
    if(LinkHelper::isHrefInternal(refereceUrl) &&
       !editor->getMiscField(QStringLiteral("id")).isEmpty() &&
       LinkHelper::getIdFromInternalHref(refereceUrl)==editor->getMiscField(QStringLiteral("id")))
    {
        QMessageBox guardBox(QMessageBox::Warning,
                             tr("Self reference"),
                             tr("A link to the note itself makes no sense."),
                             QMessageBox::NoButton,
                             editor);
        guardBox.addButton(tr("Insert anyway"), QMessageBox::AcceptRole);
        guardBox.addButton(QMessageBox::Cancel);
        guardBox.setDefaultButton(QMessageBox::Cancel);

        // Ответ через результат: согласие и программный accept() тоже считаются
        if(guardBox.exec()!=QDialog::Accepted)
            return;
    }

    // Без выделения и с текстом: вставка titled-ссылки с пробелом.
    // Работает для любых ссылок, не только внутренних
    if(!textArea->textCursor().hasSelection() &&
       !refereceUrl.isEmpty() &&
       !linkText.isEmpty())
    {
        insertTitledLink(refereceUrl, linkText);
        return;
    }

    // Вставка внутренней ссылки без выделения и без текста:
    // текст дает имя цели (или голый href для призрака).
    // Старый путь (ссылка на выделение, снятие ссылки) ниже без изменений
    if(!startedOnReference &&
       !textArea->textCursor().hasSelection() &&
       LinkHelper::isHrefInternal(refereceUrl))
    {
        QString display=targetRecordName(refereceUrl);
        if(display.isEmpty())
            display=refereceUrl;

        insertTitledLink(refereceUrl, display);
        return;
    }

    // Текст из поля отличается от выделения: заменить выделение текстом,
    // дальше ссылка вешается на него старым путем
    if(textArea->textCursor().hasSelection() && !linkText.isEmpty())
    {
        QTextCursor cursor=textArea->textCursor();
        QString selected=cursor.selectedText();
        selected.replace(QChar(0x2029), QStringLiteral(" "));

        if(selected!=linkText)
        {
            cursor.insertText(linkText);
            textArea->setTextCursor(cursor);
        }
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


QString ReferenceFormatter::selectReferenceUnderCursor(void)
{
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

    return href;
}


bool ReferenceFormatter::snapWordUnderCursor(void)
{
    if(!textArea->textCursor().hasSelection())
    {
        QTextCursor cursor=textArea->textCursor();
        const int pos=cursor.position();
        QTextDocument *doc=textArea->document();

        if(pos>0 &&
           doc->characterAt(pos-1).isLetterOrNumber() &&
           doc->characterAt(pos).isLetterOrNumber())
        {
            cursor.select(QTextCursor::WordUnderCursor);
            if(!cursor.selectedText().isEmpty())
            {
                textArea->setTextCursor(cursor);
                return true;
            }
        }
    }

    return false;
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


void ReferenceFormatter::pickNoteIntoFields(QDialog *parent,                                            QLineEdit *urlEdit,
                                            QLineEdit *nameEdit)
{
    // Своя запись исключается из выдачи: ссылка на себя бессмысленна
    const QString currentId=editor->getMiscField(QStringLiteral("id"));

    NotePickerDialog picker(parent, currentId);
    if(picker.exec()!=QDialog::Accepted)
        return;

    const QString pickedId=picker.selectedRecordId();
    if(pickedId.isEmpty())
        return;

    urlEdit->setText(FixedParameters::appTextId+
                     QStringLiteral("://note/")+
                     pickedId);

    if(nameEdit->text().isEmpty())
        nameEdit->setText(picker.selectedRecordName());
}


// Редактирование ссылки под курсором: всегда обычный двухполевый диалог
// (он сам натянет выделение и предзаполнит поля), выбор из поиска —
// по кнопке "Обзор" внутри диалога. Протокол тут не важен
void ReferenceFormatter::onEditReferenceAtCursor(void)
{
    onReferenceClicked();
}


// Вставка ссылки через глобальный выбор заметки.
// Своя запись в выдаче отсутствует, защита не срабатывает.
// Середина слова: слово остается якорем, на него вешается ссылка
void ReferenceFormatter::onInsertNoteReferenceClicked(void)
{
    const QString currentId=editor->getMiscField(QStringLiteral("id"));

    NotePickerDialog picker(editor, currentId);
    if(picker.exec()!=QDialog::Accepted)
        return;

    const QString pickedId=picker.selectedRecordId();
    if(pickedId.isEmpty())
        return;

    const QString pickedHref=FixedParameters::appTextId+
                             QStringLiteral("://note/")+
                             pickedId;

    snapWordUnderCursor();

    if(textArea->textCursor().hasSelection())
    {
        // Слово под курсором: оставить слово, повесить ссылку.
        // Тот же исход что у диалога с предзаполненным именем
        QTextCharFormat linkFormat;
        linkFormat.setAnchor(true);
        linkFormat.setAnchorHref(pickedHref);
        linkFormat.setForeground(QApplication::palette().color(QPalette::Link));
        linkFormat.setFontUnderline(true);

        textArea->textCursor().mergeCharFormat(linkFormat);
        return;
    }

    insertTitledLink(pickedHref, picker.selectedRecordName());
}


void ReferenceFormatter::insertTitledLink(const QString &internalHref,
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
