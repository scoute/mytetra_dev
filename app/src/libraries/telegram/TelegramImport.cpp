#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QMap>
#include <QModelIndex>
#include <QSet>
#include <QImage>
#include <QImageReader>
#include <QSize>
#include <QRegularExpression>
#include <QDebug>

#include "libraries/telegram/TelegramImport.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "models/recordTable/RecordTableData.h"
#include "models/attachTable/Attach.h"
#include "models/attachTable/AttachTableData.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "models/appConfig/AppConfig.h"


extern AppConfig mytetraConfig;


// Длинная сторона картинки в заметке: больше 1280 не держим
// (у Telegram почти все в 1280 и меньше). Один файл без дублей:
// в пределах лимита байт в байт, сверх лимита гладко ужимается
static const int telegramMaxSide=1280;

// Габариты для <img>: исходные, ужатые по длинной стороне
static QSize telegramDisplaySize(const QString &srcPath)
{
  QSize size=QImageReader(srcPath).size();

  if(!size.isValid())
    return QSize();

  if(size.width()>telegramMaxSide || size.height()>telegramMaxSide)
    size.scale(telegramMaxSide, telegramMaxSide, Qt::KeepAspectRatio);

  return size;
}


// Одна сущность в HTML. text заранее экранирован вызывателем
static QString entityToHtml(const QString &type, const QString &text,
                            const QJsonObject &entity)
{
  if(type==QStringLiteral("bold"))
    return QStringLiteral("<b>")+text+QStringLiteral("</b>");

  if(type==QStringLiteral("italic"))
    return QStringLiteral("<i>")+text+QStringLiteral("</i>");

  if(type==QStringLiteral("underline"))
    return QStringLiteral("<u>")+text+QStringLiteral("</u>");

  if(type==QStringLiteral("strikethrough") || type==QStringLiteral("spoiler"))
    return QStringLiteral("<s>")+text+QStringLiteral("</s>");

  if(type==QStringLiteral("code"))
    return QStringLiteral("<code>")+text+QStringLiteral("</code>");

  if(type==QStringLiteral("pre"))
    return QStringLiteral("<pre>")+text+QStringLiteral("</pre>");

  if(type==QStringLiteral("text_link"))
  {
    const QString href=entity.value(QStringLiteral("href")).toString();

    if(!href.isEmpty())
      return QStringLiteral("<a href=\"")+href.toHtmlEscaped()+
             QStringLiteral("\">")+text+QStringLiteral("</a>");
  }

  if(type==QStringLiteral("link") || type==QStringLiteral("email") ||
     type==QStringLiteral("phone") || type==QStringLiteral("mention") ||
     type==QStringLiteral("hashtag") || type==QStringLiteral("bot_command"))
    return QStringLiteral("<a href=\"")+text+QStringLiteral("\">")+text+QStringLiteral("</a>");

  // mention_name, custom_emoji, blockquote и будущие типы:
  // текст сохраняется, разметка опускается
  return text;
}


QString telegramTextToHtml(const QJsonValue &textValue)
{
  if(textValue.isString())
    return textValue.toString().toHtmlEscaped();

  if(!textValue.isArray())
    return QString();

  QString html;

  foreach(const QJsonValue &value, textValue.toArray())
  {
    if(value.isString())
    {
      html+=value.toString().toHtmlEscaped();
      continue;
    }

    if(!value.isObject())
      continue;

    const QJsonObject entity=value.toObject();
    const QString text=entity.value(QStringLiteral("text")).toString().toHtmlEscaped();
    html+=entityToHtml(entity.value(QStringLiteral("type")).toString(), text, entity);
  }

  // Переносы строк экспорта в HTML-абзацы редактора
  html.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));

  // Неразрывные пробелы Telegram слипают текст в одну строку:
  // QTextEdit не переносит по ним. Меняются на обычные
  html.replace(QChar::Nbsp, QLatin1Char(' '));

  return html;
}


static QString telegramPlainText(const QJsonValue &textValue)
{
  if(textValue.isString())
    return textValue.toString().replace(QChar::Nbsp, QLatin1Char(' '));

  if(!textValue.isArray())
    return QString();

  QString plain;

  foreach(const QJsonValue &value, textValue.toArray())
  {
    if(value.isString())
      plain+=value.toString();
    else if(value.isObject())
      plain+=value.toObject().value(QStringLiteral("text")).toString();
  }

  plain.replace(QChar::Nbsp, QLatin1Char(' '));

  return plain;
}


// Сущности плейнтекста HTML-экспорта: имена чатов и авторов.
// Порядок важен: &amp; последним, иначе двойное декодирование
static QString decodeTelegramEntities(QString text)
{
  text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
  text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
  text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
  text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
  text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));

  static QRegularExpression numericEntity("&#(\\d+);");
  QRegularExpressionMatchIterator it=numericEntity.globalMatch(text);

  // С конца чтобы позиции не съезжали
  QList<QPair<int, int>> spans;
  QStringList chars;

  while(it.hasNext())
  {
    const QRegularExpressionMatch match=it.next();
    spans.prepend(qMakePair(match.capturedStart(), match.capturedLength()));
    chars.prepend(QString(QChar(match.captured(1).toInt())));
  }

  for(int i=0; i<spans.size(); ++i)
    text.replace(spans.at(i).first, spans.at(i).second, chars.at(i));

  text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
  text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
  text.replace(QChar::Nbsp, QLatin1Char(' '));

  return text;
}


// Внутренний HTML текста сообщения в HTML-экспорте уже экранирован.
// Оставляются ссылки и простая разметка, остальное вычищается.
// &nbsp; меняется на пробел: иначе QTextEdit не переносит
static QString sanitizeTelegramHtmlFragment(const QString &inner)
{
  static QRegularExpression tagRe("<(/?)([A-Za-z0-9]+)((?:\"[^\"]*\"|'[^']*'|[^\"'>])*)>");

  QString result;
  int pos=0;
  QRegularExpressionMatchIterator it=tagRe.globalMatch(inner);

  while(it.hasNext())
  {
    const QRegularExpressionMatch match=it.next();
    result+=inner.mid(pos, match.capturedStart()-pos);
    pos=match.capturedEnd();

    const bool closing=!match.captured(1).isEmpty();
    const QString tag=match.captured(2).toLower();

    if(tag==QStringLiteral("br"))
    {
      if(!closing)
        result+=QStringLiteral("<br/>");

      continue;
    }

    if(tag==QStringLiteral("a"))
    {
      if(closing)
      {
        result+=QStringLiteral("</a>");
        continue;
      }

      static QRegularExpression hrefRe("href\\s*=\\s*\"([^\"]*)\"");
      const QRegularExpressionMatch hrefMatch=hrefRe.match(match.captured(3));

      if(hrefMatch.hasMatch())
        result+=QStringLiteral("<a href=\"")+
                hrefMatch.captured(1).toHtmlEscaped()+
                QStringLiteral("\">");

      continue;
    }

    if(tag==QStringLiteral("b") || tag==QStringLiteral("strong") ||
       tag==QStringLiteral("i") || tag==QStringLiteral("em") ||
       tag==QStringLiteral("u") || tag==QStringLiteral("s") ||
       tag==QStringLiteral("strike") || tag==QStringLiteral("code") ||
       tag==QStringLiteral("pre") || tag==QStringLiteral("blockquote"))
    {
      result+=closing ? QStringLiteral("</")+tag+QStringLiteral(">") :
                        QStringLiteral("<")+tag+QStringLiteral(">");
      continue;
    }

    // Прочее (span, div, img превью) выкидывается, текст остается
  }

  result+=inner.mid(pos);
  result.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));

  return result;
}


// Извлечение сбалансированного <div>...</div> с позиции открывающего.
// Возвращает внутренность (без внешних тегов), endPos за закрывающим
static QString extractBalancedDiv(const QString &html, int openPos, int &endPos)
{
  endPos=-1;

  int depth=0;
  int i=openPos;

  while(i<html.size())
  {
    const int openIdx=html.indexOf(QStringLiteral("<div"), i);
    const int closeIdx=html.indexOf(QStringLiteral("</div>"), i);

    if(closeIdx<0)
      return QString();

    if(openIdx>=0 && openIdx<closeIdx)
    {
      ++depth;
      i=openIdx+4;
    }
    else
    {
      --depth;
      i=closeIdx+6;

      if(depth==0)
      {
        // Конец открывающего тега (первый '>' после openPos)
        const int innerStart=html.indexOf(QLatin1Char('>'), openPos)+1;
        endPos=i;
        return html.mid(innerStart, closeIdx-innerStart);
      }
    }
  }

  return QString();
}


// Дата заголовка сообщения: "17.10.2026 20:34:48 GMT+04:00".
// Смещение вычитается: unixtime всегда UTC
static qint64 parseTelegramHtmlDate(const QString &title)
{
  static QRegularExpression dateRe("(\\d\\d)\\.(\\d\\d)\\.(\\d\\d\\d\\d) "
                                   "(\\d\\d):(\\d\\d):(\\d\\d) GMT([+-])(\\d\\d):(\\d\\d)");
  const QRegularExpressionMatch match=dateRe.match(title);

  if(!match.hasMatch())
    return -1;

  const QDateTime utc(QDate(match.captured(3).toInt(),
                            match.captured(2).toInt(),
                            match.captured(1).toInt()),
                      QTime(match.captured(4).toInt(),
                            match.captured(5).toInt(),
                            match.captured(6).toInt()),
                      Qt::UTC);

  if(!utc.isValid())
    return -1;

  const int offsetSecs=(match.captured(8).toInt()*60+match.captured(9).toInt())*60;

  return utc.toSecsSinceEpoch()-(match.captured(7)==QStringLiteral("+") ? offsetSecs : -offsetSecs);
}


static QString imageMimeByExtension(const QString &fileName)
{
  const QString ext=QFileInfo(fileName).suffix().toLower();

  if(ext==QStringLiteral("png"))
    return QStringLiteral("image/png");

  if(ext==QStringLiteral("jpg") || ext==QStringLiteral("jpeg"))
    return QStringLiteral("image/jpeg");

  if(ext==QStringLiteral("gif"))
    return QStringLiteral("image/gif");

  if(ext==QStringLiteral("webp"))
    return QStringLiteral("image/webp");

  if(ext==QStringLiteral("bmp"))
    return QStringLiteral("image/bmp");

  return QString();
}


bool parseTelegramHtml(const QString &fileName,
                       TelegramChat &chat,
                       QString &error)
{
  QFile file(fileName);

  if(!file.open(QIODevice::ReadOnly))
  {
    error="Can not open file: "+fileName;
    return false;
  }

  const QString html=QString::fromUtf8(file.readAll());
  const QDir exportDir(QFileInfo(fileName).absolutePath());

  static QRegularExpression chatRe("<div class=\"page_header\">.*?"
                                   "<div class=\"text bold\">\\s*(.*?)\\s*</div>",
                                   QRegularExpression::DotMatchesEverythingOption);
  const QRegularExpressionMatch chatMatch=chatRe.match(html);

  if(!chatMatch.hasMatch())
  {
    error="No chat header in HTML export";
    return false;
  }

  chat.name=decodeTelegramEntities(chatMatch.captured(1).trimmed());

  if(chat.name.isEmpty())
  {
    error="HTML export has no chat name";
    return false;
  }

  QString lastFrom;
  const QStringList chunks=html.split(QStringLiteral("<div class=\"message "));

  for(int ci=1; ci<chunks.size(); ++ci)
  {
    const QString &chunk=chunks.at(ci);

    // Служебные (разделители дат, звонки) пропускаются
    if(chunk.startsWith(QStringLiteral("service")))
      continue;

    static QRegularExpression idRe("id=\"message(-?\\d+)\"");
    const QRegularExpressionMatch idMatch=idRe.match(chunk);

    if(!idMatch.hasMatch())
      continue;

    TelegramMessage message;
    message.id=idMatch.captured(1).toLongLong();

    // Дата из заголовка; без нее сообщение бесполезно
    static QRegularExpression dateRe("title=\"(\\d\\d\\.\\d\\d\\.\\d\\d\\d\\d \\d\\d:\\d\\d:\\d\\d GMT[+-]\\d\\d:\\d\\d)\"");
    const QRegularExpressionMatch dateMatch=dateRe.match(chunk);
    message.unixtime=parseTelegramHtmlDate(dateMatch.hasMatch() ? dateMatch.captured(1) : QString());

    if(message.unixtime<0)
      continue;

    QString rest=chunk;

    // Пересланное вынимается первым: внутри свой from_name
    const int fwdIdx=rest.indexOf(QStringLiteral("<div class=\"forwarded body\">"));

    if(fwdIdx>=0)
    {
      const int divPos=rest.lastIndexOf(QStringLiteral("<div"), fwdIdx);
      int fwdEnd=-1;
      const QString fwdInner=extractBalancedDiv(rest, divPos, fwdEnd);

      if(fwdEnd>0)
      {
        static QRegularExpression fwdFromRe("<div class=\"from_name\">\\s*(.*?)\\s*(?:<span.*?</span>)?\\s*</div>",
                                            QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch fwdFromMatch=fwdFromRe.match(fwdInner);

        if(fwdFromMatch.hasMatch())
        {
          message.forwarded=true;
          message.fwdFrom=decodeTelegramEntities(
            fwdFromMatch.captured(1).remove(QRegularExpression("<[^>]*>")).trimmed());
        }

        static QRegularExpression fwdTextRe("<div class=\"text\">([\\s\\S]*?)</div>");
        const QRegularExpressionMatch fwdTextMatch=fwdTextRe.match(fwdInner);

        if(fwdTextMatch.hasMatch())
        {
          message.plain=decodeTelegramEntities(
            fwdTextMatch.captured(1).remove(QRegularExpression("<[^>]*>")));
          message.html=sanitizeTelegramHtmlFragment(fwdTextMatch.captured(1));
        }

        rest=rest.left(divPos)+rest.mid(fwdEnd);
      }
    }

    // Ответ: ссылка на исходное сообщение
    static QRegularExpression replyRe("href=\"#go_to_message(-?\\d+)\"");
    const QRegularExpressionMatch replyMatch=replyRe.match(rest);

    if(replyMatch.hasMatch())
      message.replyTo=replyMatch.captured(1).toLongLong();

    // Автор: в склейках отсутствует, наследуется от предыдущего
    static QRegularExpression fromRe("<div class=\"from_name\">\\s*(.*?)\\s*</div>",
                                     QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch fromMatch=fromRe.match(rest);

    if(fromMatch.hasMatch())
    {
      message.from=decodeTelegramEntities(
        fromMatch.captured(1).remove(QRegularExpression("<[^>]*>")).trimmed());
      lastFrom=message.from;
    }
    else
      message.from=lastFrom;

    // Текст сообщения (у пересланного уже взят из пересланного блока)
    if(message.html.isEmpty())
    {
      static QRegularExpression textRe("<div class=\"text\">([\\s\\S]*?)</div>");
      const QRegularExpressionMatch textMatch=textRe.match(rest);

      if(textMatch.hasMatch())
      {
        message.plain=decodeTelegramEntities(
          textMatch.captured(1).remove(QRegularExpression("<[^>]*>")));
        message.html=sanitizeTelegramHtmlFragment(textMatch.captured(1));
      }
    }

    // Медиа: якоря без вложенных ссылок, нежадный захват работает
    static QRegularExpression anchorRe("<a class=\"([^\"]*)\" href=\"([^\"]*)\"([\\s\\S]*?)</a>");
    QRegularExpressionMatchIterator ait=anchorRe.globalMatch(rest);

    while(ait.hasNext())
    {
      const QRegularExpressionMatch anchor=ait.next();
      const QString anchorClass=anchor.captured(1);
      const QString href=anchor.captured(2);
      const QString anchorInner=anchor.captured(3);

      // Ссылки "this message" у ответов и стикеры-эмодзи в тексте пропускаются:
      // первые служебные, вторые ведут на несуществующий файл
      if(href.startsWith(QLatin1Char('#')) || anchorClass.contains(QStringLiteral("sticker")))
        continue;

      TelegramMedia media;
      media.srcPath=exportDir.absoluteFilePath(href);
      media.fileName=QFileInfo(href).fileName();

      if(anchorClass.contains(QStringLiteral("photo")))
      {
        media.kind=QStringLiteral("photo");
      }
      else if(anchorClass.contains(QStringLiteral("voice")))
      {
        media.kind=QStringLiteral("voice");

        static QRegularExpression durRe("<div class=\"status details\">\\s*(\\d+:\\d\\d)\\s*</div>");
        const QRegularExpressionMatch durMatch=durRe.match(anchorInner);
        media.extra=durMatch.hasMatch() ? durMatch.captured(1) : QString();
      }
      else
      {
        media.kind=QStringLiteral("file");

        static QRegularExpression titleRe("<div class=\"title bold\">\\s*(.*?)\\s*</div>",
                                          QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch titleMatch=titleRe.match(anchorInner);

        if(titleMatch.hasMatch())
          media.fileName=decodeTelegramEntities(titleMatch.captured(1).trimmed());

        media.mime=imageMimeByExtension(media.fileName);

        if(media.mime.startsWith(QStringLiteral("image/")))
          media.kind=QStringLiteral("photo");
      }

      message.media.append(media);
    }

    // Пустые остатки без текста и медиа не тащим
    if(message.plain.isEmpty() && message.media.isEmpty())
      continue;

    chat.messages.append(message);
  }

  if(chat.messages.isEmpty())
  {
    error="No messages in HTML export";
    return false;
  }

  return true;
}


bool parseTelegramExport(const QString &fileName,
                         TelegramChat &chat,
                         QString &error)
{
  if(fileName.endsWith(QStringLiteral(".html"), Qt::CaseInsensitive) ||
     fileName.endsWith(QStringLiteral(".htm"), Qt::CaseInsensitive))
    return parseTelegramHtml(fileName, chat, error);

  return parseTelegramJson(fileName, chat, error);
}


bool parseTelegramJson(const QString &fileName,
                       TelegramChat &chat,
                       QString &error)
{
  QFile file(fileName);

  if(!file.open(QIODevice::ReadOnly))
  {
    error="Can not open file: "+fileName;
    return false;
  }

  QJsonParseError parseError;
  const QJsonDocument doc=QJsonDocument::fromJson(file.readAll(), &parseError);

  if(parseError.error!=QJsonParseError::NoError || !doc.isObject())
  {
    error="Bad Telegram JSON: "+parseError.errorString();
    return false;
  }

  const QJsonObject root=doc.object();
  chat.name=root.value(QStringLiteral("name")).toString().trimmed().replace(QChar::Nbsp, QLatin1Char(' '));

  if(chat.name.isEmpty())
  {
    error="Telegram JSON has no chat name";
    return false;
  }

  const QDir exportDir(QFileInfo(fileName).absolutePath());

  foreach(const QJsonValue &value, root.value(QStringLiteral("messages")).toArray())
  {
    if(!value.isObject())
      continue;

    const QJsonObject msg=value.toObject();

    // Служебные записи (сервис, звонки без type=message) пропускаются
    if(msg.value(QStringLiteral("type")).toString()!=QStringLiteral("message"))
      continue;

    TelegramMessage message;
    message.id=msg.value(QStringLiteral("id")).toVariant().toLongLong();
    message.unixtime=msg.value(QStringLiteral("date_unixtime")).toVariant().toLongLong();
    message.from=msg.value(QStringLiteral("from")).toString().replace(QChar::Nbsp, QLatin1Char(' '));

    if(msg.contains(QStringLiteral("forwarded_from")))
    {
      message.forwarded=true;
      message.fwdFrom=msg.value(QStringLiteral("forwarded_from")).toString().replace(QChar::Nbsp, QLatin1Char(' '));
    }

    if(msg.contains(QStringLiteral("reply_to_message_id")))
      message.replyTo=msg.value(QStringLiteral("reply_to_message_id")).toVariant().toLongLong();

    message.plain=telegramPlainText(msg.value(QStringLiteral("text")));
    message.html=telegramTextToHtml(msg.value(QStringLiteral("text")));

    // Фото: photos/..., документ: files/..., голосовое: voice_messages/...
    if(msg.contains(QStringLiteral("photo")))
    {
      TelegramMedia media;
      media.kind=QStringLiteral("photo");
      media.srcPath=exportDir.absoluteFilePath(msg.value(QStringLiteral("photo")).toString());
      media.fileName=QFileInfo(media.srcPath).fileName();
      message.media.append(media);
    }

    if(msg.contains(QStringLiteral("file")))
    {
      TelegramMedia media;
      media.kind=(msg.value(QStringLiteral("media_type")).toString()==QStringLiteral("voice_message"))
                 ? QStringLiteral("voice") : QStringLiteral("file");
      media.srcPath=exportDir.absoluteFilePath(msg.value(QStringLiteral("file")).toString());
      media.fileName=msg.value(QStringLiteral("file_name")).toString();

      if(media.fileName.isEmpty())
        media.fileName=QFileInfo(media.srcPath).fileName();

      media.mime=msg.value(QStringLiteral("mime_type")).toString();

      // Картинка, присланная документом: показывается в теле,
      // как обычное фото
      if(media.kind==QStringLiteral("file") && media.mime.startsWith(QStringLiteral("image/")))
        media.kind=QStringLiteral("photo");

      if(media.kind==QStringLiteral("voice"))
      {
        const int secs=msg.value(QStringLiteral("duration_seconds")).toInt();
        media.extra=QStringLiteral("%1:%2").arg(secs/60).arg(secs%60, 2, 10, QChar('0'));
      }

      message.media.append(media);
    }

    // Пустые служебные остатки без текста и медиа не тащим
    if(message.plain.isEmpty() && message.media.isEmpty())
      continue;

    chat.messages.append(message);
  }

  if(chat.messages.isEmpty())
  {
    error="No messages in Telegram JSON";
    return false;
  }

  return true;
}


// Ветка с именем среди детей родителя, иначе nullptr
static TreeItem *findChildBranch(TreeItem *parent, const QString &name)
{
  for(int i=0; i<parent->childCount(); ++i)
  {
    TreeItem *child=parent->child(i);

    if(child!=nullptr && child->getField(QStringLiteral("name")).trimmed()==name)
      return child;
  }

  return nullptr;
}


// Найти или создать ветку с именем. Счетчик результата растет
// только при создании
static TreeItem *ensureBranch(KnowTreeModel *model, TreeItem *parent,
                              const QString &name, TelegramImportResult &result)
{
  TreeItem *found=findChildBranch(parent, name);

  if(found!=nullptr)
    return found;

  QMap<QString, QString> fields;
  fields[QStringLiteral("id")]=getUniqueId();
  fields[QStringLiteral("name")]=name;
  model->addNewChildBranch(model->getIndexByItem(parent), fields);

  ++result.branches;

  return findChildBranch(parent, name);
}


// Один файл медиа в заметке: инлайн, ужатый или аттач.
// Имена резолвятся до построения HTML: takeUniqueName вызывается
// при сборке строки, ссылка и файл не разъедутся
struct TelegramPendingFile
{
  QString kind; // "inline", "scaled" или "attach"
  QString mediaKind; // "photo", "voice", "file"
  QString srcPath;
  QString fileName; // дисковое имя в каталоге заметки
};


// Уникальное имя в пределах набора. Каталог записи свежак,
// занятость только внутри заметки (месяца или дня)
static QString takeUniqueName(QSet<QString> &used, const QString &wanted)
{
  QString target=wanted;

  if(target.isEmpty())
    target=QStringLiteral("file");

  const int dotPos=target.lastIndexOf(QLatin1Char('.'));
  const QString stem=(dotPos>0) ? target.left(dotPos) : target;
  const QString ext=(dotPos>0) ? target.mid(dotPos) : QString();
  int attempt=2;

  while(used.contains(target))
  {
    target=stem+QStringLiteral("-")+QString::number(attempt)+ext;
    ++attempt;

    if(attempt>1000)
      return QString();
  }

  used.insert(target);

  return target;
}


// Пузырек одного сообщения: шапка, пересланное, ответ, текст, медиа.
// Без пилюли дня и заголовка месяца: их добавляет вызыватель
static QString telegramMessageBubble(const TelegramMessage &message,
                                     const QMap<qint64, QPair<QString, QString>> &snippets,
                                     QList<TelegramPendingFile> &pending,
                                     QSet<QString> &usedNames)
{
  const QDateTime dt=QDateTime::fromSecsSinceEpoch(message.unixtime);
  QString line=QStringLiteral("<div style=\"background-color:#eeffde;margin-top:6px;margin-bottom:6px;\">")+
               QStringLiteral("<font color=\"#8a8a8a\">")+
               dt.toString(QStringLiteral("dd.MM hh:mm"))+
               QStringLiteral("</font> <b>")+message.from.toHtmlEscaped()+
               QStringLiteral("</b>");

  if(message.forwarded && !message.fwdFrom.isEmpty())
    line+=QStringLiteral(" <i><font color=\"#2e7d32\">[переслано от ")+message.fwdFrom.toHtmlEscaped()+
          QStringLiteral("]</font></i>");

  if(message.replyTo!=0 && snippets.contains(message.replyTo))
  {
    const QPair<QString, QString> &quoted=snippets.value(message.replyTo);
    line+=QStringLiteral("<br/>&#8617; ответ ")+quoted.first.toHtmlEscaped()+
          QStringLiteral(": &laquo;")+quoted.second.toHtmlEscaped()+
          QStringLiteral("&raquo;");
  }

  if(!message.html.isEmpty())
    line+=QStringLiteral("<br/>")+message.html;

  foreach(const TelegramMedia &media, message.media)
  {
    if(media.kind==QStringLiteral("photo"))
    {
      // Один файл без дублей. Габариты заранее по исходнику:
      // редактор ничего не растягивает. Сверх лимита файл
      // гладко ужимается на месте
      const QSize naturalSize=QImageReader(media.srcPath).size();
      const bool needScale=naturalSize.isValid() &&
                           (naturalSize.width()>telegramMaxSide ||
                            naturalSize.height()>telegramMaxSide);
      const QSize displaySize=needScale ?
                              telegramDisplaySize(media.srcPath) :
                              naturalSize;
      const QString disk=takeUniqueName(usedNames, media.fileName);

      if(disk.isEmpty())
        continue;

      if(displaySize.isValid())
        line+=QStringLiteral("<br/><img src=\"")+disk.toHtmlEscaped()+
              QStringLiteral("\" width=\"")+QString::number(displaySize.width())+
              QStringLiteral("\" height=\"")+QString::number(displaySize.height())+
              QStringLiteral("\"/>");
      else
        line+=QStringLiteral("<br/><img src=\"")+disk.toHtmlEscaped()+
              QStringLiteral("\"/>");

      TelegramPendingFile pf;
      pf.kind=needScale ? QStringLiteral("scaled") : QStringLiteral("inline");
      pf.mediaKind=media.kind;
      pf.srcPath=media.srcPath;
      pf.fileName=disk;
      pending.append(pf);
    }
    else if(media.kind==QStringLiteral("voice"))
    {
      // Ссылка voicenote:// при открытии материализуется
      // в штатный плеер-пузырь
      const QString disk=takeUniqueName(usedNames, media.fileName);

      if(disk.isEmpty())
        continue;

      line+=QStringLiteral("<br/><a href=\"voicenote://")+disk.toHtmlEscaped()+
            QStringLiteral("\">[voice ")+media.extra+
            QStringLiteral("]</a>");

      TelegramPendingFile pf;
      pf.kind=QStringLiteral("inline");
      pf.mediaKind=media.kind;
      pf.srcPath=media.srcPath;
      pf.fileName=disk;
      pending.append(pf);
    }
    else
    {
      const QString disk=takeUniqueName(usedNames, media.fileName);

      if(disk.isEmpty())
        continue;

      line+=QStringLiteral("<br/>[Файл: ")+disk.toHtmlEscaped()+
            QStringLiteral("]");

      TelegramPendingFile pf;
      pf.kind=QStringLiteral("attach");
      pf.mediaKind=media.kind;
      pf.srcPath=media.srcPath;
      pf.fileName=disk;
      pending.append(pf);
    }
  }

  line+=QStringLiteral("</div>");

  return line;
}


// Запись заметки в ветку: текст, ctime, автор, файлы.
// Каталог берется у вставленной записи: insert может
// перегенерировать dir, предвычисленному верить нельзя
static bool writeTelegramNote(TreeItem *branch,
                              const QString &noteName,
                              const QString &html,
                              qint64 unixtime,
                              const QString &author,
                              QList<TelegramPendingFile> &pending,
                              TelegramImportResult &result,
                              QString &error)
{
  RecordTableData *table=branch->recordtableGetTableData();

  if(table==nullptr)
  {
    error="Branch has no record table: "+branch->getField(QStringLiteral("name"));
    return false;
  }

  // Тот же набор полей что McpTools::applyCreateNote при ручном создании
  Record record;
  record.switchToFat();
  record.setText(html);
  record.setField(QStringLiteral("id"), getUniqueId());
  record.setField(QStringLiteral("dir"), getUniqueId());
  record.setField(QStringLiteral("file"), QStringLiteral("text.html"));
  record.setField(QStringLiteral("name"), noteName);
  record.setField(QStringLiteral("author"), author);
  record.setField(QStringLiteral("url"), QString());
  record.setField(QStringLiteral("tags"), QStringLiteral("telegram"));
  record.setField(QStringLiteral("ctime"),
                  QDateTime::fromSecsSinceEpoch(unixtime).toString(QStringLiteral("yyyyMMddhhmmss")));

  const int pos=table->insertNewRecord(GlobalParameters::AddNewRecordBehavior::ADD_TO_END,
                                      0, record);

  if(pos<0)
  {
    error="Can not insert note: "+noteName;
    return false;
  }

  Record *stored=table->getRecord(pos);

  if(stored==nullptr)
  {
    error="Inserted note not found: "+noteName;
    return false;
  }

  const QString recordDir=mytetraConfig.get_tetradir()+QStringLiteral("/base/")+
                          stored->getField(QStringLiteral("dir"));
  AttachTableData *attachTable=stored->getAttachTablePointer();

  foreach(const TelegramPendingFile &pf, pending)
  {
    if(pf.kind==QStringLiteral("scaled"))
    {
      // Один файл: гладкое ужатие на месте. Оригинал сверх лимита
      // не храним: у Telegram почти все и так в 1280
      QImage img(pf.srcPath);

      if(img.isNull())
      {
        ++result.skippedMedia;
        continue;
      }

      img=img.scaled(telegramMaxSide, telegramMaxSide,
                     Qt::KeepAspectRatio, Qt::SmoothTransformation);

      if(img.save(QDir(recordDir).absoluteFilePath(pf.fileName), 0, 88))
        ++result.attaches;
      else
        ++result.skippedMedia;
    }
    else if(pf.kind==QStringLiteral("inline"))
    {
      // Копия байт в байт: оригинал не трогается, качество 1:1.
      // Размер показа ограничен width/height в <img>
      if(QFile::copy(pf.srcPath, QDir(recordDir).absoluteFilePath(pf.fileName)))
        ++result.attaches;
      else
        ++result.skippedMedia;
    }
    else if(attachTable!=nullptr)
    {
      Attach attach(QStringLiteral("file"), attachTable);
      attach.setField(QStringLiteral("id"), getUniqueId());
      attach.setField(QStringLiteral("fileName"), pf.fileName);

      if(attach.copyFileToBase(pf.srcPath))
      {
        attachTable->addAttach(attach);
        ++result.attaches;
      }
      else
        ++result.skippedMedia;
    }
    else
      ++result.skippedMedia;
  }

  ++result.notes;

  return true;
}


bool importTelegramChat(KnowTreeModel *model,
                        const TelegramChat &chat,
                        TelegramImportResult &result,
                        QString &error,
                        TelegramLayout layout,
                        std::function<void(int, int)> progress)
{
  if(model==nullptr)
  {
    error="No model";
    return false;
  }

  TreeItem *root=model->getItemById(QStringLiteral("0"));

  if(root==nullptr)
  {
    error="Model has no root";
    return false;
  }

  // Повторный импорт отклоняется: сначала удалить ветку чата.
  // Молчаливое слияние дало бы дубли
  TreeItem *top=findChildBranch(root, QStringLiteral("Telegram"));

  if(top!=nullptr &&
     findChildBranch(top, chat.name)!=nullptr)
  {
    error="Chat already imported: "+chat.name;
    return false;
  }

  // Карта ответов: id -> автор и начало текста
  QMap<qint64, QPair<QString, QString>> snippets;

  foreach(const TelegramMessage &message, chat.messages)
    snippets[message.id]=qMakePair(message.from,
                                   message.plain.left(80));

  if(top==nullptr)
    top=ensureBranch(model, root, QStringLiteral("Telegram"), result);

  TreeItem *chatBranch=ensureBranch(model, top, chat.name, result);

  const int total=chat.messages.size();
  int done=0;

  // Пилюля дня как в Telegram
  static const QStringList genitiveMonths={
    QStringLiteral("января"), QStringLiteral("февраля"),
    QStringLiteral("марта"), QStringLiteral("апреля"),
    QStringLiteral("мая"), QStringLiteral("июня"),
    QStringLiteral("июля"), QStringLiteral("августа"),
    QStringLiteral("сентября"), QStringLiteral("октября"),
    QStringLiteral("ноября"), QStringLiteral("декабря")
  };

  if(layout==TelegramLayout::Month || layout==TelegramLayout::Day ||
     layout==TelegramLayout::Year)
  {
    // Склеенные заметки: одна на месяц (ветки чат-год), на год
    // (прямо в чате) или на день (ветки чат-год-месяц).
    // Шапка, пилюли дней, пузырьки
    const bool byDay=(layout==TelegramLayout::Day);
    const bool byYear=(layout==TelegramLayout::Year);
    QMap<QString, QList<TelegramMessage>> groups;

    foreach(const TelegramMessage &message, chat.messages)
    {
      const QDate date=QDateTime::fromSecsSinceEpoch(message.unixtime).date();

      if(!date.isValid())
        continue;

      if(byDay)
        groups[date.toString(QStringLiteral("yyyy-MM-dd"))].append(message);
      else if(byYear)
        groups[QString::number(date.year())].append(message);
      else
        groups[QStringLiteral("%1-%2").arg(date.year()).arg(date.month(), 2, 10, QChar('0'))].append(message);
    }

    if(groups.isEmpty())
    {
      error="No dated messages";
      return false;
    }

    foreach(const QString &groupKey, groups.keys())
    {
      const int year=groupKey.left(4).toInt();

      TreeItem *parentBranch=chatBranch;

      if(!byYear)
        parentBranch=ensureBranch(model, parentBranch,
                                  QString::number(year), result);

      if(byDay)
      {
        // Заметки дней лежат прямо в папке месяца,
        // лишний уровень дня не плодится
        parentBranch=ensureBranch(model, parentBranch,
                                 groupKey.left(7), result);
      }

      QSet<QString> usedNames;
      QList<TelegramPendingFile> pending;
      QString html=QStringLiteral("<p><b>")+chat.name.toHtmlEscaped()+
                    QStringLiteral(" — ")+groupKey+
                    QStringLiteral("</b></p>");

      QDate lastDay;

      foreach(const TelegramMessage &message, groups.value(groupKey))
      {
        const QDate date=QDateTime::fromSecsSinceEpoch(message.unixtime).date();

        if(date!=lastDay)
        {
          lastDay=date;
          html+=QStringLiteral("<p align=\"center\"><span style=\"background-color:#8ab48a;\">")+
                QStringLiteral("<font color=\"#ffffff\">&nbsp;&nbsp;")+
                QString::number(lastDay.day())+
                QStringLiteral(" ")+
                genitiveMonths[lastDay.month()-1]+
                QStringLiteral("&nbsp;&nbsp;</font></span></p>");
        }

        html+=telegramMessageBubble(message, snippets, pending, usedNames);

        ++done;

        if(progress)
          progress(done, total);
      }

      // Заметка года зовется просто годом: она и так лежит
      // в папке чата, суффикс с именем чата тут лишний
      const QString noteName=byYear ? groupKey :
                             groupKey+QStringLiteral(" — ")+chat.name;

      if(!writeTelegramNote(parentBranch, noteName,
                            html, groups.value(groupKey).first().unixtime,
                            chat.name, pending, result, error))
        return false;
    }

    return true;
  }

  // Одна заметка на сообщение: ветки чат-год-месяц-день.
  // Имя: время-автор (сортировка), ctime и автор свои у каждой
  QMap<QString, QList<TelegramMessage>> days;

  foreach(const TelegramMessage &message, chat.messages)
  {
    const QDate date=QDateTime::fromSecsSinceEpoch(message.unixtime).date();

    if(!date.isValid())
      continue;

    days[date.toString(QStringLiteral("yyyy-MM-dd"))].append(message);
  }

  if(days.isEmpty())
  {
    error="No dated messages";
    return false;
  }

  foreach(const QString &dayKey, days.keys())
  {
    const int year=dayKey.left(4).toInt();
    TreeItem *yearBranch=ensureBranch(model, chatBranch,
                                      QString::number(year), result);
    TreeItem *monthBranch=ensureBranch(model, yearBranch,
                                       dayKey.left(7), result);
    TreeItem *dayBranch=ensureBranch(model, monthBranch,
                                     dayKey, result);

    QSet<QString> usedNames;

    foreach(const TelegramMessage &message, days.value(dayKey))
    {
      QList<TelegramPendingFile> pending;
      QSet<QString> fileNames;
      const QString html=telegramMessageBubble(message, snippets,
                                               pending, fileNames);
      const QDateTime dt=QDateTime::fromSecsSinceEpoch(message.unixtime);
      const QString author=message.from.isEmpty() ? chat.name : message.from;
      const QString noteName=takeUniqueName(usedNames,
          dt.toString(QStringLiteral("hh:mm"))+QStringLiteral(" — ")+author);

      if(noteName.isEmpty())
      {
        error="Can not name note";
        return false;
      }

      if(!writeTelegramNote(dayBranch, noteName, html, message.unixtime,
                            author, pending, result, error))
        return false;

      ++done;

      if(progress)
        progress(done, total);
    }
  }

  return true;
}
