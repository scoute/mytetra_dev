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


// Длинная сторона картинки в заметке: больше 600 не держим.
// Редактор ничего не растягивает за окно, база не пухнет
static const int telegramMaxSide=640;

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


bool importTelegramChat(KnowTreeModel *model,
                        const TelegramChat &chat,
                        TelegramImportResult &result,
                        QString &error,
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
  // Молчаливое слияние дало бы дубли месяцев
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

  // Группировка по месяцам, порядок хронологический
  QMap<QString, QList<TelegramMessage>> months;

  foreach(const TelegramMessage &message, chat.messages)
  {
    const QDate date=QDateTime::fromSecsSinceEpoch(message.unixtime).date();

    if(!date.isValid())
      continue;

    months[QStringLiteral("%1-%2").arg(date.year()).arg(date.month(), 2, 10, QChar('0'))].append(message);
  }

  if(months.isEmpty())
  {
    error="No dated messages";
    return false;
  }

  if(top==nullptr)
    top=ensureBranch(model, root, QStringLiteral("Telegram"), result);

  TreeItem *chatBranch=ensureBranch(model, top, chat.name, result);

  const int total=chat.messages.size();
  int done=0;

  // Каталог месячной заметки известен заранее: имена файлов
  // резолвятся до построения HTML, ссылка и файл не разъедутся
  const QString baseDir=mytetraConfig.get_tetradir()+QStringLiteral("/base/");

  foreach(const QString &monthKey, months.keys())
  {
    QSet<QString> usedNames;

    auto takeName=[&](const QString &wanted) -> QString {
      QString target=wanted;

      if(target.isEmpty())
        target=QStringLiteral("file");

      const int dotPos=target.lastIndexOf(QLatin1Char('.'));
      const QString stem=(dotPos>0) ? target.left(dotPos) : target;
      const QString ext=(dotPos>0) ? target.mid(dotPos) : QString();
      int attempt=2;

      // Каталог записи свежак: занятость только внутри месяца
      while(usedNames.contains(target))
      {
        target=stem+QStringLiteral("-")+QString::number(attempt)+ext;
        ++attempt;

        if(attempt>1000)
          return QString();
      }

      usedNames.insert(target);

      return target;
    };

    const int year=monthKey.left(4).toInt();

    // Заметка месяца лежит прямо в ветке года: папка-месяц
    // ради одной заметки только плодит уровень
    TreeItem *yearBranch=ensureBranch(model, chatBranch,
                                      QString::number(year), result);
    RecordTableData *table=yearBranch->recordtableGetTableData();

    if(table==nullptr)
    {
      error="Branch has no record table: "+monthKey;
      return false;
    }

    // Одна заметка на месяц: шапка + пузырьки сообщений.
    // Пузырьки таблицами: фон ячейки и отступы QTextDocument умеет,
    // скругление углов нет — повторяется суть дизайна Telegram
    QString html=QStringLiteral("<p><b>")+chat.name.toHtmlEscaped()+
                QStringLiteral(" — ")+monthKey+
                QStringLiteral("</b></p>");

    // Пилюля дня как в Telegram
    static const QStringList genitiveMonths={
      QStringLiteral("января"), QStringLiteral("февраля"),
      QStringLiteral("марта"), QStringLiteral("апреля"),
      QStringLiteral("мая"), QStringLiteral("июня"),
      QStringLiteral("июля"), QStringLiteral("августа"),
      QStringLiteral("сентября"), QStringLiteral("октября"),
      QStringLiteral("ноября"), QStringLiteral("декабря")
    };

    QDate lastDay;

    // Файлы месяца: картинки и голосовые инлайном в каталог заметки
    // (как нативные), остальные документы аттачами
    struct PendingFile
    {
      QString kind; // "inline", "preview" или "attach"
      QString mediaKind; // "photo", "voice", "file"
      QString srcPath;
      QString fileName; // имя файла в каталоге заметки
      QString origName; // для превью: имя оригинала
    };
    QList<PendingFile> pending;

    foreach(const TelegramMessage &message, months.value(monthKey))
    {
      const QDateTime dt=QDateTime::fromSecsSinceEpoch(message.unixtime);
      QString line;

      // Пилюля дня при смене даты, как в Telegram
      if(dt.date()!=lastDay)
      {
        lastDay=dt.date();
        line+=QStringLiteral("<p align=\"center\"><span style=\"background-color:#8ab48a;\">")+
              QStringLiteral("<font color=\"#ffffff\">&nbsp;&nbsp;")+
              QString::number(lastDay.day())+
              QStringLiteral(" ")+
              genitiveMonths[lastDay.month()-1]+
              QStringLiteral("&nbsp;&nbsp;</font></span></p>");
      }

      // Пузырек сообщения: фон и отступы нативно держит QTextDocument
      line+=QStringLiteral("<div style=\"background-color:#eeffde;margin-top:6px;margin-bottom:6px;\">")+
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
          // Габариты заранее по исходнику: превью им соответствует,
          // редактор ничего не растягивает. Большая картинка: показ
          // идет с гладкого превью, оригинал лежит рядом аттачем
          const QSize naturalSize=QImageReader(media.srcPath).size();
          const bool needPreview=naturalSize.isValid() &&
                                 (naturalSize.width()>telegramMaxSide ||
                                  naturalSize.height()>telegramMaxSide);
          const QSize displaySize=needPreview ?
                                  telegramDisplaySize(media.srcPath) :
                                  naturalSize;

          if(needPreview)
          {
            const QString origDisk=takeName(media.fileName);

            if(origDisk.isEmpty())
            {
              ++result.skippedMedia;
              continue;
            }

            const int dotPos=origDisk.lastIndexOf(QLatin1Char('.'));
            const QString previewDisk=takeName(origDisk.left(dotPos)+
                                               QStringLiteral("_preview")+
                                               origDisk.mid(dotPos));

            if(previewDisk.isEmpty())
            {
              ++result.skippedMedia;
              continue;
            }

            if(displaySize.isValid())
              line+=QStringLiteral("<br/><img src=\"")+previewDisk.toHtmlEscaped()+
                    QStringLiteral("\" width=\"")+QString::number(displaySize.width())+
                    QStringLiteral("\" height=\"")+QString::number(displaySize.height())+
                    QStringLiteral("\"/>");
            else
              line+=QStringLiteral("<br/><img src=\"")+previewDisk.toHtmlEscaped()+
                    QStringLiteral("\"/>");

            PendingFile pf;
            pf.kind=QStringLiteral("preview");
            pf.mediaKind=media.kind;
            pf.srcPath=media.srcPath;
            pf.fileName=previewDisk;
            pf.origName=origDisk;
            pending.append(pf);

            PendingFile pa;
            pa.kind=QStringLiteral("attach");
            pa.mediaKind=media.kind;
            pa.srcPath=media.srcPath;
            pa.fileName=origDisk;
            pending.append(pa);
          }
          else
          {
            const QString disk=takeName(media.fileName);

            if(disk.isEmpty())
            {
              ++result.skippedMedia;
              continue;
            }

            if(displaySize.isValid())
              line+=QStringLiteral("<br/><img src=\"")+disk.toHtmlEscaped()+
                    QStringLiteral("\" width=\"")+QString::number(displaySize.width())+
                    QStringLiteral("\" height=\"")+QString::number(displaySize.height())+
                    QStringLiteral("\"/>");
            else
              line+=QStringLiteral("<br/><img src=\"")+disk.toHtmlEscaped()+
                    QStringLiteral("\"/>");

            PendingFile pf;
            pf.kind=QStringLiteral("inline");
            pf.mediaKind=media.kind;
            pf.srcPath=media.srcPath;
            pf.fileName=disk;
            pending.append(pf);
          }
        }
        else if(media.kind==QStringLiteral("voice"))
        {
          // Ссылка voicenote:// при открытии материализуется
          // в штатный плеер-пузырь
          const QString disk=takeName(media.fileName);

          if(disk.isEmpty())
          {
            ++result.skippedMedia;
            continue;
          }

          line+=QStringLiteral("<br/><a href=\"voicenote://")+disk.toHtmlEscaped()+
                QStringLiteral("\">[voice ")+media.extra+
                QStringLiteral("]</a>");

          PendingFile pf;
          pf.kind=QStringLiteral("inline");
          pf.mediaKind=media.kind;
          pf.srcPath=media.srcPath;
          pf.fileName=disk;
          pending.append(pf);
        }
        else
        {
          line+=QStringLiteral("<br/>[Файл: ")+media.fileName.toHtmlEscaped()+
                QStringLiteral("]");

          PendingFile pf;
          pf.kind=QStringLiteral("attach");
          pf.mediaKind=media.kind;
          pf.srcPath=media.srcPath;
          pf.fileName=media.fileName;
          pending.append(pf);
        }
      }
      line+=QStringLiteral("</div>");
      html+=line;

      ++done;

      if(progress)
        progress(done, total);
    }

    // Тот же набор полей что McpTools::applyCreateNote при ручном создании
    Record record;
    record.switchToFat();
    record.setText(html);
    record.setField(QStringLiteral("id"), getUniqueId());
    record.setField(QStringLiteral("dir"), getUniqueId());
    record.setField(QStringLiteral("file"), QStringLiteral("text.html"));
    record.setField(QStringLiteral("name"), monthKey+QStringLiteral(" — ")+chat.name);
    record.setField(QStringLiteral("author"), chat.name);
    record.setField(QStringLiteral("url"), QString());
    record.setField(QStringLiteral("tags"), QStringLiteral("telegram"));

    if(!months.value(monthKey).isEmpty())
      record.setField(QStringLiteral("ctime"),
                      QDateTime::fromSecsSinceEpoch(months.value(monthKey).first().unixtime).toString(QStringLiteral("yyyyMMddhhmmss")));

    const int pos=table->insertNewRecord(GlobalParameters::AddNewRecordBehavior::ADD_TO_END,
                                        0, record);

    if(pos<0)
    {
      error="Can not insert note: "+monthKey;
      return false;
    }

    // Файлы месяца. Инлайн: копия под исходным именем в каталог
    // заметки (картинки резолвятся через DocumentUrl, голосовые
    // через voicenote://). Аттачи: штатным механизмом.
    // Нет файла на диске: метка в тексте уже есть, импорт продолжается
    Record *stored=table->getRecord(pos);

    if(stored!=nullptr)
    {
      // Каталог берется у вставленной записи: insert может
      // перегенерировать dir, предвычисленному верить нельзя
      const QString recordDir=baseDir+stored->getField(QStringLiteral("dir"));
      AttachTableData *attachTable=stored->getAttachTablePointer();

      foreach(const PendingFile &pf, pending)
      {
        // Имена уже зарезервированы через takeName: прямая запись
        if(pf.kind==QStringLiteral("preview"))
        {
          // Гладкое превью для показа: сглаживание при генерации,
          // а не при рисовании редактором (там в лоб и угловато)
          QImage img(pf.srcPath);


          if(img.isNull())
          {
            ++result.skippedMedia;
            continue;
          }

          img=img.scaled(telegramMaxSide, telegramMaxSide,
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);

          if(img.save(QDir(recordDir).absoluteFilePath(pf.fileName), 0, 88))
            ++result.previews;
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
    }

    ++result.notes;
  }

  return true;
}
