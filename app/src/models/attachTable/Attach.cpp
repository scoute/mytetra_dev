#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QMessageBox>
#include <QDebug>
#include <QXmlStreamWriter>

#include "Attach.h"
#include "AttachTableData.h"
#include "models/recordTable/Record.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/crypt/CryptService.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/DiskHelper.h"
#include "libraries/helpers/DebugHelper.h"
#include "libraries/helpers/MessageHelper.h"

extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;


// Конструктор прикрепляемого файла
Attach::Attach(AttachTableData *iParentTable)
{
  init(iParentTable);
}


// Конструктор прикрепляемого файла с указанием типа прикрепления
// в настоящий момент есть два типа прикрепления - просто файл "file" или линк на файл "link"
Attach::Attach(QString iType, AttachTableData *iParentTable)
{
  if( !typeAvailableList().contains(iType) )
    criticalError("Incorrect attach type in Attach constructor: "+iType);

  setField("type", iType);

  init(iParentTable);
}


Attach::~Attach()
{
  fileContent.clear();
}


void Attach::init(AttachTableData *iParentTable)
{
  liteFlag=true; // По-умолчанию легкий объект
  parentTable=iParentTable;
  fileContent.clear();
}


void Attach::setParentTable(AttachTableData *iParentTable)
{
  parentTable=iParentTable;
}


// Допустимые имена полей
QStringList Attach::fieldAvailableList(void)
{
  return QStringList() << "id" << "fileName" << "link" << "type" << "crypt"
                       << "description" << "source";
}


// Имена полей, которые шифруются. Описание и происхождение файла
// чувствительны так же как имя и ссылка, шифруются вместе с ними
QStringList Attach::fieldCryptedList(void)
{
  return QStringList() << "fileName" << "link" << "description" << "source";
}


// Допустимые типы аттачей
QStringList Attach::typeAvailableList(void)
{
  return QStringList() << "file" << "link";
}


// На вход метода подается тег <file>
void Attach::setupDataFromDom(QDomElement iDomElement)
{
  QStringList fieldsName=fieldAvailableList();
  foreach( QString fieldName, fieldsName ) // Перебираются имена полей (XML-тегов)
    fields[fieldName]=iDomElement.attribute(fieldName); // Напрямую устанавливаются значения из XML файла
}


QDomElement Attach::exportDataToDom(QDomDocument *doc) const
{
  QDomElement elem=doc->createElement("file");

  QStringList fieldsName=fieldAvailableList();
  foreach( QString fieldName, fieldsName ) // Перебираются имена полей (XML-тегов)
    if( fields[fieldName].size()>0 )
      elem.setAttribute(fieldName, fields[fieldName]);

  return elem;
}


void Attach::exportDataToStreamWriter(QXmlStreamWriter *xmlWriter) const
{
  xmlWriter->writeStartElement("file");

  QStringList fieldsName=fieldAvailableList();
  foreach( QString fieldName, fieldsName ) // Перебираются имена полей (XML-тегов)
    if( fields[fieldName].size()>0 )
      xmlWriter->writeAttribute(fieldName, fields[fieldName]);

  xmlWriter->writeEndElement(); // Закрылся file
}


bool Attach::isEmpty() const
{
  // Заполненный аттач не может содержать пустой id
  if(fields.contains("id")==false || getField("id").length()==0)
    return true;
  else
    return false;
}


bool Attach::isLite() const
{
  return liteFlag;
}


void Attach::switchToLite()
{
  // Переключение возможно только из полновесного состояния
  if(liteFlag==true)
    criticalError("Can't switch attach to lite state. Attach id: "+getField("id")+" File name: "+getField("fileName"));

  fileContent.clear();

  liteFlag=true;
}


void Attach::switchToFat()
{
  // Переключение возможно только из легкого состояния
  if(liteFlag!=true)
    criticalError("Unavailable switching attach object to fat state. Attach Id: "+getField("id")+" File name: "+getField("fileName"));

  liteFlag=false;
}


// Получение значения поля
// Метод возвращает расшифрованные данные, если запись была зашифрована
QString Attach::getField(QString name) const
{
  // Если имя поля недопустимо
  if(fieldAvailableList().contains(name)==false)
    criticalError("Attach::getField() : get unavailable field "+name);


  // ------------------------------------------
  // Проверки и действия для разных типов полей
  // ------------------------------------------

  // Если запрашивается линк на файл
  if(name=="link")
    if(fields["type"]!="link") // И тип аттача не является линком
      criticalError("Attach::getField() : Can't get link from non-link attach.");


  // -----------------------
  // Получение значения поля
  // -----------------------

  // Если запись зашифрована, но ключ не установлен (т.е. человек не вводил пароль)
  // то расшифровка невозможна
  if(fieldCryptedList().contains(name))
    if(fields.contains("crypt"))
      if(fields["crypt"]=="1")
        if(globalParameters.getCryptKey().length()==0)
          return QString();

  bool isCrypt=false;

  // Если имя поля принадлежит списку полей, которые могут шифроваться
  // и в наборе полей есть поле crypt
  // и поле crypt установлено в 1
  // и запрашиваемое поле не пустое (пустые данные невозможно расшифровать)
  if(fieldCryptedList().contains(name))
    if(fields.contains("crypt"))
      if(fields["crypt"]=="1")
        if(fields[name].length()>0)
          isCrypt=true;

  // Если поле не подлежит шифрованию
  if(isCrypt==false)
    return fields[name]; // Возвращается значение поля
  else
    return CryptService::decryptString(globalParameters.getCryptKey(), fields[name]); // Поле расшифровывается
}


// Установка значения поля
// Метод принимает незашифрованные данные, и шфирует их если запись является зашифрованой
void Attach::setField(QString name, QString value)
{
  // Если имя поля недопустимо
  if(fieldAvailableList().contains(name)==false)
    criticalError("Attach::setField() : set unavailable field "+name);


  // ------------------------------------------
  // Проверки и действия для разных типов полей
  // ------------------------------------------

  // Поле с типом аттача
  if(name=="type")
    if( !typeAvailableList().contains(value) )
      criticalError("Attach::setField() : Incorrect attach type : "+value);

  // Поле с именем файла
  if(name=="fileName")
    if(getField("type")=="link") // Если устанавливается имя файла для линка
      if(getField("fileName").length()>0 && value.length()>0) // Если имя уже было задано (при создании аттача), и новое имя не пустое
      {
        // Имя файла для линка менять нельзя
        showMessageBox(QObject::tr("Unable to rename a file which attached as a link."));
        return;
      }

  // Поле со ссылкой на файл (содержит путь к файлу, на который указывает линк)
  if(name=="link")
  {
    QFile tempFile(value);

    // Если файла, на который ссылается линк, не существует
    if(!tempFile.exists())
    {
      showMessageBox(QObject::tr("Bad link. File not found."));
      return;
    }
  }


  // -----------------------
  // Установка значения поля
  // -----------------------

  bool isCrypt=false;

  // Если имя поля принадлежит списку полей, которые могут шифроваться
  // и в наборе полей есть поле crypt
  // и поле crypt установлено в 1
  // и поле не пустое (пустые данные не нужно шифровать)
  if(fieldCryptedList().contains(name))
    if(fields.contains("crypt"))
      if(fields["crypt"]=="1")
        if(value.length()>0)
        {
          if(globalParameters.getCryptKey().length()>0)
            isCrypt=true;
          else
            criticalError("In Attach::setField() can not set data to crypt field "+name+". Password not setted");
        }

  // Если нужно шифровать, значение поля шифруется
  if(isCrypt==true)
    value=CryptService::encryptString(globalParameters.getCryptKey(), value);

  // Устанавливается значение поля
  fields.insert(name, value);
}


// Защищенный метод - Установка значения поля напрямую
// Используеся при шифрации-дешифрации данных аттача
// todo: подумать, может быть отказаться от этого метода
void Attach::setFieldSource(QString name, QString value)
{
  // Если имя поля недопустимо
  if(fieldAvailableList().contains(name)==false)
    criticalError("Attach::setField() : set unavailable field "+name);

  // Устанавливается значение поля
  fields.insert(name, value);
}


void Attach::pushFatDataToDisk()
{
  if(getField("type")!="file")
    criticalError("Can't push fat data for non-file attach.");

  if(liteFlag==true)
    criticalError("Can't push fat data for lite attach. Attach id: "+getField("id")+" File name: "+getField("fileName"));

  QString innerFileName=getInnerFileName();
  QString innerDirName=parentTable->record->getFullDirName();

  QMap<QString, QByteArray> fileList;
  fileList[innerFileName]=fileContent;
  DiskHelper::saveFilesToDirectory(innerDirName, fileList);
}


// То же самое что и pushFatDataToDisk, только в нужную директорию
void Attach::pushFatDataToDirectory(QString dirName)
{
  if(getField("type")!="file")
    criticalError("Can't save to directory "+dirName+" non-file attach.");

  if(liteFlag==true)
    criticalError("Can't save to directory lite attach. Attach id: "+getField("id")+" File name: "+getField("fileName"));

  QMap<QString, QByteArray> fileList;
  fileList[ getInnerFileName() ]=fileContent;
  DiskHelper::saveFilesToDirectory(dirName, fileList);
}


// Втаскивание в объект содержимого файла с диска
void Attach::popFatDataFromDisk()
{
  // Втаскивание возможно только в полновесном состоянии
  if(liteFlag==true)
    criticalError("Can't' pop data for lite attach. Attach id: "+getField("id")+" File name: "+getField("fileName"));

  fileContent.clear();

  // Читается реально лежащий на диске файл (учитывает обе схемы имен)
  QString innerFileName=getExistingInnerFileName();
  QString innerDirName=parentTable->record->getFullDirName();

  fileContent.append( (DiskHelper::getFilesFromDirectory(innerDirName, innerFileName)).value(innerFileName) );
}


// fullFileName - имя файла с полным путем, который нужно скопировать в каталог записи
bool Attach::copyFileToBase(QString iFileName)
{
  qDebug() << "Before check file. iFileName: "+iFileName;
  qDebug() << "Inner file name: "+getFullInnerFileName();

  // Проверка наличия исходного файла
  QFile file(iFileName);

  qDebug() << "Check file finish";

  if(file.exists()==false)
  {
    showMessageBox(QObject::tr("Can't open file %1. File not exists.").arg(iFileName));
    return false;
  }

  qDebug() << "Before real copy file.";

  bool result=file.copy( getFullInnerFileName() );

  qDebug() << "After real copy file.";

  if(result==false)
    showMessageBox(QObject::tr("Can't copy file %1. Maybe directory %2 not writable, or target file %3 already exists.").arg(iFileName).arg(getFullInnerDirName()).arg(getFullInnerFileName()));

  return result;
}


// Удаление файла с диска
void Attach::removeFile()
{
  if(getField("type")!="file")
    return;

  // Проверка наличия файла
  QFile file( getFullInnerDirName()+"/"+getExistingInnerFileName() );

  if(file.exists()==false)
  {
    showMessageBox(QObject::tr("Unable to delete the file %1 from disk: file not found.").arg( getFullInnerFileName() ));
    return;
  }

  // Выставляются права чтобы удаление прошло корректно
  file.setPermissions(file.permissions() | QFile::ReadUser | QFile::WriteUser);

  // Непосредственно удаление
  file.remove();
}


QString Attach::constructFileName(const QString type, const QString id, const QString fileName)
{
    // Схема выбирается настройкой режима имен
    if(type=="file")
    {
        int mode=mytetraConfig.get_attachNameMode();
        if(mode==AppConfig::AttachNameHybrid)
            return constructHumanFileName(type, id, fileName);
        if(mode==AppConfig::AttachNameOriginal)
            return constructOriginalFileName(type, id, fileName);
    }

    return constructLegacyFileName(type, id, fileName);
}


// Исходная схема: id.ext (расширение от видимого имени)
QString Attach::constructLegacyFileName(const QString type, const QString id, const QString fileName)
{
    // Для файла
    if(type=="file")
    {
        if(fileName.startsWith(".")) { // Если это dot-файл
            return "."+id;
        }
        else if(fileName.endsWith(".")) { // Если в имени файла точка в конце
            return id+".";
        }
        else {
            // Выясняется расширение по видимому имени файла
            QFileInfo fileInfo( fileName );
            QString suffix=fileInfo.suffix();

            if(suffix.length()>0) {
                return id+"."+suffix; // Если расширение есть
            } else {
                return id; // Без расширения
            }
        }
    }

    // Для линка просто возвращается имя файла, куда указывает линк
    if(type=="link")
        return fileName;

    criticalError("Bad attach type in getInnerFileName():"+type);

    return "";
}


// Общая чистка видимого имени для подстановки в имя файла на диске.
// Пробелы и табы становятся подчеркиваниями (удобно в консоли Linux,
// не нужно квотирование), разделители пути, NUL, управляющие символы
// и запрещенные в Windows символы (<>:"|?*) тоже становятся
// подчеркиваниями: каталог базы могут синхронизировать на Windows.
// Кириллица и регистр сохраняются. Повторы подчеркиваний схлопываются,
// краевые подчеркивания снимаются
QString Attach::sanitizeVisibleName(const QString name)
{
    static const QString forbiddenChars=QString::fromLatin1("<>:\"|?*");

    QString result;
    result.reserve(name.length());
    for(int i=0; i<name.length(); ++i)
    {
        QChar c=name.at(i);
        if(c==QChar('/') || c==QChar('\\') || c==QChar('\0')
           || c.category()==QChar::Other_Control
           || forbiddenChars.contains(c)
           || c.isSpace())
            result.append('_');
        else
            result.append(c);
    }

    // Схлопывание повторов подчеркиваний: "my  doc" -> "my_doc"
    while(result.contains("__"))
        result.replace("__", "_");

    // Краевые подчеркивания снимаются
    while(result.startsWith("_"))
        result=result.mid(1);
    while(result.endsWith("_"))
        result.chop(1);

    return result;
}


// Видимая часть имени, пригодная для подстановки в имя файла на диске:
// чистится общим хелпером, ведущие точки снимаются (чтобы не получались
// скрытые файлы и ".."), остаток режется до разумной длины с сохранением
// читаемости. Расширение не входит, оно пристыковывается отдельно
// как в исходной схеме
QString Attach::sanitizeHumanNamePart(const QString fileName)
{
    // Берется основа без расширения: "Договор.v2.pdf" -> "Договор.v2"
    QString stem=fileName;
    QFileInfo fileInfo(fileName);
    QString suffix=fileInfo.suffix();
    if(suffix.length()>0 && stem.endsWith("."+suffix, Qt::CaseSensitive))
        stem=stem.left(stem.length()-suffix.length()-1);

    QString result=sanitizeVisibleName(stem);

    // Ведущие точки снимаются: имя не должно становиться скрытым,
    // а ".." вообще недопустимо как часть пути
    while(result.startsWith("."))
        result=result.mid(1).trimmed();

    // Хвостовые точки Windows не любит, снимаются тоже
    while(result.endsWith("."))
        result.chop(1);

    // Длинные названия режутся: хватает начала для узнаваемости,
    // расширение при этом не страдает, оно хранится отдельно
    const int maxStemLength=80;
    if(result.length()>maxStemLength)
        result=result.left(maxStemLength).trimmed();

    return result;
}


// Человеко-читаемая схема: id_видимоеИмя.ext. Префикс id сохраняет
// уникальность (коллизии имен, регистрозависимость FS не страшны),
// видимая часть позволяет узнать файл, не открывая его
QString Attach::constructHumanFileName(const QString type, const QString id, const QString fileName)
{
    if(type=="file")
    {
        // Имя с точкой в конце не имеет пригодной основы: остается исходная схема
        if(fileName.endsWith("."))
            return constructLegacyFileName(type, id, fileName);

        // Ведущие точки снимаются до разбора: иначе QFileInfo принимает
        // имя dot-файла за расширение (".hidden" -> suffix "hidden").
        // Скрытость запоминается и восстанавливается префиксом ниже
        bool hidden=fileName.startsWith(".");
        QString work=fileName;
        while(work.startsWith("."))
            work=work.mid(1);

        QFileInfo fileInfo(work);
        QString suffix=fileInfo.suffix();

        QString stem=sanitizeHumanNamePart(work);

        // Если от видимого имени ничего пригодного не осталось,
        // возвращается исходная схема
        if(stem.isEmpty())
            return constructLegacyFileName(type, id, fileName);

        // Скрытость сохраняется: dot-файл остается скрытым,
        // но получает читаемую часть имени
        QString prefix=id+"_";
        if(hidden)
            prefix="."+prefix;

        if(suffix.length()>0)
            return prefix+stem+"."+suffix;
        else
            return prefix+stem;
    }

    if(type=="link")
        return fileName;

    criticalError("Bad attach type in getInnerFileName():"+type);

    return "";
}


// Оригинальная схема: имя как при загрузке, с вычищенными опасными
// символами (общий хелпер: пробелы в подчеркивания, без path traversal
// и Windows-запрещенных символов). Уникальность префиксом не защищена:
// два разных вложения с одинаковым видимым именем в одной записи
// конфликтуют, такое переименование пропускается с ошибкой,
// а не затирается
QString Attach::sanitizeOriginalName(const QString fileName)
{
    QString result=sanitizeVisibleName(fileName);

    // Точки в конце Windows не любит, снимаются.
    // Ведущие точки сохраняются: оригинал остается оригиналом,
    // dot-файл остается скрытым
    while(result.endsWith("."))
        result.chop(1);

    // Имя "." и ".." как часть пути недопустимы
    if(result=="." || result==".." || result.isEmpty())
        return QString();

    // Длинные имена режутся с сохранением расширения
    const int maxLength=150;
    if(result.length()>maxLength)
    {
        QFileInfo fileInfo(result);
        QString suffix=fileInfo.suffix();
        if(suffix.length()>0 && result.endsWith("."+suffix, Qt::CaseSensitive)
           && suffix.length()+1<maxLength)
            result=result.left(maxLength-suffix.length()-1).trimmed()+"."+suffix;
        else
            result=result.left(maxLength).trimmed();
    }

    if(result=="." || result==".." || result.isEmpty())
        return QString();

    return result;
}


// Оригинальная схема: вычищенное имя как при загрузке.
// Если вычищать нечего (пусто, точки), возвращается исходная схема
QString Attach::constructOriginalFileName(const QString type, const QString id, const QString fileName)
{
    if(type=="file")
    {
        QString clean=sanitizeOriginalName(fileName);
        if(!clean.isEmpty())
            return clean;

        return constructLegacyFileName(type, id, fileName);
    }

    if(type=="link")
        return fileName;

    criticalError("Bad attach type in getInnerFileName():"+type);

    return "";
}


// Все возможные имена файла на диске для данного вложения
// (все три схемы без дублей). Порядок: системное, гибридное, оригинальное.
// Используется поиском существующего файла и конвертацией между схемами
QStringList Attach::candidateFileNames(const QString type, const QString id, const QString fileName)
{
    QStringList candidates;

    QString legacy=constructLegacyFileName(type, id, fileName);
    QString human=constructHumanFileName(type, id, fileName);
    QString original=constructOriginalFileName(type, id, fileName);

    candidates << legacy;
    if(human!=legacy)
        candidates << human;
    if(original!=legacy && original!=human)
        candidates << original;

    return candidates;
}


// Имя файла по заданному режиму (для конвертации к выбранному виду)
QString Attach::fileNameForMode(int mode, const QString type, const QString id, const QString fileName)
{
    if(type!="file")
        return constructLegacyFileName(type, id, fileName);

    if(mode==AppConfig::AttachNameHybrid)
        return constructHumanFileName(type, id, fileName);
    if(mode==AppConfig::AttachNameOriginal)
        return constructOriginalFileName(type, id, fileName);

    return constructLegacyFileName(type, id, fileName);
}


// Имя файла, реально лежащего на диске: перебираются все схемы
// (база могла конвертироваться наполовину или быть открыта
// с другой настройкой режима). Используется всеми путями
// чтения/удаления, запись всегда идет по текущей схеме
// через getInnerFileName()
QString Attach::getExistingInnerFileName() const
{
    QString current=getInnerFileName();
    QString dirName=getFullInnerDirName();

    if(QFile::exists(dirName+"/"+current))
        return current;

    QStringList candidates=candidateFileNames(getField("type"), getField("id"), getField("fileName"));
    foreach(QString candidate, candidates)
        if(candidate!=current && QFile::exists(dirName+"/"+candidate))
            return candidate;

    return current;
}


// Внутрисистемное имя файла (без пути)
// Внутрисистемное имя складывается из идентификатора аттача и расширения, взятого от имени файла
QString Attach::getInnerFileName() const
{
  return constructFileName(getField("type"), getField("id"), getField("fileName"));
}


// Внутрисистемное имя файла с путем
QString Attach::getFullInnerFileName() const
{
  if(getField("type")=="file") // Для файла
  {
    QString resultFileName=getFullInnerDirName()+"/"+getInnerFileName();
    return resultFileName;
  }

  if(getField("type")=="link") // Для линка
    return getField("link");

  criticalError("Bad attach type in getFullInnerFileName():"+getField("type"));

  return "";
}


// Внутрисистемное имя файла с абсолютным путем
QString Attach::getAbsoluteInnerFileName() const
{
  QFileInfo fileInfo( getFullInnerFileName() );

  return fileInfo.absoluteFilePath();
}


// Внутрисистемный путь к файлу (полный)
QString Attach::getFullInnerDirName() const
{
  return parentTable->record->getFullDirName();
}


// Размер аттача в байтах
qint64 Attach::getFileSize() const
{
  QString tempFileName;

  if(fields["type"]=="file")
  {
    QString recordDir=parentTable->record->getFullDirName();
    tempFileName=recordDir+"/"+getExistingInnerFileName();
  }

  if(fields["type"]=="link")
    tempFileName=getField("link");

  QFile tempFile(tempFileName);

  if(!tempFile.exists())
    return 0;
  else
    return tempFile.size();
}


// Шифрация аттача на диске и в памяти
void Attach::encrypt(unsigned int area)
{
  // В этом методе важна последовательность действий,
  // чтобы не получилась ситуации, когда часть данных зашифрована,
  // а другая пытается их использовать, а флаг шифрации еще не установлен

  // Если аттач уже зашифрован, значит есть какая-то ошибка в логике выше
  if(getField("crypt")=="1")
    criticalError("Attach::encrypt() : Cant encrypt already encrypted attach.");


  // Шифруется файл
  if(area & areaFile)
    if(getField("type")=="file")
      CryptService::encryptFile(globalParameters.getCryptKey(), getFullInnerFileName());

  // Шифруется содержимое файла в памяти, если таковое есть
  if(area & areaMemory)
    if(liteFlag==false && fileContent.length()>0)
      fileContent=CryptService::encryptByteArray(globalParameters.getCryptKey(), fileContent);


  // Шифруются поля, которые подлежат шифрованию
  foreach( QString fieldName, fieldCryptedList() )
  {
    // У аттача с типом file не должно быть обращений к полю link (оно не должно использоваться)
    if(getField("type")=="file" && fieldName=="link")
      continue;

    // Если поле с указанным именем существует
    if(getField(fieldName).length()>0)
      setFieldSource(fieldName, CryptService::encryptString( globalParameters.getCryptKey(), getField(fieldName)));
  }

  // Устанавливается флаг, что запись зашифрована
  setField("crypt", "1");
}


// Расшифровка аттача на диске и в памяти
void Attach::decrypt(unsigned int area)
{
  // Если аттач не зашифрован, и происходит расшифровка, значит есть какая-то ошибка в логике выше
  if(getField("crypt")!="1")
    criticalError("Attach::decrypt() : Cant decrypt unencrypted attach.");

  // Расшифровывается файл
  if(area & areaFile)
    if(getField("type")=="file")
      CryptService::decryptFile(globalParameters.getCryptKey(), getFullInnerFileName());

  // Расшифровывается содержимое файла в памяти, если таковое есть
  if(area & areaMemory)
    if(liteFlag==false && fileContent.length()>0)
      fileContent=CryptService::decryptByteArray(globalParameters.getCryptKey(), fileContent);

  // Расшифровываются поля, которые подлежат шифрованию
  foreach( QString fieldName, fieldCryptedList() )
  {
    // У аттача с типом file не должно быть обращений к полю link (оно не должно использоваться)
    if(getField("type")=="file" && fieldName=="link")
      continue;

    // Если поле с указанным именем существует и содержит данные, оно расшифровывается из исходных зашифрованных данных
    if(getField(fieldName).length()>0)
      setFieldSource(fieldName, CryptService::decryptString( globalParameters.getCryptKey(), fields[fieldName]));
  }

  // Устанавливается флаг, что запись не зашифрована
  setField("crypt", ""); // Отсутствие значения предпочтительней, так как тогда в XML-данные не будет попадать атрибут crypt="0"
}


// Расшифровка переданного DOM-элемента
// Метод статический, он не изменяет сам объект Attach
void Attach::decryptDomElement(QDomElement &iDomElement)
{
  if(iDomElement.hasAttribute("crypt") && iDomElement.attribute("crypt")=="1")
    foreach( QString fieldName, fieldCryptedList() ) // Перебираются зашифрованные поля
      if(iDomElement.hasAttribute(fieldName) && iDomElement.attribute(fieldName).length()>0)
      {
        QString decryptAttribute=CryptService::decryptString( globalParameters.getCryptKey(), iDomElement.attribute(fieldName));

        iDomElement.setAttribute(fieldName, decryptAttribute);
      }

  iDomElement.setAttribute("crypt", "0");
}


void Attach::renameFile(QString newFileName){
  if(newFileName.length()==0){
    showMessageBox(QObject::tr("Invalid empty file name."));
     return;
  }

  QString type=getField("type");  
  if(type=="link")
  {
    // Имя файла для линка менять нельзя
    showMessageBox(QObject::tr("Unable to rename a file which attached as a link."));
    return;
  }

  QFile file(getFullInnerFileName());

  // Подправляются права, чтобы файл под новым именем нормально читался
  file.setPermissions(file.permissions() | QFile::ReadUser | QFile::WriteUser);

  if(!file.exists())
  {
    showMessageBox(QObject::tr("Unable to rename the file %1 from disk: file not found.").arg( getFullInnerFileName() ));
    return;
  }
  
  QString resultFileName=getFullInnerDirName()+"/"+constructFileName(type, getField("id"), newFileName);

  // Результат переименования проверяется до обновления метаданных, иначе
  // при неудаче (занятое имя, права, регистрозависимость) поле fileName
  // указывало бы на несуществующий файл
  if(!file.rename(resultFileName))
  {
    showMessageBox(QObject::tr("Unable to rename the file %1 to %2 from disk.").arg( getFullInnerFileName() ).arg( resultFileName ));
    return;
  }

  setField("fileName", newFileName);
}
