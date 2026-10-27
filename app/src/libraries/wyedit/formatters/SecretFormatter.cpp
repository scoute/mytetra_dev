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
  // Уже закрашенное хранит свой цвет в документе и не перекрашивается.
  // Действует только на теме Custom: базовые темы как есть
  if(mytetraConfig.getInterfaceTheme()!="custom")
    return QColor(0x77, 0x76, 0x7b);

  const QColor configuredColor(mytetraConfig.get_secretColor());

  if(configuredColor.isValid())
    return configuredColor;

  return QColor(0x77, 0x76, 0x7b);
}


// Закрасить выделение — или открыть секрет под курсором (тоггл).
// Без выделения и вне секрета только подсказка в статусной строке:
// красить нечего, а слово под курсором угадывать не надо
void SecretFormatter::onSecretClicked(void)
{
  // Решение "открывать": секрет под курсором без выделения —
  // штатный детектор; в выделении достаточно ХОТЬ ОДНОГО секрета
  // (строгое "все выделение внутри" требует ювелирного выделения
  // без захвата соседних символов и молча перекрашивает поверх)
  bool reveal=false;
  QTextCursor cursor=textArea->textCursor();

  qDebug() << "Secret toggle: hasSelection" << cursor.hasSelection()
           << "pos" << cursor.position()
           << "sel" << cursor.selectionStart() << cursor.selectionEnd();

  if(cursor.hasSelection())
    reveal=selectionContainsSecret(cursor.selectionStart(),
                                   cursor.selectionEnd());
  else
  {
    reveal=editor->cursorPositionDetector->isCursorOnSecret();

    // Осиротевшая закраска без якоря (остаток старых версий):
    // секретом считается и просто пара секретных красок
    if(!reveal)
    {
      QTextCursor probeCursor=textArea->textCursor();
      reveal=isSecretLikeFormat(probeCursor.charFormat());

      if(!reveal && probeCursor.movePosition(QTextCursor::Right))
        reveal=isSecretLikeFormat(probeCursor.charFormat());
    }
  }

  qDebug() << "Secret toggle: decision" << (reveal ? "reveal" : "hide-or-hint");

  if(reveal)
  {
    const int savedPosition=cursor.position();

    // Границы секрета как в копировании: метка соседних символов
    // (или секретные краски), выделение расширяется до целого секрета.
    // Правая граница упирается в конец документа: setPosition за
    // границей игнорируется, а не клиппится
    QTextCursor probeCursor(cursor);
    QTextDocument *document=textArea->document();
    const int docEnd=document->characterCount()-1;

    int left=cursor.hasSelection() ? cursor.selectionStart() : cursor.position();
    int right=cursor.hasSelection() ? cursor.selectionEnd() : cursor.position();

    while(left>0)
    {
      probeCursor.setPosition(left);
      if(!isSecretLikeFormat(probeCursor.charFormat()))
        break;
      left--;
    }

    while(right<docEnd)
    {
      probeCursor.setPosition(right+1);
      if(!isSecretLikeFormat(probeCursor.charFormat()))
        break;
      right++;
    }

    revealSecretRange(left, right);

    qDebug() << "Secret revealed range" << left << right
             << "cursor restored to" << savedPosition;

    cursor.setPosition(savedPosition);
    textArea->setTextCursor(cursor);

    QStatusBar *statusBar=globalParameters.getStatusBar();

    if(statusBar!=nullptr)
      statusBar->showMessage(tr("Secret revealed"));

    return;
  }

  if(!cursor.hasSelection())
  {
    qDebug() << "Secret toggle: no selection outside secret, hint shown";

    QStatusBar *statusBar=globalParameters.getStatusBar();

    if(statusBar!=nullptr)
      statusBar->showMessage(tr("Select text to hide as secret"));

    return;
  }

  qDebug() << "Secret hide range" << cursor.selectionStart() << cursor.selectionEnd();

  QTextCharFormat charFormat;
  charFormat.setAnchor(true);
  charFormat.setAnchorHref(secretHref());
  charFormat.setForeground(secretColor());
  charFormat.setBackground(secretColor());
  charFormat.setFontUnderline(false);

  cursor.mergeCharFormat(charFormat);
}


// Есть ли хоть один секрет внутри выделения [start, end).
// Секретом считается и якорь secret, и просто пара секретных красок
// (осиротевшая закраска). Проверка строгая (без захвата соседей):
// формат символа i читается позицией i+1. Пустой диапазон — false
bool SecretFormatter::selectionContainsSecret(int start, int end) const
{
  if(end<=start)
    return false;

  QTextCursor probeCursor=textArea->textCursor();

  for(int i=start; i<end; ++i)
  {
    probeCursor.setPosition(i+1);

    if(isSecretLikeFormat(probeCursor.charFormat()))
      return true;
  }

  return false;
}


// Похож ли формат на секрет: якорь secret либо пара секретных красок
// без якоря (остаток старых версий, чистится тем же reveal)
bool SecretFormatter::isSecretLikeFormat(const QTextCharFormat &format)
{
  if(isSecretFormat(format))
    return true;

  const QColor secret=secretColor();

  return format.foreground().color()==secret &&
         format.background().color()==secret;
}


// Снять закраску с диапазона: якорь убирается, краски ставятся
// в обычные (явно, через палитру: clearForeground/clearBackground
// в mergeCharFormat — no-op, снятое свойство не перезаписывает).
// Курсор возвращается в исходную точку
void SecretFormatter::revealSecretRange(int left, int right)
{
  if(left>=right)
    return;

  QTextCursor cursor=textArea->textCursor();
  cursor.setPosition(left);
  cursor.setPosition(right, QTextCursor::KeepAnchor);

  QTextCharFormat clearFormat;
  clearFormat.setAnchor(false);
  clearFormat.setAnchorHref(QString());
  clearFormat.setForeground(QApplication::palette().color(QPalette::Text));
  clearFormat.setBackground(Qt::NoBrush);

  cursor.mergeCharFormat(clearFormat);
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
