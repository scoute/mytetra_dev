#ifndef AUDIOFORMATTER_H
#define AUDIOFORMATTER_H

#include <QMediaPlayer>

class OpusPlayer;

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

  // Клик левее зоны кружка переключает, правее мотает.
  // Чистая функция для стенда: ширина бабла и смещение клика
  // от левого края, доля 0..1 на выходе
  enum BubbleClickAction
  {
    BubbleClickToggle,
    BubbleClickSeek
  };

  static BubbleClickAction bubbleClickAction(qreal bubbleWidth, qreal offsetX,
                                             qreal &seekFraction);

  // Заменить голосовые ссылки живого документа баблами.
  // Вызывается после загрузки текста, до сброса modified
  void materializeBubbles(void);

  // HTML для сохранения: баблы подменены ссылками.
  // Живой документ не трогается
  QString exportHtml(void);

public slots:

  void onMicRecordClicked(void);

  // Клик по баблу в тексте: по кружку play/pause,
  // по полосе правее мотает. Точка в координатах документа
  void onBubbleClicked(const QString &href, const QPointF &docPoint);

  // Контекстное меню на голосовой ссылке или бабле
  void onPlayVoiceClicked(void);
  void onRenameVoiceClicked(void);
  void onMoveVoiceClicked(void);
  void onShowVoiceInFolder(void);
  void onDeleteVoiceClicked(void);

private slots:

  void onVoiceTick(void);
  void onVoiceStateChanged(QMediaPlayer::State state);

  // Финал собственного Opus-проигрывателя бабла
  void onOpusVoiceFinished(void);

private:

  // Переименовать файл голосовой заметки: санитизация имени,
  // коллизии, QFile::rename. Возвращает новый href, пусто при
  // отказе (причина в error). То же имя не отказ: возвращается
  // старый href. Статика чтобы стенд гонял без редактора
  static QString renameVoiceFile(const QString &workDir,
                                 const QString &oldHref,
                                 const QString &stem,
                                 QString *error);

  // Переложить файл голосовой заметки в каталог другой записи.
  // Имя сохраняется, при коллизии свободное (суффикс -2, -3...).
  // Возвращает новый href, пусто при отказе (причина в error).
  // Тот же каталог не отказ: возвращается старый href, файл
  // не трогается. Статика чтобы стенд гонял без редактора
  static QString moveVoiceFileToDir(const QString &srcDir,
                                    const QString &oldHref,
                                    const QString &dstDir,
                                    QString *error);

  // Дописать голосовую ссылку в конец чужого text.html (закрытая
  // заметка правится напрямую, мимо редактора). Ложь при отказе
  // (причина в error). Файла нет: создается минимальный каркас
  static bool appendVoiceLinkToTextFile(const QString &textFileName,
                                        const QString &href,
                                        qint64 durationMs,
                                        QString *error);

  // Длительность голосовой ссылки из живого документа: у бабла
  // из свойств, у сырой ссылки парсингом текста. 0 если не
  // разобралась
  qint64 voiceDurationUnderCursor(const QString &href) const;

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

  // Собственный Opus-проигрыватель для .opus: бэкенд Qt opus
  // на Windows/Wine не умеет, здесь декодер свой
  OpusPlayer *opusVoicePlayer=nullptr;
  QTimer *voiceTick=nullptr;
  QString activeHref;
  bool wasPlaying=false;
};

#endif // AUDIOFORMATTER_H
