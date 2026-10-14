#include <QMessageBox>
#include <QTextCursor>
#include <QTextDocument>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QAudioDeviceInfo>

#include "AudioFormatter.h"

#include "../Editor.h"
#include "../EditorTextArea.h"
#include "../EditorCursorPositionDetector.h"
#include "views/voiceNote/VoiceRecorderDialog.h"
#include "views/voiceNote/VoicePlayerDialog.h"
#include "libraries/VoiceNoteHelper.h"
#include "libraries/helpers/DiskHelper.h"


AudioFormatter::AudioFormatter()
{

}


void AudioFormatter::onMicRecordClicked(void)
{
  // Без микрофона записывать нечего
  if(QAudioDeviceInfo::availableDevices(QAudio::AudioInput).isEmpty())
  {
    QMessageBox::information(editor,
                             tr("Voice note"),
                             tr("Microphone not found"));
    return;
  }

  // Каталог записи: файл ложится рядом с картинками заметки.
  // Пусто пока запись не сохранена в базу
  const QString workDir=editor->getWorkDirectory();

  if(workDir.isEmpty())
  {
    QMessageBox::information(editor,
                             tr("Voice note"),
                             tr("Save the note first"));
    return;
  }

  VoiceRecorderDialog recorder(workDir, editor);

  if(recorder.exec()!=QDialog::Accepted)
    return;

  const QString filePath=recorder.recordedFilePath();

  if(!QFileInfo(filePath).isFile())
    return;

  // Ссылка вида [voice 0:07] на файл в каталоге записи.
  // Клик перехватывает ReferenceFormatter и открывает плеер
  const QString href=VoiceNoteHelper::makeHref(QFileInfo(filePath).fileName());
  const QString text=VoiceNoteHelper::linkText(recorder.recordedDurationMs());

  QTextCursor cursor=textArea->textCursor();
  cursor.insertHtml(QStringLiteral("<a href=\"")+href.toHtmlEscaped()+
                    QStringLiteral("\">")+text.toHtmlEscaped()+
                    QStringLiteral("</a> "));
}


QString AudioFormatter::voiceHrefUnderCursor(void) const
{
  if(!editor->cursorPositionDetector->isCursorOnReference())
    return QString();

  const QString href=editor->cursorPositionDetector->referenceHref();

  if(!VoiceNoteHelper::isVoiceHref(href))
    return QString();

  return href;
}


bool AudioFormatter::selectVoiceLinkRange(const QString &href)
{
  if(href.isEmpty())
    return false;

  QTextCursor probe=textArea->textCursor();
  const int pos=probe.position();
  QTextDocument *doc=textArea->document();

  // CharFormat смотрит на символ слева от курсора: принадлежность
  // символа i проверяется с позиции i+1
  auto charHasHref=[&](int index) {
    if(index<0 || index>=doc->characterCount()-1)
      return false;
    probe.setPosition(index+1);
    return probe.charFormat().anchorHref()==href;
  };

  if(!charHasHref(pos) && !charHasHref(pos-1))
    return false;

  int start=pos;
  while(charHasHref(start-1))
    --start;

  int end=pos;
  while(charHasHref(end))
    ++end;

  // Хвостовой пробел от вставки забирается тоже чтобы не мусорить
  if(end<doc->characterCount()-1 && doc->characterAt(end)==QLatin1Char(' '))
    ++end;

  QTextCursor select=textArea->textCursor();
  select.setPosition(start);
  select.setPosition(end, QTextCursor::KeepAnchor);
  textArea->setTextCursor(select);

  return select.hasSelection();
}


void AudioFormatter::onPlayVoiceClicked(void)
{
  const QString href=voiceHrefUnderCursor();

  if(href.isEmpty())
    return;

  const QString filePath=VoiceNoteHelper::resolveFilePath(editor->getWorkDirectory(), href);

  VoicePlayerDialog player(filePath, VoiceNoteHelper::fileNameFromHref(href), editor);
  player.exec();
}


void AudioFormatter::onShowVoiceInFolder(void)
{
  const QString href=voiceHrefUnderCursor();

  if(href.isEmpty())
    return;

  const QString directory=editor->getWorkDirectory();

  if(directory.isEmpty())
  {
    QMessageBox::information(editor,
                             tr("Show voice file in folder"),
                             tr("Note directory is unknown, there is nothing to open."));
    return;
  }

  // Путь делается абсолютным: workDirectory обычно относительный,
  // а файловому менеджеру относительный file-URL не открывается
  const QString absoluteDirectory=QDir(directory).absolutePath();

  if(!QDir(absoluteDirectory).exists())
  {
    QMessageBox::information(editor,
                             tr("Show voice file in folder"),
                             tr("Directory does not exist: %1").arg(absoluteDirectory));
    return;
  }

  if(!QDesktopServices::openUrl(QUrl::fromLocalFile(absoluteDirectory)))
  {
    QMessageBox::information(editor,
                             tr("Show voice file in folder"),
                             tr("Can not open directory: %1").arg(absoluteDirectory));
  }
}


void AudioFormatter::onDeleteVoiceClicked(void)
{
  const QString href=voiceHrefUnderCursor();

  if(href.isEmpty())
    return;

  const QString fileName=VoiceNoteHelper::fileNameFromHref(href);
  const QString filePath=VoiceNoteHelper::resolveFilePath(editor->getWorkDirectory(), href);

  if(QMessageBox::question(editor,
                           tr("Delete voice note"),
                           tr("Delete voice note %1? Link text will be removed, file moved to trash.").arg(fileName),
                           QMessageBox::Yes | QMessageBox::No,
                           QMessageBox::No)!=QMessageBox::Yes)
    return;

  // Текст ссылки вырезается (возвращается через Ctrl+Z),
  // файл уезжает в корзину средствами помощника
  if(selectVoiceLinkRange(href))
    textArea->textCursor().removeSelectedText();

  if(QFileInfo(filePath).isFile())
    DiskHelper::removeFileToTrash(filePath);
}
