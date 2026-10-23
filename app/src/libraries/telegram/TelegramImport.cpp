#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
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


// Лексическая проверка: путь остался внутри каталога.
// Для целей, которых еще нет на диске (canonicalFilePath пуст)
static bool isPathInside(const QString &root, const QString &path)
{
  const QString cleanRoot=QDir::cleanPath(root);
  const QString cleanPath=QDir::cleanPath(path);

  return cleanPath!=cleanRoot &&
         cleanPath.startsWith(cleanRoot+QLatin1Char('/'));
}


// Медиа из чужого экспорта: href относительный и обязан остаться
// внутри каталога экспорта. Иначе messages.html с href="../../.ssh/id_rsa"
// затащил бы произвольный локальный файл в заметку. Схемы (file:, http:)
// и абсолютные пути отклоняются, симлинк наружу ловится каноническим путем.
static QString safeExportMediaPath(const QDir &exportDir, const QString &href)
{
  const QString trimmed=href.trimmed();

  if(trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
    return QString();

  const QUrl url(trimmed);

  if(url.isValid() && !url.scheme().isEmpty())
    return QString();

  if(QDir::isAbsolutePath(trimmed))
    return QString();

  // В HTML экспорта пробелы и национальные символы обычно percent-encoded
  const QString decoded=QUrl::fromPercentEncoding(trimmed.toUtf8());

  if(decoded.isEmpty() || decoded.contains(QChar('\0')))
    return QString();

  const QString root=QDir::cleanPath(exportDir.absolutePath());
  const QString candidate=QDir::cleanPath(exportDir.absoluteFilePath(decoded));

  if(!isPathInside(root, candidate))
    return QString();

  const QFileInfo info(candidate);

  if(!info.isFile() || !info.isReadable())
    return QString();

  const QString canonicalRoot=QFileInfo(root).canonicalFilePath();
  const QString canonicalFile=info.canonicalFilePath();

  if(canonicalRoot.isEmpty() || canonicalFile.isEmpty() ||
     !isPathInside(canonicalRoot, canonicalFile))
    return QString();

  return canonicalFile;
}


// Имя файла внутри каталога заметки: только базовое имя, без
// разделителей и управляющих символов. Имя из <div class="title">
// или поля file_name недоверенное и иначе увело бы запись за пределы
// каталога записи (../../.bashrc)
static QString safeMediaFileName(const QString &source)
{
  QString name=source;
  name.replace(QLatin1Char('\\'), QLatin1Char('/'));
  name=QFileInfo(name).fileName();
  name.remove(QChar('\0'));
  name.remove(QRegularExpression(QStringLiteral("[\\x01-\\x1f\\x7f]")));

  if(name.isEmpty() || name==QStringLiteral(".") || name==QStringLiteral(".."))
    return QString();

  if(name.size()>180)
  {
    const int dotPos=name.lastIndexOf(QLatin1Char('.'));

    if(dotPos>0 && name.size()-dotPos<=16)
      name=name.left(180-(name.size()-dotPos))+name.mid(dotPos);
    else
      name=name.left(180);
  }

  return name;
}


// Ссылка в тексте заметки: только явно безопасные схемы.
// javascript:, data:, file: из чужого экспорта не должны стать
// кликабельными действиями в редакторе
static bool isSafeTelegramHref(const QString &href)
{
  const QString lower=href.trimmed().toLower();

  if(lower.startsWith(QLatin1Char('#')))
    return true;

  return lower.startsWith(QStringLiteral("http://")) ||
         lower.startsWith(QStringLiteral("https://")) ||
         lower.startsWith(QStringLiteral("mailto:")) ||
         lower.startsWith(QStringLiteral("tel:")) ||
         lower.startsWith(QStringLiteral("tg://"));
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

    if(!href.isEmpty() && isSafeTelegramHref(href))
      return QStringLiteral("<a href=\"")+href.toHtmlEscaped()+
             QStringLiteral("\">")+text+QStringLiteral("</a>");

    return text;
  }

  if(type==QStringLiteral("link") || type==QStringLiteral("email") ||
     type==QStringLiteral("phone") || type==QStringLiteral("mention") ||
     type==QStringLiteral("hashtag") || type==QStringLiteral("bot_command"))
  {
    QString href=text;

    if(type==QStringLiteral("email") && !href.startsWith(QStringLiteral("mailto:")))
      href=QStringLiteral("mailto:")+href;
    else if(type==QStringLiteral("phone") && !href.startsWith(QStringLiteral("tel:")))
      href=QStringLiteral("tel:")+href;

    if(isSafeTelegramHref(href))
      return QStringLiteral("<a href=\"")+href.toHtmlEscaped()+
             QStringLiteral("\">")+text+QStringLiteral("</a>");

    return text;
  }

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
  QList<bool> linkStack;
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
        const bool safeLink=linkStack.isEmpty() ? false : linkStack.takeLast();
        result+=safeLink ? QStringLiteral("</a>") : QStringLiteral("</span>");
        continue;
      }

      static QRegularExpression hrefRe("href\\s*=\\s*\"([^\"]*)\"");
      const QRegularExpressionMatch hrefMatch=hrefRe.match(match.captured(3));
      const bool safeLink=hrefMatch.hasMatch() &&
                          isSafeTelegramHref(hrefMatch.captured(1));

      if(safeLink)
        result+=QStringLiteral("<a href=\"")+
                hrefMatch.captured(1).toHtmlEscaped()+
                QStringLiteral("\">");
      else
        result+=QStringLiteral("<span>");

      linkStack.append(safeLink);
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


// Размер для карточки файла в стиле Telegram: "21.4 KB"
static QString formatTelegramSize(qint64 bytes)
{
  if(bytes<0)
    return QString();

  if(bytes<1024)
    return QString::number(bytes)+QStringLiteral(" B");

  const double kb=bytes/1024.0;

  if(kb<1024.0)
    return QString::number(kb, 'f', 1)+QStringLiteral(" KB");

  return QString::number(kb/1024.0, 'f', 1)+QStringLiteral(" MB");
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
      media.srcPath=safeExportMediaPath(exportDir, href);

      // Битая/внешняя ссылка: пропускаем медиа, а не тащим
      // произвольный локальный путь в заметку
      if(media.srcPath.isEmpty())
        continue;

      media.fileName=safeMediaFileName(
        QFileInfo(QUrl::fromPercentEncoding(href.toUtf8())).fileName());

      if(media.fileName.isEmpty())
        continue;

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
        {
          const QString titleName=safeMediaFileName(
            decodeTelegramEntities(titleMatch.captured(1).trimmed()));

          if(!titleName.isEmpty())
            media.fileName=titleName;
        }

        static QRegularExpression statusRe("<div class=\"status details\">\\s*(.*?)\\s*</div>",
                                           QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch statusMatch=statusRe.match(anchorInner);

        if(statusMatch.hasMatch() && media.kind==QStringLiteral("file"))
          media.sizeText=decodeTelegramEntities(statusMatch.captured(1).trimmed());

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
      const QString src=safeExportMediaPath(
        exportDir, msg.value(QStringLiteral("photo")).toString());

      if(!src.isEmpty())
      {
        TelegramMedia media;
        media.kind=QStringLiteral("photo");
        media.srcPath=src;
        media.fileName=safeMediaFileName(QFileInfo(src).fileName());

        if(!media.fileName.isEmpty())
          message.media.append(media);
      }
    }

    if(msg.contains(QStringLiteral("file")))
    {
      const QString src=safeExportMediaPath(
        exportDir, msg.value(QStringLiteral("file")).toString());

      if(!src.isEmpty())
      {
        TelegramMedia media;
        media.kind=(msg.value(QStringLiteral("media_type")).toString()==QStringLiteral("voice_message"))
                   ? QStringLiteral("voice") : QStringLiteral("file");
        media.srcPath=src;
        media.fileName=safeMediaFileName(msg.value(QStringLiteral("file_name")).toString());

        if(media.fileName.isEmpty())
          media.fileName=safeMediaFileName(QFileInfo(src).fileName());

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

        if(media.kind==QStringLiteral("file") && msg.contains(QStringLiteral("file_size")))
          media.sizeText=formatTelegramSize(msg.value(QStringLiteral("file_size")).toVariant().toLongLong());

        if(!media.fileName.isEmpty())
          message.media.append(media);
      }
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
  QString attachId; // id аттача для kind=attach (inner-имя от него)
};


// Уникальное имя в пределах набора. Каталог записи свежак,
// занятость только внутри заметки (месяца или дня)
static QString takeUniqueName(QSet<QString> &used, const QString &wanted)
{
  QString target=safeMediaFileName(wanted);

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
                                     QSet<QString> &usedNames,
                                     AttachTableData *attachTable,
                                     TelegramImportResult &result)
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

      if(disk.isEmpty() || !QFileInfo(media.srcPath).isFile())
      {
        ++result.skippedMedia;
        continue;
      }

      // Inner-имя детерминировано от id и видимого имени:
      // пробный Attach вычисляет его заранее для ссылки.
      // id запоминается: вставка использует тот же, иначе
      // ссылка и файл разъедутся
      Attach probeAttach(QStringLiteral("file"), attachTable);
      probeAttach.setField(QStringLiteral("id"), getUniqueId());
      probeAttach.setField(QStringLiteral("fileName"), disk);

      // Карточка файла как в Telegram: скрепка, имя-ссылка, размер.
      // Открывается внешней программой через относительную ссылку
      // (резолвится относительно каталога заметки)
      line+=QStringLiteral("<br/><div style=\"background-color:#f1f1f1;\">Файл: <a href=\"")+
            probeAttach.getInnerFileName().toHtmlEscaped()+
            QStringLiteral("\">")+disk.toHtmlEscaped()+
            QStringLiteral("</a>");

      if(!media.sizeText.isEmpty())
        line+=QStringLiteral(" ")+media.sizeText.toHtmlEscaped();

      line+=QStringLiteral("</div>");

      TelegramPendingFile pf;
      pf.kind=QStringLiteral("attach");
      pf.mediaKind=media.kind;
      pf.srcPath=media.srcPath;
      pf.fileName=disk;
      pf.attachId=probeAttach.getField(QStringLiteral("id"));
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
    // Имя уже очищено при разборе и в takeUniqueName; повторная
    // проверка перед записью - защита от будущих правок выше по коду
    const QString recordRoot=QDir(recordDir).absolutePath();
    const QString targetPath=QDir(recordRoot).absoluteFilePath(pf.fileName);

    if(!isPathInside(recordRoot, targetPath))
    {
      ++result.skippedMedia;
      continue;
    }

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

      if(img.save(targetPath, 0, 88))
        ++result.attaches;
      else
        ++result.skippedMedia;
    }
    else if(pf.kind==QStringLiteral("inline"))
    {
      // Копия байт в байт: оригинал не трогается, качество 1:1.
      // Размер показа ограничен width/height в <img>
      if(QFile::copy(pf.srcPath, targetPath))
        ++result.attaches;
      else
        ++result.skippedMedia;
    }
    else if(attachTable!=nullptr)
    {
      Attach attach(QStringLiteral("file"), attachTable);
      attach.setField(QStringLiteral("id"), pf.attachId);
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


TelegramLayout telegramLayoutForMode(const QString &mode)
{
  if(mode==QStringLiteral("message"))
    return TelegramLayout::Message;

  if(mode==QStringLiteral("day"))
    return TelegramLayout::Day;

  if(mode==QStringLiteral("year"))
    return TelegramLayout::Year;

  return TelegramLayout::Month;
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

        // Таблицы еще нет (запись не вставлена): пробному аттачу
        // она не нужна, inner-имя считается только из id и имени
        html+=telegramMessageBubble(message, snippets, pending, usedNames,
                                     nullptr, result);

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
                                               pending, fileNames,
                                               nullptr, result);
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
