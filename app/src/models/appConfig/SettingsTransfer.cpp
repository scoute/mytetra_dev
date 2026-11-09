#include <QObject>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QTextCodec>

#include "SettingsTransfer.h"


// Ключи conf.ini вне переноса: версия схемы (миграции приемника
// отрабатывают сами), абсолютные пути базы и корзины (на другой
// машине их может не быть), секреты шифрования
QStringList SettingsTransfer::confSkipKeys(void)
{
  return QStringList()
    << QStringLiteral("version")
    << QStringLiteral("tetradir")
    << QStringLiteral("trashdir")
    << QStringLiteral("passwordMiddleHash")
    << QStringLiteral("passwordSaveFlag");
}


// Ключи editorconf.ini вне переноса: только версия схемы
QStringList SettingsTransfer::editorSkipKeys(void)
{
  return QStringList()
    << QStringLiteral("version");
}


// Копирование группы (пустая — весь корень) с пропуском ключей.
// Вложенные группы переносятся как есть через полные имена
void SettingsTransfer::copyGroup(QSettings *src,
                                 const QString &srcGroup,
                                 QSettings *dst,
                                 const QString &dstGroup,
                                 const QStringList &skip)
{
  if(src==nullptr || dst==nullptr)
    return;

  if(!srcGroup.isEmpty())
    src->beginGroup(srcGroup);

  if(!dstGroup.isEmpty())
    dst->beginGroup(dstGroup);

  foreach(const QString &key, src->allKeys())
  {
    if(skip.contains(key))
      continue;

    dst->setValue(key, src->value(key));
  }

  if(!dstGroup.isEmpty())
    dst->endGroup();

  if(!srcGroup.isEmpty())
    src->endGroup();
}


// Текущий штамп для имен копий и файлов: yyyyMMdd-hhmmss
QString SettingsTransfer::currentStamp(void)
{
  return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
}


// Резервная копия файла рядом с ним: путь.bak-<штамп>
bool SettingsTransfer::backupFile(const QString &path, QString *error)
{
  if(!QFileInfo(path).isFile())
    return true;

  const QString backupPath=path+QStringLiteral(".bak-")+currentStamp();

  if(!QFile::copy(path, backupPath))
  {
    if(error!=nullptr)
      *error=tr("Can not create backup copy of %1").arg(path);

    return false;
  }

  return true;
}


// Свой файл текстуры бумаги (страница Theme, "own file") в выгрузку.
// Встроенные тайлы (путь ":/...") едут с программой и не нужны,
// пустой ключ означает выбор из выпадашки — тоже нечего тащить
void SettingsTransfer::exportCustomTheme(QSettings *conf, QSettings *out)
{
  if(conf==nullptr || out==nullptr)
    return;

  const QString srcPath=conf->value(QStringLiteral("themeCustomNotePaperImage")).toString();

  if(srcPath.isEmpty() || srcPath.startsWith(QStringLiteral(":")))
    return;

  QFile src(srcPath);

  if(!src.open(QIODevice::ReadOnly))
    return;

  const QByteArray data=src.readAll();
  src.close();

  if(data.isEmpty())
    return;

  out->beginGroup(QStringLiteral("CustomTheme"));
  out->setValue(QStringLiteral("paperFileName"), QFileInfo(srcPath).fileName());
  out->setValue(QStringLiteral("paperData"), QString::fromLatin1(data.toBase64()));
  out->endGroup();
}


// Байты текстуры из выгрузки в custom-theme приемника с ремапом ключа.
// Каталог берется из места живого conf.ini приемника
bool SettingsTransfer::importCustomTheme(QSettings *conf,
                                         QSettings *in,
                                         QString *error)
{
  if(conf==nullptr || in==nullptr)
  {
    if(error!=nullptr)
      *error=tr("Settings storage is unavailable");

    return false;
  }

  in->beginGroup(QStringLiteral("CustomTheme"));
  const QString fileName=QFileInfo(in->value(QStringLiteral("paperFileName")).toString()).fileName();
  const QByteArray data=QByteArray::fromBase64(in->value(QStringLiteral("paperData")).toString().toLatin1());
  in->endGroup();

  // Секции нет (старая выгрузка) или пустая — свой файл приемника
  // не трогается, ключ остается как был
  if(fileName.isEmpty() || data.isEmpty())
    return true;

  const QString workDir=QFileInfo(conf->fileName()).absolutePath();
  QDir dir(workDir);

  if(!dir.mkpath(QStringLiteral("custom-theme")))
  {
    if(error!=nullptr)
      *error=tr("Can not create custom-theme directory");

    return false;
  }

  const QString target=dir.absoluteFilePath(QStringLiteral("custom-theme/")+fileName);
  QFile targetFile(target);

  if(!targetFile.open(QIODevice::WriteOnly))
  {
    if(error!=nullptr)
      *error=tr("Can not write file %1").arg(target);

    return false;
  }

  if(targetFile.write(data)!=data.size())
  {
    if(error!=nullptr)
      *error=tr("Can not write file %1").arg(target);

    return false;
  }

  targetFile.close();

  conf->setValue(QStringLiteral("themeCustomNotePaperImage"), target);

  return true;
}


// Выгрузка в один INI-файл: [Meta], [Conf], [Editor],
// [Shortcuts/<раздел>]
bool SettingsTransfer::exportSettings(QSettings *conf,
                                      QSettings *editorConf,
                                      const QString &shortcutPath,
                                      const QString &outPath,
                                      QString *error)
{
  if(conf==nullptr || editorConf==nullptr)
  {
    if(error!=nullptr)
      *error=tr("Settings storage is unavailable");

    return false;
  }

  QSettings out(outPath, QSettings::IniFormat);
  out.setIniCodec(QTextCodec::codecForName("UTF-8"));

  out.beginGroup(QStringLiteral("Meta"));
  out.setValue(QStringLiteral("format"), QStringLiteral("1"));
  out.setValue(QStringLiteral("exportDate"),
               QDateTime::currentDateTime().toString(Qt::ISODate));
  out.endGroup();

  copyGroup(conf, QString(), &out, QStringLiteral("Conf"), confSkipKeys());
  copyGroup(editorConf, QString(), &out, QStringLiteral("Editor"), editorSkipKeys());

  // Свой файл текстуры бумаги: ключ хранит абсолютный путь,
  // без байтов на другой машине ему цена ноль
  exportCustomTheme(conf, &out);

  // Секции хоткеев как есть: initKeyTable приемника выкинет
  // устаревшие действия сам
  QSettings shortcuts(shortcutPath, QSettings::IniFormat);

  if(QFileInfo(shortcutPath).isFile())
  {
    foreach(const QString &group, shortcuts.childGroups())
      copyGroup(&shortcuts, group,
                &out, QStringLiteral("Shortcuts/")+group,
                QStringList());
  }

  out.sync();

  if(out.status()!=QSettings::NoError)
  {
    if(error!=nullptr)
      *error=tr("Can not write file %1").arg(outPath);

    return false;
  }

  return true;
}


// Загрузка: проверка [Meta], резервные копии, применение.
// Версии схем и пути приемника не трогаются
bool SettingsTransfer::importSettings(QSettings *conf,
                                      QSettings *editorConf,
                                      const QString &shortcutPath,
                                      const QString &inPath,
                                      QString *error)
{
  if(conf==nullptr || editorConf==nullptr)
  {
    if(error!=nullptr)
      *error=tr("Settings storage is unavailable");

    return false;
  }

  if(!QFileInfo(inPath).isFile())
  {
    if(error!=nullptr)
      *error=tr("File not found: %1").arg(inPath);

    return false;
  }

  QSettings in(inPath, QSettings::IniFormat);
  in.setIniCodec(QTextCodec::codecForName("UTF-8"));

  // Проверка формата до любых изменений и копий
  if(in.value(QStringLiteral("Meta/format")).toString()!=QStringLiteral("1"))
  {
    if(error!=nullptr)
      *error=tr("Unsupported settings file format");

    return false;
  }

  // Резервные копии текущих файлов
  if(!backupFile(conf->fileName(), error))
    return false;

  if(!backupFile(editorConf->fileName(), error))
    return false;

  if(!backupFile(shortcutPath, error))
    return false;

  // Применение: только известные префиксы, пропуски как при выгрузке.
  // Лишние ключи чужой версии ложатся как есть и никому не мешают,
  // недостающие подтянутся умолчаниями через обычные геттеры
  in.beginGroup(QStringLiteral("Conf"));

  foreach(const QString &key, in.allKeys())
  {
    if(confSkipKeys().contains(key))
      continue;

    conf->setValue(key, in.value(key));
  }

  in.endGroup();

  in.beginGroup(QStringLiteral("Editor"));

  foreach(const QString &key, in.allKeys())
  {
    if(editorSkipKeys().contains(key))
      continue;

    editorConf->setValue(key, in.value(key));
  }

  in.endGroup();

  // Свой файл текстуры: байты в custom-theme приемника, ключ
  // переписывается на новый путь (старый абсолютный уже перезаписан
  // выше циклом Conf и без ремапа указывал бы в пустоту)
  if(!importCustomTheme(conf, &in, error))
    return false;

  QSettings shortcuts(shortcutPath, QSettings::IniFormat);
  shortcuts.setIniCodec(QTextCodec::codecForName("UTF-8"));

  in.beginGroup(QStringLiteral("Shortcuts"));

  foreach(const QString &group, in.childGroups())
  {
    in.beginGroup(group);

    foreach(const QString &key, in.allKeys())
      shortcuts.setValue(group+QStringLiteral("/")+key, in.value(key));

    in.endGroup();
  }

  in.endGroup();

  conf->sync();
  editorConf->sync();
  shortcuts.sync();

  if(conf->status()!=QSettings::NoError ||
     editorConf->status()!=QSettings::NoError ||
     shortcuts.status()!=QSettings::NoError)
  {
    if(error!=nullptr)
      *error=tr("Can not write settings files");

    return false;
  }

  return true;
}
