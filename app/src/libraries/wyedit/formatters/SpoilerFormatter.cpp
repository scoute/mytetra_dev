#include <QTextCursor>
#include <QTextDocument>
#include <QColor>
#include <QStatusBar>
#include <QApplication>
#include <QClipboard>

#include "SpoilerFormatter.h"
#include "../Editor.h"
#include "../EditorTextArea.h"
#include "../EditorCursorPositionDetector.h"
#include "libraries/GlobalParameters.h"


extern GlobalParameters globalParameters;


QString SpoilerFormatter::spoilerHref(void)
{
  return QStringLiteral("spoiler");
}


bool SpoilerFormatter::isSpoilerFormat(const QTextCharFormat &format)
{
  return format.isAnchor() && format.anchorHref()==spoilerHref();
}


QColor SpoilerFormatter::spoilerColor(void)
{
  return QColor(0x2e, 0x8b, 0x57);
}


// Закрасить выделение. Без выделения только подсказка в статусной строке:
// красить нечего, а слово под курсором угадывать не надо
void SpoilerFormatter::onSpoilerClicked(void)
{
  QTextCursor cursor=textArea->textCursor();

  if(!cursor.hasSelection())
  {
    QStatusBar *statusBar=globalParameters.getStatusBar();

    if(statusBar!=nullptr)
      statusBar->showMessage(tr("Select text to hide as spoiler"));

    return;
  }

  QTextCharFormat charFormat;
  charFormat.setAnchor(true);
  charFormat.setAnchorHref(spoilerHref());
  charFormat.setForeground(spoilerColor());
  charFormat.setBackground(spoilerColor());
  charFormat.setFontUnderline(false);

  cursor.mergeCharFormat(charFormat);
}


// Скопировать содержимое спойлера под курсором в буфер.
// Границы берутся по метке соседних символов, курсор возвращается как был
void SpoilerFormatter::onContextMenuCopySpoiler(void)
{
  if(!editor->cursorPositionDetector->isCursorOnSpoiler())
    return; // под курсором не спойлер

  QTextCursor cursor=textArea->textCursor();
  const int savedPosition=cursor.position();
  const int savedAnchor=cursor.anchor();

  int left=cursor.selectionStart();
  int right=cursor.selectionEnd();

  // Граница по символам: символ c спойлер если формат позиции c+1
  // с меткой (Qt отдает формат символа слева от курсора).
  // Позиции 0..N валидны всегда, за документ не выходим
  QTextCursor probeCursor(cursor);
  QTextDocument *document=textArea->document();

  while(left>0)
  {
    probeCursor.setPosition(left);
    if(!isSpoilerFormat(probeCursor.charFormat()))
      break;
    left--;
  }

  while(right<document->characterCount())
  {
    probeCursor.setPosition(right+1);
    if(!isSpoilerFormat(probeCursor.charFormat()))
      break;
    right++;
  }

  cursor.setPosition(left);
  cursor.setPosition(right, QTextCursor::KeepAnchor);
  QApplication::clipboard()->setText(cursor.selectedText());

  cursor.setPosition(savedAnchor);
  cursor.setPosition(savedPosition, QTextCursor::KeepAnchor);
  textArea->setTextCursor(cursor);
}
