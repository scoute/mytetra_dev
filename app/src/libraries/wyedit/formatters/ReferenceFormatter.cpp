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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextBlock>
#include <QUrl>
#include <QApplication>
#include <QClipboard>

#include "ReferenceFormatter.h"
#include "SecretFormatter.h"

#include "main.h"
#include "views/mainWindow/MainWindow.h"
#include "views/tree/KnowTreeView.h"
#include "models/tree/KnowTreeModel.h"
#include "models/recordTable/Record.h"
#include "models/attachTable/AttachTableData.h"
#include "models/attachTable/Attach.h"
#include "controllers/attachTable/AttachTableController.h"
#include "libraries/GlobalParameters.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/FixedParameters.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/LinkHelper.h"
#include "libraries/helpers/DiskHelper.h"
#include "libraries/crypt/CryptService.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "libraries/VoiceNoteHelper.h"
#include "views/tree/TreeScreen.h"
#include "views/voiceNote/VoicePlayerDialog.h"
#include "views/notePicker/NotePickerDialog.h"
#include "../Editor.h"
#include "../EditorConfig.h"
#include "../EditorTextArea.h"
#include "../EditorToolBarAssistant.h"
#include "../EditorCursorPositionDetector.h"
#include "../../TraceLogger.h"


extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;


ReferenceFormatter::ReferenceFormatter()
{

}


// Редактирование ссылки
void ReferenceFormatter::onReferenceClicked(void)
{
    // TRACELOG

    QString href=selectReferenceUnderCursor();

    // Защита от вставки в середину слова (см. moveCursorToWordEnd)
    moveCursorToWordEnd();

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
        // Ответ через нажатую кнопку: exec() возвращает StandardButton
        // (для кастомной кнопки — ее роль, а не QDialog::Accepted),
        // поэтому сравнение только через clickedButton()
        QPushButton *insertAnywayButton=guardBox.addButton(tr("Insert anyway"), QMessageBox::AcceptRole);
        guardBox.addButton(QMessageBox::Cancel);
        guardBox.setDefaultButton(QMessageBox::Cancel);

        // Ответ считывается с нажатой кнопки: exec() для кастомной
        // кнопки возвращает ее роль, а не QDialog::Accepted, поэтому
        // сравнивать результат exec() с Accepted нельзя — только
        // clickedButton() с указателем кнопки "Вставить всё равно"
        guardBox.exec();

        if(guardBox.clickedButton()!=insertAnywayButton)
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
        charFormat.setForeground(linkColorForHref(refereceUrl));
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


bool ReferenceFormatter::moveCursorToWordEnd(void)
{
    if(textArea->textCursor().hasSelection())
        return false;

    QTextCursor cursor=textArea->textCursor();
    const int pos=cursor.position();
    QTextDocument *doc=textArea->document();

    if(pos>0 &&
       doc->characterAt(pos-1).isLetterOrNumber() &&
       doc->characterAt(pos).isLetterOrNumber())
    {
        cursor.movePosition(QTextCursor::EndOfWord);
        textArea->setTextCursor(cursor);
        return true;
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

    // Середина слова: курсор в конец слова, слово целое,
    // ссылка со своим именем ложится рядом
    moveCursorToWordEnd();

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
    linkFormat.setForeground(linkColorForHref(internalHref));
    linkFormat.setFontUnderline(true);

    QTextCursor cursor=textArea->textCursor();

    // Ведущий пробел только если слева не начало строки и не пробел:
    // в начале строки он давал бы висячий отступ. Замыкающий всегда:
    // ссылка никогда не приклеится к продолжению набора.
    // Двойной пробел при готовом разделителе безвреден и виден сразу
    QTextCharFormat edgeFormat=textArea->textCursor().charFormat();
    edgeFormat.setAnchor(false);
    edgeFormat.setAnchorHref(QString());
    edgeFormat.clearForeground();
    edgeFormat.setFontUnderline(false);

    bool needLeading=true;
    if(cursor.atBlockStart())
        needLeading=false;
    else if(cursor.position()>0)
    {
        const QChar prev=textArea->document()->characterAt(cursor.position()-1);
        if(prev.isSpace() || prev.unicode()==0x2029)
            needLeading=false;
    }

    if(needLeading)
        cursor.insertText(QStringLiteral(" "), edgeFormat);

    cursor.insertText(title, linkFormat);
    cursor.insertText(QStringLiteral(" "), edgeFormat);

    textArea->setTextCursor(cursor);
}


// Метка брошенного в текст файла: [EXT] видимое имя.
// Только ASCII: шрифт заметки настраивается, эмодзи-скрепка
// в DejaVu Sans отсутствует и рисуется tofu-квадратом.
// Расширение верхним регистром, без него метка [FILE]
void ReferenceFormatter::insertAttachBadge(const QString &innerFileName,
                                          const QString &displayFileName)
{
    if(innerFileName.isEmpty() || displayFileName.isEmpty())
        return;

    QString ext=QFileInfo(displayFileName).suffix().toUpper();

    if(ext.isEmpty())
        ext=QStringLiteral("FILE");

    const QString href=QStringLiteral("attach://")+innerFileName;
    const QString title=QStringLiteral("[")+ext+QStringLiteral("] ")+displayFileName;

    insertTitledLink(href, title);
}


// Метка аттача: attach://<внутреннее имя на диске>
bool ReferenceFormatter::isAttachHref(const QString &href)
{
    return href.startsWith(QStringLiteral("attach://"));
}


// Внутреннее имя из метки аттача, пусто если не метка
QString ReferenceFormatter::attachInnerFromHref(const QString &href)
{
    if(!isAttachHref(href))
        return QString();

    return href.mid(static_cast<int>(sizeof("attach://")-1));
}


// Имя MIME для переноса меток аттачей между заметками
const char *ReferenceFormatter::attachMimeTypeName(void)
{
    static const char *attachMimeType="application/x-mytetra-attach";

    return attachMimeType;
}


// Абзац метки для дописки в текст закрытой заметки.
// Тот же вид что у insertAttachBadge: ссылка плюс [EXT] имя
QString ReferenceFormatter::badgeParagraphHtml(const QString &innerName,
                                              const QString &displayName)
{
    if(innerName.isEmpty() || displayName.isEmpty())
        return QString();

    QString ext=QFileInfo(displayName).suffix().toUpper();

    if(ext.isEmpty())
        ext=QStringLiteral("FILE");

    const QString href=QStringLiteral("attach://")+innerName;
    const QString title=QStringLiteral("[")+ext+QStringLiteral("] ")+displayName;

    return QStringLiteral("<p><a href=\"")+
        href.toHtmlEscaped()+QStringLiteral("\">")+
        title.toHtmlEscaped()+QStringLiteral("</a></p>");
}


// Дописка метки в текстовый файл закрытой заметки: вставка перед
// </body>, файла нет — минимальный каркас. Как голосовые ссылки
bool ReferenceFormatter::appendAttachBadgeToTextFile(const QString &textFileName,
                                                     const QString &innerName,
                                                     const QString &displayName,
                                                     QString *error)
{
    const QString paragraph=badgeParagraphHtml(innerName, displayName);

    if(paragraph.isEmpty())
    {
        if(error!=nullptr)
            *error=tr("Empty attach badge");

        return false;
    }

    if(textFileName.isEmpty())
    {
        if(error!=nullptr)
            *error=tr("Target note text file unknown");

        return false;
    }

    if(!QFileInfo(textFileName).isFile())
    {
        QFile out(textFileName);

        if(!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            if(error!=nullptr)
                *error=tr("Can not write target note: %1").arg(textFileName);

            return false;
        }

        out.write((QStringLiteral("<html><body>")+paragraph+
                   QStringLiteral("</body></html>")).toUtf8());

        return true;
    }

    QFile in(textFileName);

    if(!in.open(QIODevice::ReadOnly))
    {
        if(error!=nullptr)
            *error=tr("Can not read target note: %1").arg(textFileName);

        return false;
    }

    QString html=QString::fromUtf8(in.readAll());
    in.close();
    html.remove(QChar('\0'));

    int pos=html.lastIndexOf(QStringLiteral("</body>"), -1, Qt::CaseInsensitive);

    if(pos<0)
        pos=html.lastIndexOf(QStringLiteral("</html>"), -1, Qt::CaseInsensitive);

    if(pos>=0)
        html.insert(pos, paragraph);
    else
        html+=paragraph;

    QFile out(textFileName);

    if(!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if(error!=nullptr)
            *error=tr("Can not write target note: %1").arg(textFileName);

        return false;
    }

    out.write(html.toUtf8());

    return true;
}


// Метка аттача под курсором, пусто если курсор не на метке
QString ReferenceFormatter::attachHrefUnderCursor(void)
{
  if(editor->cursorPositionDetector->isCursorOnReference())
  {
    const QString href=editor->cursorPositionDetector->referenceHref();

    if(isAttachHref(href))
      return href;
  }

  return QString();
}


// Выделить первую метку с href в документе для удаления из исходника.
// Поиск по всему документу, а не от курсора: между открытием меню
// и срабатыванием пункта каретка могла уехать
bool ReferenceFormatter::selectAttachBadgeRange(const QString &href)
{
  if(href.isEmpty())
    return false;

  QTextDocument *doc=textArea->document();

  for(QTextBlock block=doc->firstBlock(); block.isValid(); block=block.next())
    for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
    {
      const QTextFragment fragment=it.fragment();

      if(!fragment.isValid())
        continue;

      if(fragment.charFormat().anchorHref()!=href)
        continue;

      int end=fragment.position()+fragment.length();

      // Хвостовой пробел от вставки забирается тоже чтобы не мусорить
      if(end<doc->characterCount()-1 && doc->characterAt(end)==QLatin1Char(' '))
        ++end;

      QTextCursor select(doc);
      select.setPosition(fragment.position());
      select.setPosition(end, QTextCursor::KeepAnchor);
      textArea->setTextCursor(select);

      return true;
    }

  return false;
}


// Перемещение метки аттача в другую заметку: файл копируется
// в каталог приемника и заводится в его таблице, ссылка дописывается
// в текст приемника, из исходника метка и аттач удаляются.
// Как перенос голосовой: отказ в шифрованный приемник (прямая
// дописка сломала бы шифротекст), подтверждение, откат дописки
void ReferenceFormatter::onMoveAttachClicked(void)
{
  const QString href=attachHrefUnderCursor();

  if(href.isEmpty())
    return;

  AttachTableController *srcController=find_object<AttachTableController>(
    QStringLiteral("attachTableController"));

  AttachTableData *srcTable=(srcController!=nullptr)
    ? srcController->getAttachTableData() : nullptr;

  if(srcTable==nullptr)
  {
    QMessageBox::information(editor,
                             tr("Move attachment"),
                             tr("Attachments are unavailable"));
    return;
  }

  const QString innerName=attachInnerFromHref(href);

  QString rowId;

  for(int row=0; row<srcTable->size(); ++row)
  {
    const QString candidateId=srcTable->getIdByRow(row);

    if(srcTable->getInnerFileName(row)==innerName ||
       srcTable->getAttach(candidateId).getExistingInnerFileName()==innerName)
    {
      rowId=candidateId;
      break;
    }
  }

  if(rowId.isEmpty())
  {
    QMessageBox::information(editor,
                             tr("Move attachment"),
                             tr("Attachment not found"));
    return;
  }

  const QString displayName=srcTable->getFileNameById(rowId);
  const QString srcDir=editor->getWorkDirectory();
  const QString srcPath=QDir::cleanPath(QDir(srcDir).absoluteFilePath(
    srcTable->getAttach(rowId).getExistingInnerFileName()));

  if(srcDir.isEmpty() || !QFileInfo(srcPath).isFile())
  {
    QMessageBox::information(editor,
                             tr("Move attachment"),
                             tr("Attachment file not found"));
    return;
  }

  const QString currentId=editor->getMiscField(QStringLiteral("id"));

  NotePickerDialog picker(editor, currentId);

  if(picker.exec()!=QDialog::Accepted)
    return;

  const QString pickedId=picker.selectedRecordId();

  if(pickedId.isEmpty())
    return;

  KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

  if(treeView==nullptr)
  {
    QMessageBox::information(editor,
                             tr("Move attachment"),
                             tr("Notes tree unavailable"));
    return;
  }

  Record *target=static_cast<KnowTreeModel *>(treeView->model())->getRecord(pickedId);

  if(target==nullptr)
  {
    QMessageBox::information(editor,
                             tr("Move attachment"),
                             tr("Target note not found"));
    return;
  }

  // Шифрованный приемник правится только через редактор с ключом:
  // прямая дописка в файл сломала бы шифротекст
  if(target->getField(QStringLiteral("crypt"))==QStringLiteral("1"))
  {
    QMessageBox::information(editor,
                             tr("Move attachment"),
                             tr("Can not move into encrypted note"));
    return;
  }

  // Шифрованный источник: нужны открытые байты, значит разблокированная база
  const bool srcCrypt=srcTable->isRecordCrypt();

  if(srcCrypt && globalParameters.getCryptKey().isEmpty())
  {
    QMessageBox::information(editor,
                             tr("Move attachment"),
                             tr("Unlock the database first"));
    return;
  }

  const QString targetName=target->getField(QStringLiteral("name"));

  if(QMessageBox::question(editor,
                           tr("Move attachment"),
                           tr("Move attachment %1 to note \"%2\"? Badge will be removed from current note.").arg(displayName).arg(targetName),
                           QMessageBox::Yes | QMessageBox::No,
                           QMessageBox::No)!=QMessageBox::Yes)
    return;

  if(target->getField(QStringLiteral("dir")).isEmpty() ||
     target->getField(QStringLiteral("file")).isEmpty())
  {
    QMessageBox::information(editor,
                             tr("Move attachment"),
                             tr("Target note text file unknown"));
    return;
  }

  const QString targetDir=mytetraConfig.get_tetradir()+
    QStringLiteral("/base/")+
    target->getField(QStringLiteral("dir"));
  const QString targetTextFile=targetDir+
    QStringLiteral("/")+target->getField(QStringLiteral("file"));

  // Открытые байты для копии: из шифрованного источника через
  // расшифровку во временный файл
  QString copyFrom=srcPath;
  QString tmpDecrypted;

  if(srcCrypt)
  {
    tmpDecrypted=DiskHelper::copyFileToTrash(srcPath);
    CryptService::decryptFile(globalParameters.getCryptKey(),
                              tmpDecrypted);

    if(!QFileInfo(tmpDecrypted).isFile())
    {
      QMessageBox::information(editor,
                               tr("Move attachment"),
                               tr("Can not decrypt attachment file"));
      return;
    }

    copyFrom=tmpDecrypted;
  }

  // Запись в таблице приемника тем же порядком что пикер:
  // копия в базу, затем строка. Коллизия оригинала уводит
  // файл в гибрид через copyFileToBase
  AttachTableData *targetTable=target->getAttachTablePointer();
  Attach newAttach(QStringLiteral("file"), targetTable);
  newAttach.setField("id", getUniqueId());
  newAttach.setField("fileName", displayName);
  newAttach.setField("source", srcPath);

  const bool copyOk=newAttach.copyFileToBase(copyFrom);

  if(!tmpDecrypted.isEmpty())
    QFile::remove(tmpDecrypted);

  if(!copyOk)
    return;

  targetTable->addAttach(newAttach);

  const QString newInner=newAttach.getExistingInnerFileName();

  // Ссылка дописывается в закрытую заметку напрямую.
  // Не влезла: запись из таблицы приемника удаляется назад,
  // источник и его текст не тронуты
  QString error;

  if(!appendAttachBadgeToTextFile(targetTextFile, newInner, displayName, &error))
  {
    targetTable->deleteAttach(newAttach.getField(QStringLiteral("id")));

    TreeScreen *rollbackScreen=find_object<TreeScreen>("treeScreen");

    if(rollbackScreen!=nullptr)
      rollbackScreen->saveKnowTree();

    QMessageBox::information(editor,
                             tr("Move attachment"),
                             error);
    return;
  }

  // Дерево сохраняется, статистика и статусбар как в контроллере
  TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

  if(treeScreen!=nullptr)
  {
    treeScreen->saveKnowTree();
    treeScreen->knowTreeModel->refreshRecordStats(pickedId);
  }

  MainWindow *mainWindow=find_object<MainWindow>("mainwindow");

  if(mainWindow!=nullptr)
    mainWindow->updateStats();

  // Источник чистится последним: метка из текста, затем аттач.
  // Явное подтвержденное действие, отмена Ctrl+Z его не возвращает
  if(selectAttachBadgeRange(href))
    textArea->textCursor().removeSelectedText();

  srcController->removeAttachByInnerName(innerName);

  if(globalParameters.getStatusBar()!=nullptr)
    globalParameters.getStatusBar()->showMessage(
      tr("Moved to \"%1\"").arg(targetName), 3000);
}


// Цвет ссылки по адресу: единая точка в конфиге
// (внешние, mytetra://note/..., mytetra://toc-go/...).
// Невалидные значения настроек геттеры уже подменили дефолтами
QColor ReferenceFormatter::linkColorForHref(const QString &href) const
{
    return editorConfig->linkColorForHref(href);
}


// Действия при выборе контекстного меню редактора "Перейти по ссылке"
void ReferenceFormatter::onContextMenuGotoReference()
{
    QString href=editor->cursorPositionDetector->referenceHref(); // Текст ссылки

    onClickedGotoReference(href);
}


void ReferenceFormatter::onClickedGotoReference(QString href)
{
    // Голосовая заметка: вместо внешнего открывателя свой плеер.
    // Файл лежит в каталоге записи рядом с картинками
    if(VoiceNoteHelper::isVoiceHref(href))
    {
      const QString filePath=VoiceNoteHelper::resolveFilePath(editor->getWorkDirectory(), href);

      VoicePlayerDialog player(filePath, VoiceNoteHelper::fileNameFromHref(href), editor);
      player.exec();
      return;
    }

    // Клик по спойлеру не навигация: выделение и так показывает содержимое.
    // Метка живет в том же anchor-канале что ссылки
    if(href==SecretFormatter::secretHref())
      return;

    // Метка аттача из дропа в текст: внутреннее имя в каталоге записи.
    // Шифрованная запись открывается как в таблице аттачей: копия
    // в корзину с расшифровкой, иначе откроется шифрованный blob
    if(isAttachHref(href))
    {
      const QString innerName=attachInnerFromHref(href);
      const QString workDir=editor->getWorkDirectory();

      if(innerName.isEmpty() || workDir.isEmpty())
        return;

      const QString cleanWorkDir=QDir::cleanPath(QDir(workDir).absolutePath());

      AttachTableController *attachController=find_object<AttachTableController>(
        QStringLiteral("attachTableController"));

      AttachTableData *attachTableData=(attachController!=nullptr)
        ? attachController->getAttachTableData() : nullptr;

      // Строка аттача: прямое внутреннее имя либо любая схема
      // из кандидатов (режим имен сменили после вставки, коллизия
      // оригинала увела файл в гибрид). Только файлы, не линки
      QString rowId;

      if(attachTableData!=nullptr)
      {
        for(int row=0; row<attachTableData->size(); ++row)
        {
          const QString candidateId=attachTableData->getIdByRow(row);
          const Attach candidateAttach=attachTableData->getAttach(candidateId);

          if(candidateAttach.getField(QStringLiteral("type"))!=QStringLiteral("file"))
            continue;

          if(attachTableData->getInnerFileName(row)==innerName ||
             Attach::candidateFileNames(QStringLiteral("file"),
               candidateAttach.getField(QStringLiteral("id")),
               candidateAttach.getField(QStringLiteral("fileName"))).contains(innerName))
          {
            rowId=candidateId;
            break;
          }
        }
      }

      // Реальный файл: прямой путь, иначе найденная строка.
      // getExistingInnerFileName сам ищет по всем схемам
      QString absolutePath=QDir::cleanPath(QDir(workDir).absoluteFilePath(innerName));

      if(!QFileInfo(absolutePath).isFile() && !rowId.isEmpty())
        absolutePath=QDir::cleanPath(QDir(workDir).absoluteFilePath(
          attachTableData->getAttach(rowId).getExistingInnerFileName()));

      if(!QFileInfo(absolutePath).isFile())
        return;

      // Только внутрь каталога записи, без выхода наверх.
      // Проверка до расшифровки: копия в корзину лежит уже вне записи
      if(!absolutePath.startsWith(cleanWorkDir+QLatin1Char('/')))
        return;

      if(!rowId.isEmpty())
      {
        const Attach rowAttach=attachTableData->getAttach(rowId);

        if(attachTableData->isRecordCrypt() &&
           rowAttach.getField(QStringLiteral("type"))==QStringLiteral("file"))
        {
          if(mytetraConfig.getEnableDecryptFileToTrashDirectory())
          {
            absolutePath=DiskHelper::copyFileToTrash(absolutePath);
            CryptService::decryptFile(globalParameters.getCryptKey(), absolutePath);
          }
          else
          {
            QMessageBox msgBox;
            msgBox.setText(tr("Unable to preview the encrypted attached file.\nPlease, use «Save As...» button, or\nenable «Decrypt to a temporary file» option in Settings."));
            msgBox.exec();
            return;
          }
        }
      }

      QDesktopServices::openUrl(QUrl::fromLocalFile(absolutePath));
      return;
    }

    // Относительная ссылка без схемы: файл рядом с текстом записи
    // (картинки, аттачи из импорта). Раскрывается относительно
    // каталога записи и открывается внешней программой
    {
      const QString absolutePath=LinkHelper::resolveRelativeLink(editor->getWorkDirectory(), href);

      if(!absolutePath.isEmpty())
      {
        QDesktopServices::openUrl(QUrl::fromLocalFile(absolutePath));
        return;
      }
    }

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
