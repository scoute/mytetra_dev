#include <QTextCursor>
#include <QTextDocument>
#include <QColor>
#include <QStatusBar>
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
  // Цвет из настроек: пользователь выбирает любой, по умолчанию зеленый.
  // Уже закрашенное хранит свой цвет в документе и не перекрашивается
  const QColor configuredColor(mytetraConfig.get_secretColor());

  if(configuredColor.isValid())
    return configuredColor;

  return QColor(0x2e, 0x8b, 0x57);
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
