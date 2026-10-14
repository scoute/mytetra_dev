#ifndef AUDIOFORMATTER_H
#define AUDIOFORMATTER_H

#include "Formatter.h"

// Голосовые заметки: запись с микрофона в Opus/MP3, файл
// в каталоге записи, в текст ссылка [voice m:ss], клик
// и контекстное меню открывают плеер

class AudioFormatter : public Formatter
{
  Q_OBJECT

public:
  AudioFormatter();

public slots:

  void onMicRecordClicked(void);

  // Контекстное меню на голосовой ссылке
  void onPlayVoiceClicked(void);
  void onShowVoiceInFolder(void);
  void onDeleteVoiceClicked(void);

private:

  // Ссылка под курсором, пусто если курсор не на голосовой
  QString voiceHrefUnderCursor(void) const;

  // Выделить весь текст ссылки под курсором. Ложь если не на ссылке
  bool selectVoiceLinkRange(const QString &href);
};

#endif // AUDIOFORMATTER_H
