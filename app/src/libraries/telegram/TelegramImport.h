#ifndef TELEGRAMIMPORT_H
#define TELEGRAMIMPORT_H

#include <QString>
#include <QList>
#include <QDateTime>
#include <functional>

class KnowTreeModel;
class QModelIndex;

// Импорт JSON-экспорта Telegram Desktop (result.json).
// Карта: Telegram -> <чат> -> <год> -> заметка <ГГГГ-ММ>.
// Тексты сущностей преобразуются в HTML, медиа (фото, файлы,
// голосовые) кладутся аттачами к заметке месяца.


struct TelegramMedia
{
  QString kind; // "photo", "file", "voice"
  QString srcPath; // абсолютный путь в каталоге экспорта
  QString fileName; // имя файла в каталоге заметки
  QString mime; // mime файла-документа, пусто для photo/voice
  QString extra; // длительность голосового
};


struct TelegramMessage
{
  qint64 id=0;
  qint64 unixtime=0;
  QString from;
  bool forwarded=false;
  QString fwdFrom;
  qint64 replyTo=0;
  QString plain; // текст без разметки (для цитат ответов)
  QString html; // текст в HTML для тела заметки
  QList<TelegramMedia> media;
};


struct TelegramChat
{
  QString name;
  QList<TelegramMessage> messages;
};


struct TelegramImportResult
{
  int branches=0;
  int notes=0;
  int attaches=0; // строк аттачей (оригиналы картинок, документы)
  int previews=0; // сглаженных превью для показа
  int skippedMedia=0;
};


// Разбор result.json. exportDir возвращается через chat? Нет:
// пути к медиа строятся вызывателем из каталога рядом с json
bool parseTelegramJson(const QString &fileName,
                       TelegramChat &chat,
                       QString &error);

// Разбор HTML-экспорта (messages.html). Та же структура чата
bool parseTelegramHtml(const QString &fileName,
                       TelegramChat &chat,
                       QString &error);

// По расширению: .html -> HTML, иначе JSON
bool parseTelegramExport(const QString &fileName,
                         TelegramChat &chat,
                         QString &error);

// Сущности text_entities (или строка) в HTML. Неизвестные типы
// отдаются плейнтекстом: данные не теряются при любом формате
QString telegramTextToHtml(const class QJsonValue &textValue);

// Импорт в модель (без сейва: вызыватель зовет saveKnowTree/save).
// progress(done, total) опционален, вызывается по сообщениям.
// Повторный импорт того же чата отклоняется: сначала удалить ветку
bool importTelegramChat(KnowTreeModel *model,
                        const TelegramChat &chat,
                        TelegramImportResult &result,
                        QString &error,
                        std::function<void(int, int)> progress={});

#endif // TELEGRAMIMPORT_H
