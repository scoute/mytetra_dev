#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QColor>
#include <QStatusBar>
#include <QColorDialog>
#include <QApplication>
#include <QClipboard>

#include "SecretFormatter.h"
#include "../Editor.h"
#include "../EditorTextArea.h"
#include "../EditorCursorPositionDetector.h"
#include "libraries/GlobalParameters.h"
#include "models/appConfig/AppConfig.h"


extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;


QString SecretFormatter::secretHref(void)
{
  return QStringLiteral("secret");
}


bool SecretFormatter::isSecretFormat(const QTextCharFormat &format)
{
  return format.isAnchor() && format.anchorHref()==secretHref();
}


QColor SecretFormatter::secretColor(void)
{
  // Цвет из настроек: пользователь выбирает любой, по умолчанию серый.
  // Уже закрашенное хранит свой цвет в документе и не перекрашивается
  const QColor configuredColor(mytetraConfig.get_secretColor());

  if(configuredColor.isValid())
    return configuredColor;

  return QColor(0x77, 0x76, 0x7b);
}


// Закрасить выделение. Без выделения только подсказка в статусной строке:
// красить нечего, а слово под курсором угадывать не надо
void SecretFormatter::onSecretClicked(void)
{
  QTextCursor cursor=textArea->textCursor();

  if(!cursor.hasSelection())
  {
    QStatusBar *statusBar=globalParameters.getStatusBar();

    if(statusBar!=nullptr)
      statusBar->showMessage(tr("Select text to hide as secret"));

    return;
  }

  QTextCharFormat charFormat;
  charFormat.setAnchor(true);
  charFormat.setAnchorHref(secretHref());
  charFormat.setForeground(secretColor());
  charFormat.setBackground(secretColor());
  charFormat.setFontUnderline(false);

  cursor.mergeCharFormat(charFormat);
}


// Скопировать содержимое секрета под курсором в буфер.
// Границы берутся по метке соседних символов, курсор возвращается как был
void SecretFormatter::onContextMenuCopySecret(void)
{
  if(!editor->cursorPositionDetector->isCursorOnSecret())
    return; // под курсором не спойлер

  QTextCursor cursor=textArea->textCursor();
  const int savedPosition=cursor.position();
  const int savedAnchor=cursor.anchor();

  int left=cursor.selectionStart();
  int right=cursor.selectionEnd();

  // Граница по символам: символ c спойлер если формат позиции c+1
  // с меткой (Qt отдает формат символа слева от курсора).
  // Правая граница упирается в конец документа: setPosition за
  // границей игнорируется, а не клиппится
  QTextCursor probeCursor(cursor);
  QTextDocument *document=textArea->document();
  const int docEnd=document->characterCount()-1;

  while(left>0)
  {
    probeCursor.setPosition(left);
    if(!isSecretFormat(probeCursor.charFormat()))
      break;
    left--;
  }

  while(right<docEnd)
  {
    probeCursor.setPosition(right+1);
    if(!isSecretFormat(probeCursor.charFormat()))
      break;
    right++;
  }

  cursor.setPosition(left);
  cursor.setPosition(right, QTextCursor::KeepAnchor);

  // selectedText разделяет абзацы символом U+2029: в одну строку он
  // превращается в кракозябру, в plain-текст идет обычный перевод строки
  QString plainText=cursor.selectedText();
  plainText.replace(QChar::ParagraphSeparator, '\n');
  QApplication::clipboard()->setText(plainText);

  cursor.setPosition(savedAnchor);
  cursor.setPosition(savedPosition, QTextCursor::KeepAnchor);
  textArea->setTextCursor(cursor);
}


// Сменить глобальный цвет секретов: диалог, запись в конфиг,
// живая перекраска текущей заметки
void SecretFormatter::onContextMenuChangeSecretColor(void)
{
  if(!editor->cursorPositionDetector->isCursorOnSecret())
    return; // под курсором не секрет

  const QColor chosenColor=QColorDialog::getColor(secretColor(), textArea);

  if(!chosenColor.isValid())
    return;

  mytetraConfig.set_secretColor(chosenColor.name());

  // Сразу на диск: иначе смена живет только до выхода
  // (или до первой потери фокуса окном)
  mytetraConfig.sync();

  repaintSecrets();
}


// Перекрасить все секреты документа глобальным цветом.
// Идут только несовпадающие: уже conforming документ не трогается
// вообще (ни undo, ни modified). Одна операция отмены на все
void SecretFormatter::repaintSecrets(void)
{
  QTextDocument *document=textArea->document();
  const QColor targetColor=secretColor();

  struct SecretRange
  {
    int start;
    int length;
  };
  QList<SecretRange> ranges;

  for(QTextBlock block=document->begin(); block.isValid(); block=block.next())
  {
    for(QTextBlock::iterator it=block.begin(); !(it.atEnd()); ++it)
    {
      QTextFragment fragment=it.fragment();
      if(!fragment.isValid() || fragment.length()<=0)
        continue;

      const QTextCharFormat format=fragment.charFormat();
      if(!isSecretFormat(format))
        continue;

      if(format.foreground().color()==targetColor &&
         format.background().color()==targetColor)
        continue;

      SecretRange range;
      range.start=fragment.position();
      range.length=fragment.length();
      ranges << range;
    }
  }

  if(ranges.isEmpty())
    return;

  const bool wasModified=document->isModified();

  QTextCursor editCursor(document);
  editCursor.beginEditBlock();

  QTextCharFormat paintFormat;
  paintFormat.setForeground(targetColor);
  paintFormat.setBackground(targetColor);

  for(const SecretRange &range : ranges)
  {
    editCursor.setPosition(range.start);
    editCursor.setPosition(range.start+range.length, QTextCursor::KeepAnchor);
    editCursor.mergeCharFormat(paintFormat);
  }

  editCursor.endEditBlock();

  // Подтяжка не правка пользователя
  if(!wasModified)
    document->setModified(false);
}
