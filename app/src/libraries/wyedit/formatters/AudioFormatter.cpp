#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QRegularExpression>
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
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/GlobalParameters.h"
#include "models/appConfig/AppConfig.h"
#include "models/recordTable/Record.h"
#include "models/tree/KnowTreeModel.h"
#include "views/notePicker/NotePickerDialog.h"
#include "views/tree/KnowTreeView.h"

extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;


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
  QTextDocument *doc=textArea->document();
  QTextCursor cursor=textArea->textCursor();
  const int at=cursor.position();

  // Формат для текста вокруг: из символа перед вставкой,
  // без бабловых свойств. Иначе печатаемый следом текст
  // наследует формат бабла: setPlainText и ввод берут текущий
  QTextCharFormat base;

  if(at>0)
  {
    QTextCursor probe(doc);
    probe.setPosition(at);
    base=probe.charFormat();
    base.setObjectType(QTextFormat::NoObject);
    base.clearProperty(VoiceNoteHelper::BubblePropHref);
    base.clearProperty(VoiceNoteHelper::BubblePropDurationMs);
    base.clearProperty(VoiceNoteHelper::BubblePropFileName);
  }

  cursor.insertText(QString(QChar::ObjectReplacementCharacter),
                    VoiceNoteHelper::bubbleCharFormat(href, durationMs, fileName));
  cursor.insertText(QStringLiteral(" "), base);
  cursor.setCharFormat(base);
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

  if(!VoiceNoteHelper::hasVoiceLinks(doc))
    return;

  // Одна операция отмены на все, как repaintSecrets
  QTextCursor cursor(doc);
  cursor.beginEditBlock();
  VoiceNoteHelper::materializeVoiceLinks(doc);
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

  VoiceNoteHelper::dematerializeBubbles(copy);

  const QString html=copy->toHtml("UTF-8");
  delete copy;

  return html;
}


AudioFormatter::BubbleClickAction AudioFormatter::bubbleClickAction(qreal bubbleWidth,
                                                                     qreal offsetX,
                                                                     qreal &seekFraction)
{
  seekFraction=0.0;

  // Нет геометрии (меню, ошибка раскладки): безопасный toggle
  if(bubbleWidth<=0.0 || offsetX<VoiceBubbleObject::toggleZoneWidth())
    return BubbleClickToggle;

  seekFraction=qBound(0.0, offsetX/bubbleWidth, 1.0);

  return BubbleClickSeek;
}


void AudioFormatter::onBubbleClicked(const QString &href, const QPointF &docPoint)
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

  // Клик правее кружка мотает: доля позиции клика в ширине бабла
  bool seekClicked=false;
  qreal seekFraction=0.0;

  const int bubblePos=findBubblePos(href);

  if(bubblePos>=0)
  {
    const qreal leftEdge=VoiceBubbleObject::bubbleLeftEdge(textArea->document(), bubblePos);

    if(leftEdge>=0.0)
    {
      // Ширина берется у рисовальщика по формату символа
      QTextCursor probe(textArea->document());
      probe.setPosition(bubblePos+1);
      const qreal width=bubbleObject->intrinsicSize(
          textArea->document(), bubblePos, probe.charFormat()).width();

      if(bubbleClickAction(width, docPoint.x()-leftEdge, seekFraction)==
         BubbleClickSeek)
        seekClicked=true;
    }
  }

  // Повторный клик по кружку играющего ставит на паузу,
  // по стоявшему на паузе продолжает
  if(!seekClicked)
  {
    if(activeHref==href && voicePlayer->state()==QMediaPlayer::PlayingState)
    {
      voicePlayer->pause();
      return;
    }
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

  // Мотание по клику: позиция от доли, длительность известна
  // только после загрузки медиа. Без длительности просто играет
  if(seekClicked && voicePlayer->duration()>0)
    voicePlayer->setPosition(static_cast<qint64>(seekFraction*voicePlayer->duration()));

  // На паузе клик по полосе только двигает позицию
  if(seekClicked && voicePlayer->state()==QMediaPlayer::PausedState)
  {
    if(voicePlayer->duration()>0)
      textArea->document()->markContentsDirty(bubblePos, 1);

    return;
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


QString AudioFormatter::renameVoiceFile(const QString &workDir,
                                                 const QString &oldHref,
                                                 const QString &stem,
                                                 QString *error)
{
    const QString done=QString();

    if(!VoiceNoteHelper::isVoiceHref(oldHref))
    {
        if(error!=nullptr)
            *error=tr("Not a voice link");

        return done;
    }

    const QString oldName=VoiceNoteHelper::fileNameFromHref(oldHref);
    const QString oldPath=VoiceNoteHelper::resolveFilePath(workDir, oldHref);

    if(oldName.isEmpty() || !QFileInfo(oldPath).isFile())
    {
        if(error!=nullptr)
            *error=tr("Voice file not found");

        return done;
    }

    // Без путей и запретных символов, как имя в диалоге записи.
    // Расширение всегда от старого файла: голос должен остаться
    // проигрываемым
    QString clean=QFileInfo(stem.trimmed()).fileName().trimmed();
    clean.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]")));
    clean=clean.trimmed();

    static const QRegularExpression knownExt(
        QStringLiteral("\\.(opus|mp3|wav|oga|ogg)$"),
        QRegularExpression::CaseInsensitiveOption);
    clean.remove(knownExt);
    clean=clean.trimmed().left(100);

    if(clean.isEmpty())
    {
        if(error!=nullptr)
            *error=tr("Empty file name");

        return done;
    }

    const QString newName=clean+QStringLiteral(".")+QFileInfo(oldName).suffix();

    // То же имя не отказ: переименовывать нечего
    if(newName==oldName)
        return VoiceNoteHelper::makeHref(oldName);

    const QString newPath=VoiceNoteHelper::resolveFilePath(
        workDir, VoiceNoteHelper::makeHref(newName));

    if(QFileInfo(newPath).exists())
    {
        if(error!=nullptr)
            *error=tr("File already exists: %1").arg(newName);

        return done;
    }

    if(!QFile::rename(oldPath, newPath))
    {
        if(error!=nullptr)
            *error=tr("Can not rename voice file: %1").arg(oldName);

        return done;
    }

    return VoiceNoteHelper::makeHref(newName);
}


void AudioFormatter::onRenameVoiceClicked(void)
{
    const QString href=voiceHrefUnderCursor();

    if(href.isEmpty())
        return;

    const QString oldName=VoiceNoteHelper::fileNameFromHref(href);

    bool ok=false;
    const QString stem=QInputDialog::getText(editor,
                                             tr("Rename voice note"),
                                             tr("New file name:"),
                                             QLineEdit::Normal,
                                             QFileInfo(oldName).completeBaseName(),
                                             &ok);

    if(!ok)
        return;

    QString error;
    const QString newHref=renameVoiceFile(editor->getWorkDirectory(),
                                          href, stem, &error);

    if(newHref.isEmpty())
    {
        QMessageBox::information(editor,
                                 tr("Rename voice note"),
                                 error);
        return;
    }

    if(newHref==href)
        return;

    // Играющий бабл перед переименованием останавливается:
    // медиа-источник сменил путь
    if(activeHref==href)
        stopVoicePlayback();

    // Текст обновляется: у бабла свойства, у сырой ссылки href.
    // Длительность и имя в бабле: имя тоже новое
    const QString newName=VoiceNoteHelper::fileNameFromHref(newHref);

    if(findBubblePos(href)>=0)
    {
        if(!selectVoiceBubbleRange(href))
            return;

        QTextCharFormat format;
        format.setProperty(VoiceNoteHelper::BubblePropHref, newHref);
        format.setProperty(VoiceNoteHelper::BubblePropFileName, newName);
        textArea->textCursor().mergeCharFormat(format);
    }
    else
    {
        if(!selectVoiceLinkRange(href))
            return;

        QTextCharFormat format;
        format.setAnchorHref(newHref);
        textArea->textCursor().mergeCharFormat(format);
    }

    if(globalParameters.getStatusBar()!=nullptr)
        globalParameters.getStatusBar()->showMessage(
            tr("Renamed to %1").arg(newName), 3000);
}


QString AudioFormatter::moveVoiceFileToDir(const QString &srcDir,
                                                  const QString &oldHref,
                                                  const QString &dstDir,
                                                  QString *error)
{
    const QString done=QString();

    if(!VoiceNoteHelper::isVoiceHref(oldHref))
    {
        if(error!=nullptr)
            *error=tr("Not a voice link");

        return done;
    }

    const QString fileName=VoiceNoteHelper::fileNameFromHref(oldHref);
    const QString srcPath=VoiceNoteHelper::resolveFilePath(srcDir, oldHref);

    if(fileName.isEmpty() || !QFileInfo(srcPath).isFile())
    {
        if(error!=nullptr)
            *error=tr("Voice file not found");

        return done;
    }

    if(dstDir.isEmpty() || !QDir(dstDir).exists())
    {
        if(error!=nullptr)
            *error=tr("Target note directory not found");

        return done;
    }

    // Тот же каталог: переносить нечего, ссылка прежняя
    if(QDir(srcDir).absolutePath()==QDir(dstDir).absolutePath())
        return oldHref;

    // Свободное имя в цели: занято значит суффикс -2, -3...
    QString finalName=fileName;

    if(QFileInfo(QDir(dstDir).absoluteFilePath(finalName)).exists())
    {
        const int dotPos=fileName.lastIndexOf(QLatin1Char('.'));
        const QString stem=(dotPos>0) ? fileName.left(dotPos) : fileName;
        const QString ext=(dotPos>0) ? fileName.mid(dotPos) : QString();
        finalName.clear();

        for(int attempt=2; attempt<1000; ++attempt)
        {
            const QString candidate=stem+QStringLiteral("-")+
                                    QString::number(attempt)+ext;

            if(!QFileInfo(QDir(dstDir).absoluteFilePath(candidate)).exists())
            {
                finalName=candidate;
                break;
            }
        }

        if(finalName.isEmpty())
        {
            if(error!=nullptr)
                *error=tr("No free file name in target note");

            return done;
        }
    }

    const QString dstPath=QDir(dstDir).absoluteFilePath(finalName);

    // Переезд одним переименованием; не вышло (разные ФС) значит
    // копия плюс удаление источника
    if(!QFile::rename(srcPath, dstPath))
    {
        if(!QFile::copy(srcPath, dstPath))
        {
            if(error!=nullptr)
                *error=tr("Can not move voice file: %1").arg(fileName);

            return done;
        }

        QFile::remove(srcPath);
    }

    return VoiceNoteHelper::makeHref(finalName);
}


bool AudioFormatter::appendVoiceLinkToTextFile(const QString &textFileName,
                                              const QString &href,
                                              qint64 durationMs,
                                              QString *error)
{
    if(!VoiceNoteHelper::isVoiceHref(href))
    {
        if(error!=nullptr)
            *error=tr("Not a voice link");

        return false;
    }

    if(textFileName.isEmpty())
    {
        if(error!=nullptr)
            *error=tr("Target note text file unknown");

        return false;
    }

    const QString paragraph=QStringLiteral("<p><a href=\"")+
        href.toHtmlEscaped()+QStringLiteral("\">")+
        VoiceNoteHelper::linkText(durationMs).toHtmlEscaped()+
        QStringLiteral("</a></p>");

    // Файла еще нет (новая заметка): минимальный каркас со ссылкой
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


qint64 AudioFormatter::voiceDurationUnderCursor(const QString &href) const
{
    if(href.isEmpty())
        return 0;

    QTextDocument *doc=textArea->document();

    // Бабл: длительность лежит в свойствах формата
    const int bubblePos=findBubblePos(href);

    if(bubblePos>=0)
    {
        QTextCursor probe(doc);
        probe.setPosition(bubblePos+1);
        const qint64 duration=probe.charFormat().property(
            VoiceNoteHelper::BubblePropDurationMs).toLongLong();

        return qMax<qint64>(0, duration);
    }

    // Сырая ссылка: парсинг видимого текста [voice M:SS]
    for(QTextBlock block=doc->firstBlock(); block.isValid(); block=block.next())
        for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
        {
            const QTextFragment fragment=it.fragment();

            if(!fragment.isValid())
                continue;

            if(fragment.charFormat().anchorHref()!=href)
                continue;

            const qint64 duration=VoiceNoteHelper::parseDuration(fragment.text());

            if(duration>=0)
                return duration;
        }

    return 0;
}


void AudioFormatter::onMoveVoiceClicked(void)
{
    const QString href=voiceHrefUnderCursor();

    if(href.isEmpty())
        return;

    const QString srcDir=editor->getWorkDirectory();
    const QString srcPath=VoiceNoteHelper::resolveFilePath(srcDir, href);

    if(!QFileInfo(srcPath).isFile())
    {
        QMessageBox::information(editor,
                                 tr("Move voice note"),
                                 tr("Voice file not found"));
        return;
    }

    const qint64 durationMs=voiceDurationUnderCursor(href);
    const QString currentId=editor->getMiscField(QStringLiteral("id"));

    // Своя запись из выдачи исключена самим пикером
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
                                 tr("Move voice note"),
                                 tr("Notes tree unavailable"));
        return;
    }

    Record *target=static_cast<KnowTreeModel *>(treeView->model())->getRecord(pickedId);

    if(target==nullptr)
    {
        QMessageBox::information(editor,
                                 tr("Move voice note"),
                                 tr("Target note not found"));
        return;
    }

    // Шифрованная цель правится только через редактор с ключом:
    // прямая дописка в файл сломала бы шифротекст
    if(target->getField(QStringLiteral("crypt"))==QStringLiteral("1"))
    {
        QMessageBox::information(editor,
                                 tr("Move voice note"),
                                 tr("Can not move into encrypted note"));
        return;
    }

    const QString targetName=target->getField(QStringLiteral("name"));
    const QString fileName=VoiceNoteHelper::fileNameFromHref(href);

    if(QMessageBox::question(editor,
                             tr("Move voice note"),
                             tr("Move voice note %1 to note \"%2\"? Link will be removed from current note.").arg(fileName).arg(targetName),
                             QMessageBox::Yes | QMessageBox::No,
                             QMessageBox::No)!=QMessageBox::Yes)
        return;

    // Пути собираются из полей как в RecordTableController:
    // прямые геттеры Record защищены
    if(target->getField(QStringLiteral("dir")).isEmpty() ||
       target->getField(QStringLiteral("file")).isEmpty())
    {
        QMessageBox::information(editor,
                                 tr("Move voice note"),
                                 tr("Target note text file unknown"));
        return;
    }

    const QString dstDir=mytetraConfig.get_tetradir()+
        QStringLiteral("/base/")+
        target->getField(QStringLiteral("dir"));
    const QString dstTextFile=dstDir+
        QStringLiteral("/")+target->getField(QStringLiteral("file"));

    QString error;
    const QString newHref=moveVoiceFileToDir(srcDir, href, dstDir, &error);

    if(newHref.isEmpty())
    {
        QMessageBox::information(editor,
                                 tr("Move voice note"),
                                 error);
        return;
    }

    // Ссылка дописывается в закрытую заметку напрямую.
    // Не влезла: файл возвращается назад, текст источника не тронут
    if(!appendVoiceLinkToTextFile(dstTextFile,
                                  newHref, durationMs, &error))
    {
        QString backError;
        moveVoiceFileToDir(dstDir, newHref, srcDir, &backError);

        QMessageBox::information(editor,
                                 tr("Move voice note"),
                                 error);
        return;
    }

    // Играющий бабл перед вырезанием останавливается:
    // медиа-источник сменил каталог
    if(activeHref==href)
        stopVoicePlayback();

    if(findBubblePos(href)>=0)
        selectVoiceBubbleRange(href);
    else
        selectVoiceLinkRange(href);

    textArea->textCursor().removeSelectedText();

    if(globalParameters.getStatusBar()!=nullptr)
        globalParameters.getStatusBar()->showMessage(
            tr("Moved to \"%1\"").arg(targetName), 3000);
}


void AudioFormatter::onPlayVoiceClicked(void)
{  const QString href=voiceHrefUnderCursor();

  if(href.isEmpty())
    return;

  // Бабл играет на месте, старая ссылка открывает плеер.
  // Из меню точки клика нет: передается пустая, клик считается по кружку
  if(findBubblePos(href)>=0)
    onBubbleClicked(href, QPointF(-1.0, -1.0));
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
