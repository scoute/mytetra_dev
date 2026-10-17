#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QMap>
#include <QModelIndex>
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

  return html;
}


static QString telegramPlainText(const QJsonValue &textValue)
{
  if(textValue.isString())
    return textValue.toString();

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

  return plain;
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
  chat.name=root.value(QStringLiteral("name")).toString().trimmed();

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
    message.from=msg.value(QStringLiteral("from")).toString();

    if(msg.contains(QStringLiteral("forwarded_from")))
    {
      message.forwarded=true;
      message.fwdFrom=msg.value(QStringLiteral("forwarded_from")).toString();
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

  foreach(const QString &monthKey, months.keys())
  {
    const int year=monthKey.left(4).toInt();
    TreeItem *yearBranch=ensureBranch(model, chatBranch,
                                      QString::number(year), result);
    TreeItem *monthBranch=ensureBranch(model, yearBranch, monthKey, result);
    RecordTableData *table=monthBranch->recordtableGetTableData();

    if(table==nullptr)
    {
      error="Branch has no record table: "+monthKey;
      return false;
    }

    // Одна заметка на месяц: шапка + абзацы сообщений
    QString html=QStringLiteral("<p><b>")+chat.name.toHtmlEscaped()+
                QStringLiteral(" — ")+monthKey+
                QStringLiteral("</b></p>");

    // Файлы месяца: картинки и голосовые инлайном в каталог заметки
    // (как нативные), остальные документы аттачами
    struct PendingFile
    {
      QString kind; // "inline" или "attach"
      QString srcPath;
      QString fileName;
    };
    QList<PendingFile> pending;

    foreach(const TelegramMessage &message, months.value(monthKey))
    {
      const QDateTime dt=QDateTime::fromSecsSinceEpoch(message.unixtime);
      QString line=QStringLiteral("<p><b>")+
                   dt.toString(QStringLiteral("dd.MM.yyyy hh:mm"))+
                   QStringLiteral(" ")+message.from.toHtmlEscaped()+
                   QStringLiteral("</b>");

      if(message.forwarded && !message.fwdFrom.isEmpty())
        line+=QStringLiteral(" [переслано от ")+message.fwdFrom.toHtmlEscaped()+
              QStringLiteral("]");

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
        // Имя в каталоге заметки: исходное, при клаше с суффиксом -2
        QString targetName=media.fileName;

        if(media.kind==QStringLiteral("photo"))
        {
          line+=QStringLiteral("<br/><img src=\"")+targetName.toHtmlEscaped()+
                QStringLiteral("\"/>");

          PendingFile pf;
          pf.kind=QStringLiteral("inline");
          pf.srcPath=media.srcPath;
          pf.fileName=targetName;
          pending.append(pf);
        }
        else if(media.kind==QStringLiteral("voice"))
        {
          // Ссылка voicenote:// при открытии материализуется
          // в штатный плеер-пузырь
          line+=QStringLiteral("<br/><a href=\"voicenote://")+targetName.toHtmlEscaped()+
                QStringLiteral("\">[voice ")+media.extra+
                QStringLiteral("]</a>");

          PendingFile pf;
          pf.kind=QStringLiteral("inline");
          pf.srcPath=media.srcPath;
          pf.fileName=targetName;
          pending.append(pf);
        }
        else
        {
          line+=QStringLiteral("<br/>[Файл: ")+media.fileName.toHtmlEscaped()+
                QStringLiteral("]");

          PendingFile pf;
          pf.kind=QStringLiteral("attach");
          pf.srcPath=media.srcPath;
          pf.fileName=media.fileName;
          pending.append(pf);
        }
      }

      line+=QStringLiteral("</p>");
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
    record.setField(QStringLiteral("name"), monthKey);
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
      const QString recordDir=mytetraConfig.get_tetradir()+
                             QStringLiteral("/base/")+
                             stored->getField(QStringLiteral("dir"));
      AttachTableData *attachTable=stored->getAttachTablePointer();

      foreach(const PendingFile &pf, pending)
      {
        if(pf.kind==QStringLiteral("inline"))
        {
          QString target=pf.fileName;

          if(QFileInfo(QDir(recordDir).absoluteFilePath(target)).isFile())
          {
            const int dotPos=target.lastIndexOf(QLatin1Char('.'));
            const QString stem=(dotPos>0) ? target.left(dotPos) : target;
            const QString ext=(dotPos>0) ? target.mid(dotPos) : QString();
            bool placed=false;

            for(int attempt=2; attempt<1000; ++attempt)
            {
              const QString candidate=stem+QStringLiteral("-")+
                                      QString::number(attempt)+ext;

              if(!QFileInfo(QDir(recordDir).absoluteFilePath(candidate)).isFile())
              {
                target=candidate;
                placed=true;
                break;
              }
            }

            if(!placed)
            {
              ++result.skippedMedia;
              continue;
            }
          }

          if(QFile::copy(pf.srcPath, QDir(recordDir).absoluteFilePath(target)))
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
      }
    }

    ++result.notes;
  }

  return true;
}
