#ifndef AUDIOFORMATTER_H
#define AUDIOFORMATTER_H

#include <QMediaPlayer>

#include "Formatter.h"
#include "VoiceBubbleObject.h"

class QTimer;

// Голосовые заметки: запись с микрофона в Opus/MP3, файл
// в каталоге записи, в текст вставляется инлайн-бабл.
// В файле заметки хранится ссылка [voice m:ss], при загрузке
// ссылки превращаются в баблы, при сохранении обратно в ссылки

class AudioFormatter : public Formatter
{
  Q_OBJECT

public:
  AudioFormatter();

  // Рисовальщик баблов, регистрируется в документе редактора
  VoiceBubbleObject *bubbleHandler(void);

  // Состояние воспроизведения для рисовальщика бабла
  void bubblePlaybackState(const QString &href, bool &playing,
                           qint64 &posMs, qint64 &totalMs);

  // Вставить бабл в позицию курсора
  void insertBubble(const QString &href, qint64 durationMs,
                    const QString &fileName);

  // Заменить голосовые ссылки живого документа баблами.
  // Вызывается после загрузки текста, до сброса modified
  void materializeBubbles(void);

  // HTML для сохранения: баблы подменены ссылками.
  // Живой документ не трогается
  QString exportHtml(void);

public slots:

  void onMicRecordClicked(void);

  // Клик по баблу в тексте: play/pause на месте
  void onBubbleClicked(const QString &href);

  // Контекстное меню на голосовой ссылке или бабле
  void onPlayVoiceClicked(void);
  void onShowVoiceInFolder(void);
  void onDeleteVoiceClicked(void);

private slots:

  void onVoiceTick(void);
  void onVoiceStateChanged(QMediaPlayer::State state);

private:

  // Ссылка или бабл под курсором, пусто если курсор не на голосовой
  QString voiceHrefUnderCursor(void) const;

  // Выделить весь текст ссылки под курсором. Ложь если не на ссылке
  bool selectVoiceLinkRange(const QString &href);

  // Выделить бабл под курсором. Ложь если курсор не на бабле
  bool selectVoiceBubbleRange(const QString &href);

  // Позиция бабла со ссылкой в документе, -1 если нет
  int findBubblePos(const QString &href) const;

  // Есть ли баблы в документе
  bool documentHasBubbles(void) const;

  // Остановить воспроизведение и погасить активный бабл
  void stopVoicePlayback(void);

  VoiceBubbleObject *bubbleObject;

  QMediaPlayer *voicePlayer=nullptr;
  QTimer *voiceTick=nullptr;
  QString activeHref;
  bool wasPlaying=false;
};

#endif // AUDIOFORMATTER_H
