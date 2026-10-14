#include <QMessageBox>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QAudioDeviceInfo>

#include "AudioFormatter.h"

#include "../Editor.h"
#include "../EditorTextArea.h"
#include "../EditorCursorPositionDetector.h"
#include "views/voiceNote/VoiceRecorderDialog.h"
#include "views/voiceNote/VoicePlayerDialog.h"
#include "libraries/VoiceNoteHelper.h"
#include "libraries/helpers/DiskHelper.h"
#include "libraries/GlobalParameters.h"

extern GlobalParameters globalParameters;


AudioFormatter::AudioFormatter()
{
  bubbleObject=new VoiceBubbleObject(this);
}


VoiceBubbleObject *AudioFormatter::bubbleHandler(void)
{
  return bubbleObject;
}


void AudioFormatter::bubblePlaybackState(const QString &href, bool &playing,
                                         qint64 &posMs, qint64 &totalMs)
{
  playing=false;

  if(voicePlayer!=nullptr && activeHref==href)
  {
    if(voicePlayer->state()==QMediaPlayer::PlayingState)
    {
      playing=true;
      posMs=voicePlayer->position();
    }

    const qint64 duration=voicePlayer->duration();

    if(duration>0)
      totalMs=duration;
  }
}


void AudioFormatter::insertBubble(const QString &href, qint64 durationMs,
                                  const QString &fileName)
{
  QTextCursor cursor=textArea->textCursor();

  cursor.insertText(QString(QChar::ObjectReplacementCharacter),
                    VoiceNoteHelper::bubbleCharFormat(href, durationMs, fileName));
  cursor.insertText(QStringLiteral(" "));
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

  // В текст вставляется бабл, в файле он хранится ссылкой.
  // Старые ссылки тоже материализуются при загрузке
  const QString fileName=QFileInfo(filePath).fileName();
  const QString href=VoiceNoteHelper::makeHref(fileName);

  insertBubble(href, recorder.recordedDurationMs(), fileName);
}


void AudioFormatter::materializeBubbles(void)
{
  QTextDocument *doc=textArea->document();

  struct VoiceLinkRange
  {
    int start;
    int end;
    QString href;
    QString text;
  };

  QList<VoiceLinkRange> ranges;

  // Собираются все голосовые ссылки: идущие подряд фрагменты
  // с одним href склеиваются в один диапазон
  for(QTextBlock block=doc->firstBlock(); block.isValid(); block=block.next())
    for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
    {
      const QTextFragment fragment=it.fragment();

      if(!fragment.isValid())
        continue;

      const QString href=fragment.charFormat().anchorHref();

      if(!VoiceNoteHelper::isVoiceHref(href))
        continue;

      const int start=fragment.position();
      const int end=start+fragment.length();

      if(!ranges.isEmpty() && ranges.last().href==href && ranges.last().end==start)
      {
        ranges.last().end=end;
        ranges.last().text+=fragment.text();
      }
      else
        ranges.append({start, end, href, fragment.text()});
    }

  if(ranges.isEmpty())
    return;

  // Замена с конца чтобы позиции не съезжали. Одна операция
  // отмены на все, как repaintSecrets
  QTextCursor cursor(doc);
  cursor.beginEditBlock();

  for(int i=ranges.size()-1; i>=0; --i)
  {
    const VoiceLinkRange range=ranges.at(i);
    const QString fileName=VoiceNoteHelper::fileNameFromHref(range.href);
    qint64 durationMs=VoiceNoteHelper::parseDuration(range.text);

    if(durationMs<0)
      durationMs=0;

    cursor.setPosition(range.start);
    cursor.setPosition(range.end, QTextCursor::KeepAnchor);
    cursor.insertText(QString(QChar::ObjectReplacementCharacter),
                      VoiceNoteHelper::bubbleCharFormat(range.href, durationMs, fileName));
  }

  cursor.endEditBlock();
}


QString AudioFormatter::exportHtml(void)
{
  QTextDocument *live=textArea->document();

  if(!documentHasBubbles())
    return live->toHtml("UTF-8");

  // Подмена делается на копии: живой документ, курсор
  // и undo пользователя не трогаются
  QTextDocument *copy=live->clone();
  copy->setMetaInformation(QTextDocument::DocumentUrl,
                           live->metaInformation(QTextDocument::DocumentUrl));

  QTextCursor cursor(copy);
  cursor.beginEditBlock();

  // Позиции собираются заранее, замена с конца
  struct BubbleRange
  {
    int pos;
    QString href;
    qint64 durationMs;
  };

  QList<BubbleRange> ranges;

  for(QTextBlock block=copy->firstBlock(); block.isValid(); block=block.next())
    for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
    {
      const QTextFragment fragment=it.fragment();

      if(!fragment.isValid())
        continue;

      const QTextCharFormat format=fragment.charFormat();

      if(!VoiceNoteHelper::isBubbleFormat(format))
        continue;

      ranges.append({fragment.position(),
                     format.property(VoiceNoteHelper::BubblePropHref).toString(),
                     format.property(VoiceNoteHelper::BubblePropDurationMs).toLongLong()});
    }

  cursor.beginEditBlock();

  for(int i=ranges.size()-1; i>=0; --i)
  {
    const BubbleRange range=ranges.at(i);
    const QString text=VoiceNoteHelper::linkText(range.durationMs);

    cursor.setPosition(range.pos);
    cursor.setPosition(range.pos+1, QTextCursor::KeepAnchor);
    cursor.insertHtml(QStringLiteral("<a href=\"")+range.href.toHtmlEscaped()+
                      QStringLiteral("\">")+text.toHtmlEscaped()+
                      QStringLiteral("</a>"));
  }

  cursor.endEditBlock();

  const QString html=copy->toHtml("UTF-8");
  delete copy;

  return html;
}


void AudioFormatter::onBubbleClicked(const QString &href)
{
  if(href.isEmpty())
    return;

  const QString filePath=VoiceNoteHelper::resolveFilePath(editor->getWorkDirectory(), href);

  if(!QFileInfo(filePath).isFile())
  {
    if(globalParameters.getStatusBar()!=nullptr)
      globalParameters.getStatusBar()->showMessage(tr("Voice file not found"), 3000);

    return;
  }

  if(voicePlayer==nullptr)
  {
    voicePlayer=new QMediaPlayer(this);

    connect(voicePlayer, QOverload<QMediaPlayer::State>::of(&QMediaPlayer::stateChanged),
            this,          &AudioFormatter::onVoiceStateChanged);

    voiceTick=new QTimer(this);
    voiceTick->setInterval(300);

    connect(voiceTick, &QTimer::timeout,
            this,        &AudioFormatter::onVoiceTick);

    voiceTick->start();
  }

  // Повторный клик по играющему ставит на паузу,
  // по стоявшему на паузе продолжает
  if(activeHref==href && voicePlayer->state()==QMediaPlayer::PlayingState)
  {
    voicePlayer->pause();
    return;
  }

  if(activeHref!=href)
  {
    voicePlayer->stop();
    voicePlayer->setMedia(QUrl::fromLocalFile(QFileInfo(filePath).absoluteFilePath()));
    activeHref=href;
  }
  else if(voicePlayer->state()==QMediaPlayer::StoppedState)
  {
    // Доиграло до конца: повтор с начала
    voicePlayer->setPosition(0);
  }

  voicePlayer->play();
}


void AudioFormatter::onVoiceTick(void)
{
  if(voicePlayer==nullptr || activeHref.isEmpty())
    return;

  // Бабл удалили из текста во время игры: стоп
  const int pos=findBubblePos(activeHref);

  if(pos<0)
  {
    stopVoicePlayback();
    return;
  }

  textArea->document()->markContentsDirty(pos, 1);
}


void AudioFormatter::onVoiceStateChanged(QMediaPlayer::State state)
{
  if(state==QMediaPlayer::PlayingState)
    wasPlaying=true;
  else if(wasPlaying && state==QMediaPlayer::StoppedState)
  {
    // Доиграло до конца: кнопка снова play, время сбрасывается
    wasPlaying=false;

    const int pos=findBubblePos(activeHref);

    activeHref.clear();

    if(pos>=0)
      textArea->document()->markContentsDirty(pos, 1);
  }
  else if(state==QMediaPlayer::PausedState)
  {
    const int pos=findBubblePos(activeHref);

    if(pos>=0)
      textArea->document()->markContentsDirty(pos, 1);
  }
}


int AudioFormatter::findBubblePos(const QString &href) const
{
  if(href.isEmpty())
    return -1;

  QTextDocument *doc=textArea->document();

  for(QTextBlock block=doc->firstBlock(); block.isValid(); block=block.next())
    for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
    {
      const QTextFragment fragment=it.fragment();

      if(!fragment.isValid())
        continue;

      const QTextCharFormat format=fragment.charFormat();

      if(VoiceNoteHelper::isBubbleFormat(format) &&
         format.property(VoiceNoteHelper::BubblePropHref).toString()==href)
        return fragment.position();
    }

  return -1;
}


bool AudioFormatter::documentHasBubbles(void) const
{
  QTextDocument *doc=textArea->document();

  for(QTextBlock block=doc->firstBlock(); block.isValid(); block=block.next())
    for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
    {
      const QTextFragment fragment=it.fragment();

      if(fragment.isValid() && VoiceNoteHelper::isBubbleFormat(fragment.charFormat()))
        return true;
    }

  return false;
}


void AudioFormatter::stopVoicePlayback(void)
{
  if(voicePlayer!=nullptr)
    voicePlayer->stop();

  wasPlaying=false;

  const int pos=findBubblePos(activeHref);

  activeHref.clear();

  if(pos>=0 && textArea!=nullptr && textArea->document()!=nullptr)
    textArea->document()->markContentsDirty(pos, 1);
}


QString AudioFormatter::voiceHrefUnderCursor(void) const
{
  if(editor->cursorPositionDetector->isCursorOnReference())
  {
    const QString href=editor->cursorPositionDetector->referenceHref();

    if(VoiceNoteHelper::isVoiceHref(href))
      return href;
  }

  // Бабл не ссылка: смотрится формат символа у курсора
  QTextCursor cursor=textArea->textCursor();

  return VoiceNoteHelper::bubbleHrefAt(textArea->document(), cursor.position());
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


bool AudioFormatter::selectVoiceBubbleRange(const QString &href)
{
  if(href.isEmpty())
    return false;

  QTextDocument *doc=textArea->document();
  QTextCursor probe=textArea->textCursor();
  const int pos=probe.position();

  auto charIsBubble=[&](int index) {
    if(index<0 || index>=doc->characterCount()-1)
      return false;
    probe.setPosition(index+1);
    const QTextCharFormat format=probe.charFormat();
    return VoiceNoteHelper::isBubbleFormat(format) &&
           format.property(VoiceNoteHelper::BubblePropHref).toString()==href;
  };

  int target=-1;

  if(charIsBubble(pos))
    target=pos;
  else if(charIsBubble(pos-1))
    target=pos-1;

  if(target<0)
    return false;

  QTextCursor select=textArea->textCursor();
  select.setPosition(target);
  select.setPosition(target+1, QTextCursor::KeepAnchor);
  textArea->setTextCursor(select);

  return select.hasSelection();
}


void AudioFormatter::onPlayVoiceClicked(void)
{
  const QString href=voiceHrefUnderCursor();

  if(href.isEmpty())
    return;

  // Бабл играет на месте, старая ссылка открывает плеер
  if(findBubblePos(href)>=0)
    onBubbleClicked(href);
  else
  {
    const QString filePath=VoiceNoteHelper::resolveFilePath(editor->getWorkDirectory(), href);

    VoicePlayerDialog player(filePath, VoiceNoteHelper::fileNameFromHref(href), editor);
    player.exec();
  }
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

  // Играющий бабл перед удалением останавливается
  if(activeHref==href)
    stopVoicePlayback();

  // Текст ссылки или бабл вырезается (возвращается через Ctrl+Z),
  // файл уезжает в корзину средствами помощника
  if(findBubblePos(href)>=0)
    selectVoiceBubbleRange(href);
  else
    selectVoiceLinkRange(href);

  textArea->textCursor().removeSelectedText();

  if(QFileInfo(filePath).isFile())
    DiskHelper::removeFileToTrash(filePath);
}
