#include "BranchPublisher.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QDomDocument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QUuid>
#include <QCryptographicHash>
#include <QXmlStreamWriter>

#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/teamProfile/TeamProfile.h"
#include "libraries/GitWrapper.h"
#include "libraries/BranchDiffEngine.h"
#include "libraries/helpers/DiskHelper.h"


// Имя завершающего маркера целостности версии публикации.
// Формат — компактный построчный (не JSON): первая строка
//   v1	<publishVersion>
// затем на каждый файл данных строкa
//   <относительный путь>	<sha256hex>
// Построчный формат читается стримингом и не раздувает память на ветках
// с миллионами файлов (в отличие от JSON-объекта)
const QString PUBLICATION_MANIFEST_FILE=QStringLiteral("manifest.json");


// Предварительное объявление канонической сериализации
// (определение — в анонимном пространстве имён ниже)
namespace
{
void writeElementCanonical(QXmlStreamWriter *writer, const QDomElement &element);
QStringList publicationDataFiles(const QString &dirPath);
}


// Санитизация части имени каталога (имя ветки/владельца).
// Оставляются обычные символы, пробелы заменяются underscores,
// недопустимые символы — тоже. Имя не должно начинаться/заканчиваться '_',
// чтобы не спутать его с разделителем ключа и имени
QString BranchPublisher::sanitizeNamePart(const QString &value)
{
    QString result;
    for(int i=0; i<value.size(); ++i)
    {
        QChar ch=value.at(i);
        if(ch=='-' || ch=='.' || ch=='_' || ch==' ' || ch.isLetterOrNumber())
            result+=ch;
        else
            result+='_';
    }

    result=result.simplified();
    result.replace(' ', '_');

    while(result.startsWith('_'))
        result.remove(0, 1);
    while(result.endsWith('_'))
        result.chop(1);

    if(result.isEmpty())
        result=QStringLiteral("unnamed");

    return result;
}


// Имя каталога публикации: <branchId>_<имя ветки>
QString BranchPublisher::buildBranchDirName(const QString &branchId, const QString &branchName)
{
    QString key=branchId.section('_', 0, 0);
    return key+"_"+BranchPublisher::sanitizeNamePart(branchName);
}


// Имя каталога владельца: <ownerId>_<имя владельца>
QString BranchPublisher::buildOwnerDirName(TeamProfile &profile)
{
    if(!profile.isConfigured())
        return QString();

    QString key=profile.getTeamId().section('_', 0, 0);
    QString name=BranchPublisher::sanitizeNamePart(profile.getTeamName());

    return key+"_"+name;
}


// Поиск каталога публикации по ключу (части имени до первого '_')
QString BranchPublisher::findPublicationByNameKey(const QString &parentDir,
                                                  const QString &branchIdKey)
{
    QDir dir(parentDir);
    if(!dir.exists())
        return QString();

    QString prefix=branchIdKey+"_";

    QFileInfoList entries=dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                            QDir::Name | QDir::DirsFirst);
    for(int i=0; i<entries.size(); ++i)
    {
        QString name=entries.at(i).fileName();
        if(name.startsWith(prefix) && !name.endsWith(".tmp") && !name.contains(".rev-")
           && !name.contains(".sync-conflict"))
            return entries.at(i).absoluteFilePath();
    }

    return QString();
}


// Каталог данных публикаций: shareddir/sync (зеркало для Syncthing)
QString BranchPublisher::syncDir(const QString &sharedDir)
{
    return sharedDir+"/sync";
}


// Авторитетная зона владельца: shareddir/local (не синхронизируется)
QString BranchPublisher::localDir(const QString &sharedDir)
{
    return sharedDir+"/local";
}


// Каталог сборки версий: shareddir/.tmp (вне sync/, та же файловая система)
QString BranchPublisher::tmpDir(const QString &sharedDir)
{
    return sharedDir+"/.tmp";
}


// Каталог публикации ветки в авторитете local/
QString BranchPublisher::findLocalPublicationDir(const QString &sharedDir,
                                                 const QString &ownerDirName,
                                                 const QString &branchDirName)
{
    if(sharedDir.isEmpty())
        return QString();

    return BranchPublisher::findPublicationInRoot(BranchPublisher::localDir(sharedDir),
                                                 ownerDirName, branchDirName);
}


// Фактический каталог публикации ветки
QString BranchPublisher::findPublicationDir(const QString &sharedDir,
                                            const QString &ownerDirName,
                                            const QString &branchDirName)
{
    if(sharedDir.isEmpty())
        return QString();

    QString branchIdKey=branchDirName.section('_', 0, 0);

    // Новый макет: область данных sync/
    QString syncPath=BranchPublisher::syncDir(sharedDir);
    QString inSync=BranchPublisher::findPublicationInRoot(syncPath, ownerDirName, branchDirName);
    if(!inSync.isEmpty())
        return inSync;

    // Устаревший макет: публикации прямо в shareddir
    return BranchPublisher::findPublicationInRoot(sharedDir, ownerDirName, branchDirName);
}


// Поиск публикации в одном каталоге-корне
QString BranchPublisher::findPublicationInRoot(const QString &rootDir,
                                               const QString &ownerDirName,
                                               const QString &branchDirName)
{
    if(rootDir.isEmpty())
        return QString();

    QString branchIdKey=branchDirName.section('_', 0, 0);

    // Точное совпадение в каталоге владельца
    if(!ownerDirName.isEmpty())
    {
        QString ownerPath=QFileInfo(rootDir+"/"+ownerDirName).absoluteFilePath();
        if(QFileInfo::exists(ownerPath+"/"+branchDirName))
            return ownerPath+"/"+branchDirName;

        // Поиск по ключу в каталоге владельца (переименование ветки)
        QString byKey=BranchPublisher::findPublicationByNameKey(ownerPath, branchIdKey);
        if(!byKey.isEmpty())
            return byKey;
    }

    // Плоская схема: публикация прямо в корне (профиль был пуст при публикации)
    QString exact=QFileInfo(rootDir+"/"+branchDirName).absoluteFilePath();
    if(QFileInfo::exists(exact))
        return exact;

    return BranchPublisher::findPublicationByNameKey(rootDir, branchIdKey);
}


// Имя каталога-области данных внутри каталога обмена
namespace
{
const QString SyncAreaDirName=QStringLiteral("sync");
}


// Ключи веток в одном каталоге-корне (sync/ или устаревший shareddir)
QSet<QString> BranchPublisher::listPublishedBranchKeysInRoot(const QString &rootDir)
{
    QSet<QString> keys;

    QDir shared(rootDir);
    if(!shared.exists())
        return keys;

    QFileInfoList entries=shared.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                               QDir::Name | QDir::DirsFirst);
    for(int i=0; i<entries.size(); ++i)
    {
        QString name=entries.at(i).fileName();

        // Область данных (sync) обрабатывается как отдельный корень.
        // Конфликтные копии Syncthing — не публикации и не владельцы
        if(name==SyncAreaDirName || name.contains(".sync-conflict"))
            continue;

        // Каталог владельца: <ownerId>_<ownerName>; внутри — публикации
        QDir ownerDir(entries.at(i).absoluteFilePath());
        QFileInfoList publications=ownerDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                          QDir::Name | QDir::DirsFirst);
        for(int j=0; j<publications.size(); ++j)
        {
            QString pubName=publications.at(j).fileName();
            if(pubName.contains('_') && !pubName.startsWith('.') && !pubName.contains(".rev-")
               && !pubName.contains(".sync-conflict"))
                keys.insert(pubName.section('_', 0, 0));
        }

        // Плоская публикация прямо в корне: <branchId>_<name>
        if(name.contains('_') && !name.startsWith('.') && !name.contains(".rev-")
           && !name.contains(".sync-conflict"))
            keys.insert(name.section('_', 0, 0));
    }

    return keys;
}


// Набор ключей веток, уже опубликованных в shareddir.
// Учитываются sync/ (зеркало), local/ (авторитет владельца) и устаревший макет
QSet<QString> BranchPublisher::listPublishedBranchKeys(const QString &sharedDir)
{
    QSet<QString> keys;

    // Авторитет владельца (новый макет)
    keys.unite(BranchPublisher::listPublishedBranchKeysInRoot(BranchPublisher::localDir(sharedDir)));

    // Область данных (новый макет)
    keys.unite(BranchPublisher::listPublishedBranchKeysInRoot(BranchPublisher::syncDir(sharedDir)));

    // Устаревший макет (публикации прямо в shareddir)
    keys.unite(BranchPublisher::listPublishedBranchKeysInRoot(sharedDir));

    return keys;
}


// Список авторитетных публикаций владельца в shared/local/
QList<PublicationMeta> BranchPublisher::listLocalPublications(const QString &sharedDir)
{
    return BranchPublisher::listPublicationsInRoot(BranchPublisher::localDir(sharedDir));
}


// Гарантировать наличие .stignore в sync/ (генерируется кодом, чтобы Syncthing
// не тянул промежуточный churn promotion и временные файлы)
void BranchPublisher::ensureSyncIgnore(const QString &syncPath)
{
    if(syncPath.isEmpty())
        return;

    QDir dir(syncPath);
    if(!dir.exists() && !QDir().mkpath(syncPath))
        return;

    const QString ignorePath=syncPath+"/.stignore";
    if(QFileInfo::exists(ignorePath))
        return;

    QFile ignoreFile(ignorePath);
    if(!ignoreFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&ignoreFile);
    out << "// MyTetra: временные файлы записи и promotion (сгенерировано автоматически)\n";
    out << "*.part\n";
    out << "*.tmp\n";
    out << ".~lock.*\n";
    out << ".tmp-*\n";
    out << "*.rev-*\n";
    ignoreFile.close();
}


// Рекурсивная проверка, содержит ли поддерево опубликованную ветку
namespace
{
bool subtreeContainsPublishedKey(TreeItem *item,
                                 const QSet<QString> &publishedBranchKeys)
{
    if(item->getField("id").section('_', 0, 0).length()>0 &&
       publishedBranchKeys.contains(item->getField("id").section('_', 0, 0)))
        return true;

    for(int i=0; i<item->childCount(); ++i)
        if(subtreeContainsPublishedKey(item->child(i), publishedBranchKeys))
            return true;

    return false;
}
}


// Проверка вложенности публикаций
bool BranchPublisher::hasNestedPublication(TreeItem *startItem,
                                           const QSet<QString> &publishedBranchKeys)
{
    // Предок уже опубликован?
    TreeItem *parent=startItem->parent();
    while(parent)
    {
        QString parentId=parent->getField("id");
        if(parentId!="0" && parentId.length()>0 &&
           publishedBranchKeys.contains(parentId.section('_', 0, 0)))
            return true;

        parent=parent->parent();
    }

    // Ветка содержит опубликованную подветку?
    return subtreeContainsPublishedKey(startItem, publishedBranchKeys);
}


// Текущая версия публикации из meta.json
int BranchPublisher::readPublishVersion(const QString &publicationDir)
{
    QString metaPath=publicationDir+"/meta.json";
    QFile metaFile(metaPath);
    if(!metaFile.open(QIODevice::ReadOnly))
        return 0;

    QJsonDocument doc=QJsonDocument::fromJson(metaFile.readAll());
    metaFile.close();

    if(!doc.isObject())
        return 0;

    return doc.object().value("publishVersion").toInt(0);
}


// Чтение метаданных публикации из meta.json
PublicationMeta BranchPublisher::readPublication(const QString &publicationDir)
{
    PublicationMeta meta;
    meta.dirPath=publicationDir;

    QString metaPath=publicationDir+"/meta.json";
    QFile metaFile(metaPath);
    if(!metaFile.open(QIODevice::ReadOnly))
        return meta;

    QJsonDocument doc=QJsonDocument::fromJson(metaFile.readAll());
    metaFile.close();

    if(!doc.isObject())
        return meta;

    QJsonObject object=doc.object();

    meta.branchId=object.value("branchId").toString();
    meta.title=object.value("title").toString();
    meta.ownerId=object.value("ownerId").toString();
    meta.ownerName=object.value("ownerName").toString();
    meta.ownerEmail=object.value("ownerEmail").toString();
    meta.publishVersion=object.value("publishVersion").toInt(0);
    meta.publishedAt=object.value("publishedAt").toString();
    meta.encryptedSource=object.value("encryptedSource").toBool(false);

    return meta;
}


// Список публикаций в одном каталоге-корне (sync/ или устаревший shareddir)
QList<PublicationMeta> BranchPublisher::listPublicationsInRoot(const QString &rootDir)
{
    QList<PublicationMeta> publications;

    QDir shared(rootDir);
    if(!shared.exists())
        return publications;

    QFileInfoList entries=shared.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot,
                                                QDir::Name | QDir::DirsFirst);
    for(int i=0; i<entries.size(); ++i)
    {
        QString entryPath=entries.at(i).absoluteFilePath();
        QString entryName=entries.at(i).fileName();

        // Область данных обрабатывается как отдельный корень.
        // Конфликтные копии Syncthing — не публикации и не владельцы
        if(entryName==SyncAreaDirName || entryName.contains(".sync-conflict"))
            continue;

        // Каталог владельца: <ownerId>_<ownerName>; внутри — публикации
        QDir ownerDir(entryPath);
        QFileInfoList publicationsInOwner=ownerDir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot,
                                                                 QDir::Name | QDir::DirsFirst);
        for(int j=0; j<publicationsInOwner.size(); ++j)
        {
            QString pubPath=publicationsInOwner.at(j).absoluteFilePath();

            // Пропуск служебных, временных каталогов и конфликтов Syncthing
            if(publicationsInOwner.at(j).fileName().contains(".sync-conflict"))
                continue;
            if(!QFileInfo::exists(pubPath+"/meta.json"))
                continue;

            PublicationMeta meta=BranchPublisher::readPublication(pubPath);
            if(!meta.branchId.isEmpty())
                publications.append(meta);
        }

        // Плоская публикация прямо в корне: <branchId>_<name>
        // (конфликтные копии Syncthing — нет)
        if(!entryName.contains(".sync-conflict")
           && QFileInfo::exists(entryPath+"/meta.json"))
        {
            PublicationMeta meta=BranchPublisher::readPublication(entryPath);
            if(!meta.branchId.isEmpty())
                publications.append(meta);
        }
    }

    return publications;
}


// Список всех публикаций в shareddir
QList<PublicationMeta> BranchPublisher::listPublications(const QString &sharedDir)
{
    QList<PublicationMeta> publications;

    // Область данных (новый макет)
    publications.append(BranchPublisher::listPublicationsInRoot(BranchPublisher::syncDir(sharedDir)));

    // Устаревший макет (публикации прямо в shareddir)
    publications.append(BranchPublisher::listPublicationsInRoot(sharedDir));

    return publications;
}


// Сбор branch.xml из mytetra.xml (результат KnowTreeModel::exportBranchToDirectory)
bool BranchPublisher::buildBranchXmlFromMytetraXml(const QString &tempDir)
{
    QString mytetraXmlPath=tempDir+"/mytetra.xml";
    QFile xmlFile(mytetraXmlPath);
    if(!xmlFile.open(QIODevice::ReadOnly))
        return false;

    QDomDocument sourceDoc;
    if(!sourceDoc.setContent(&xmlFile))
    {
        xmlFile.close();
        return false;
    }
    xmlFile.close();

    // Извлечение узла ветки из структуры mytetra.xml:
    // <root><format .../><content><node ...>...</node></content></root>
    QDomElement contentElement=sourceDoc.documentElement()
                                .firstChildElement("content");
    QDomElement branchNode=contentElement.firstChildElement("node");
    if(branchNode.isNull())
        return false;

    // Новый документ формата публикации: <branch><node ...>...</node></branch>
    QDomDocument branchDoc("mytetradoc");
    QDomElement branchRoot=branchDoc.createElement("branch");
    branchDoc.appendChild(branchRoot);

    QDomNode importedNode=branchDoc.importNode(branchNode, true);
    if(importedNode.isNull())
        return false;

    branchRoot.appendChild(importedNode);

    // Запись branch.xml канонической сериализацией: сортированные атрибуты,
    // байт-стабильность между запусками (QDom toString() для этого не годится —
    // порядок атрибутов в нём случаен для каждого процесса)
    QFile branchFile(tempDir+"/branch.xml");
    if(!branchFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QXmlStreamWriter writer(&branchFile);
    writer.setCodec("UTF-8");
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(1);
    writer.writeStartDocument();
    writer.writeDTD("<!DOCTYPE mytetradoc>");
    writer.writeStartElement("branch");
    for(QDomElement child=branchRoot.firstChildElement(); !child.isNull();
        child=child.nextSiblingElement())
        writeElementCanonical(&writer, child);
    writer.writeEndElement();
    writer.writeEndDocument();
    branchFile.close();

    if(writer.hasError())
        return false;

    // Исходный mytetra.xml больше не нужен
    QFile::remove(mytetraXmlPath);

    return true;
}


// Запись meta.json
bool BranchPublisher::writeMetaJson(const QString &tempDir, TreeItem *startItem,
                                    TeamProfile &profile, int publishVersion,
                                    bool encryptedSource)
{
    QJsonObject meta;
    meta["schemaVersion"]=1;
    meta["branchId"]=startItem->getField("id");
    meta["title"]=startItem->getField("name");
    meta["ownerId"]=profile.getTeamId();
    meta["ownerName"]=profile.getTeamName();
    meta["ownerEmail"]=profile.getTeamEmail();
    meta["publishVersion"]=publishVersion;
    meta["publishedAt"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    meta["encryptedSource"]=encryptedSource;
    meta["notes"]=QString();

    QFile metaFile(tempDir+"/meta.json");
    if(!metaFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QJsonDocument doc(meta);
    metaFile.write(doc.toJson(QJsonDocument::Indented));
    metaFile.close();

    return true;
}


// Запись файла manifest.json: карта относительный путь -> sha256 всех файлов
// данных каталога (branch.xml + records/). Служебные файлы (meta.json,
// manifest.json, .git, скрытые) не входят — они не данные публикации.
// Гарантия: manifest.json пишется последним в собранном каталоге, поэтому
// его наличие с совпадающим publishVersion означает полный и согласованный
// перенос версии через Syncthing. Формат построчный (v1):
//   первая строка "v1\t<publishVersion>", далее "путь\tsha256hex"
bool BranchPublisher::writePublicationManifest(const QString &dirPath,
                                               const QString &branchId,
                                               int publishVersion)
{
    Q_UNUSED(branchId)

    QFile manifestFile(dirPath+"/"+PUBLICATION_MANIFEST_FILE);
    if(!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&manifestFile);
    out << "v1\t" << publishVersion << "\n";

    // Список файлов данных — единый с promotion (см. publicationDataFiles),
    // минус служебные meta.json и сам манифест. Сортированный порядок —
    // канонический для стриминговой сверки при проверке
    QStringList files;
    const QStringList dataFiles=publicationDataFiles(dirPath);
    for(const QString &relative : dataFiles)
    {
        if(relative==PUBLICATION_MANIFEST_FILE || relative=="meta.json")
            continue;
        files.append(relative);
    }

    for(const QString &relative : files)
    {
        QFile f(dirPath+"/"+relative);
        if(!f.open(QIODevice::ReadOnly))
            continue;
        const QByteArray sha=QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256);
        f.close();
        out << relative << "\t" << QString::fromLatin1(sha.toHex()) << "\n";
    }

    manifestFile.close();
    return true;
}


// Чтение манифеста: карта путь -> sha256 (hex) и версия публикации
void BranchPublisher::readManifest(const QString &publicationDir,
                                   QMap<QString, QString> &fileShas,
                                   int *publishVersionOut)
{
    fileShas.clear();
    if(publishVersionOut)
        *publishVersionOut=0;

    QFile manifestFile(publicationDir+"/"+PUBLICATION_MANIFEST_FILE);
    if(!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&manifestFile);
    bool firstLine=true;
    while(!in.atEnd())
    {
        const QString line=in.readLine();

        // Пропуск пустых строк и возможного завершающего переноса
        if(line.isEmpty())
            continue;

        const int tab=line.indexOf('\t');
        if(tab<0)
            continue;

        const QString key=line.left(tab);
        const QString value=line.mid(tab+1);

        if(firstLine)
        {
            firstLine=false;
            // Заголовок: "v1\t<publishVersion>"
            if(key=="v1")
            {
                if(publishVersionOut)
                    *publishVersionOut=value.toInt();
                continue; // заголовок в карту файлов не попадает
            }
        }

        fileShas.insert(key, value);
    }
    manifestFile.close();
}


// Версия публикации из заголовка манифеста (0 — манифеста нет/не читается)
int BranchPublisher::manifestPublishVersion(const QString &publicationDir)
{
    QFile manifestFile(publicationDir+"/"+PUBLICATION_MANIFEST_FILE);
    if(!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;

    const QString header=manifestFile.readLine();
    manifestFile.close();

    // Заголовок: "v1\t<publishVersion>"
    const int tab=header.indexOf('\t');
    if(header.startsWith("v1\t") && tab>=0)
    {
        bool ok=false;
        const int version=header.mid(tab+1).trimmed().toInt(&ok);
        if(ok)
            return version;
    }

    return 0;
}


// Признак «данные версии ещё в пути» (только заголовок манифеста, дёшево)
bool BranchPublisher::isPublicationDataPending(const QString &publicationDir,
                                               int publishVersion)
{
    const int manifestVersion=BranchPublisher::manifestPublishVersion(publicationDir);
    if(manifestVersion==publishVersion)
        return false;

    // Устаревший per-branch макет без манифеста — штатный (целостность за .git)
    if(manifestVersion==0 && QFileInfo::exists(publicationDir+"/.git"))
        return false;

    return true;
}


// Проверка целостности публикации на конкретную версию
bool BranchPublisher::publicationVersionComplete(const QString &publicationDir,
                                                 int publishVersion)
{
    // Устаревшая per-branch публикация: целостность гарантируется её .git
    if(QFileInfo::exists(publicationDir+"/.git"))
        return true;

    QMap<QString, QString> fileShas;
    int manifestVersion=0;
    BranchPublisher::readManifest(publicationDir, fileShas, &manifestVersion);

    // Манифест обязан соответствовать текущей версии публикации
    if(manifestVersion!=publishVersion)
        return false;

    // Каждый файл из манифеста должен присутствовать и совпадать по sha256
    for(auto it=fileShas.constBegin(); it!=fileShas.constEnd(); ++it)
    {
        QFile f(publicationDir+"/"+it.key());
        if(!f.open(QIODevice::ReadOnly))
            return false;
        const QByteArray sha=QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256);
        f.close();
        if(QString::fromLatin1(sha.toHex())!=it.value())
            return false;
    }

    return true;
}


// Размер содержимого каталога
quint64 BranchPublisher::calculateDirSizeBytes(const QString &dirPath)
{
    quint64 totalBytes=0;
    QDirIterator it(dirPath, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while(it.hasNext())
    {
        it.next();
        totalBytes+=static_cast<quint64>(it.fileInfo().size());
    }
    return totalBytes;
}


// Имя файла истории версий публикации
const QString PUBLICATION_CHANGELOG_FILE=QStringLiteral("changelog.json");


// Атомарное копирование одного файла (через .part + rename)
namespace
{
bool copyFileAtomic(const QString &sourcePath, const QString &destPath)
{
    QDir parent(QFileInfo(destPath).absolutePath());
    if(!parent.exists() && !parent.mkpath(parent.absolutePath()))
        return false;

    const QString partPath=destPath+QStringLiteral(".part-")
                           +QUuid::createUuid().toString().remove('{').remove('}');
    if(!QFile::copy(sourcePath, partPath))
        return false;

    // Замена целевого файла (rename поверх существующего запрещён —
    // сначала удаляем, окно мизерное, читатели защищены манифестом)
    if(QFileInfo::exists(destPath) && !QFile::remove(destPath))
    {
        QFile::remove(partPath);
        return false;
    }

    if(!QDir().rename(partPath, destPath))
    {
        QFile::remove(partPath);
        return false;
    }

    return true;
}


// Список файлов данных публикации (относительные пути), исключая служебные:
// .git/, скрытые dot-файлы, .tmp-*, *.rev-*, .stignore, *.part-*,
// а также конфликтные копии Syncthing (*.sync-conflict-*).
// Конфликты не удаляются и не попадают в манифест: их разруливает человек
// в файловом менеджере; подписчик их не видит (целостность — по манифесту).
// manifest.json, changelog.json и meta.json включаются в список,
// но promotion копирует их последними (см. promoteToSync)
QStringList publicationDataFiles(const QString &dirPath)
{
    QStringList files;
    QDirIterator it(dirPath, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while(it.hasNext())
    {
        it.next();
        const QString relative=QDir(dirPath).relativeFilePath(it.filePath());
        if(relative.startsWith(".git/")
           || relative.startsWith(".")
           || relative.contains(".rev-")
           || relative==".stignore"
           || relative.contains(".part-")
           || relative.contains(".sync-conflict"))
            continue;
        files.append(relative);
    }
    files.sort(Qt::CaseInsensitive);
    return files;
}


// Рекурсивная запись DOM-элемента через QXmlStreamWriter с сортировкой
// атрибутов (каноническая форма). Нужна потому, что QDom хранит атрибуты
// в хеше: порядок при парсинге случаен для каждого запуска процесса, и
// branch.xml через toString() менялся байтово при каждой сборке без
// семантических изменений (пустые версии, рост журнала, вечные «обновления»
// у подписчиков). Потоковая запись с сортированными атрибутами даёт
// байт-стабильный результат для одинакового DOM
void writeElementCanonical(QXmlStreamWriter *writer, const QDomElement &element)
{
    writer->writeStartElement(element.tagName());

    QDomNamedNodeMap attrs=element.attributes();
    QMap<QString, QString> sorted;
    for(int i=0; i<attrs.count(); ++i)
        sorted.insert(attrs.item(i).nodeName(), attrs.item(i).nodeValue());
    for(auto it=sorted.constBegin(); it!=sorted.constEnd(); ++it)
        writer->writeAttribute(it.key(), it.value());

    for(QDomNode child=element.firstChild(); !child.isNull(); child=child.nextSibling())
    {
        if(child.isElement())
            writeElementCanonical(writer, child.toElement());
        else if(child.isText() && !child.nodeValue().trimmed().isEmpty())
            writer->writeCharacters(child.nodeValue());
    }

    writer->writeEndElement();
}


// Сериализация списка изменений версии в JSON-массив (для changelog.json)
QString serializeChangesToJson(const QList<DiffChange> &changes)
{
    QJsonArray array;
    for(const DiffChange &change : changes)
    {
        QJsonObject object;
        switch(change.type)
        {
            case DiffChange::RecordAdd:    object["kind"]=QStringLiteral("add"); break;
            case DiffChange::RecordUpdate: object["kind"]=QStringLiteral("update"); break;
            case DiffChange::RecordDelete: object["kind"]=QStringLiteral("delete"); break;
            case DiffChange::BranchRename: object["kind"]=QStringLiteral("branch-rename"); break;
            case DiffChange::BranchMove:   object["kind"]=QStringLiteral("branch-move"); break;
            case DiffChange::BranchAdd:    object["kind"]=QStringLiteral("branch-add"); break;
            case DiffChange::BranchDelete: object["kind"]=QStringLiteral("branch-delete"); break;
        }
        object["recordId"]=change.recordId;
        object["branchId"]=change.branchId;
        object["title"]=change.title;
        if(change.type==DiffChange::RecordUpdate)
        {
            QJsonArray fieldNames;
            for(const auto &field : change.fields)
                fieldNames.append(field.first);
            object["fields"]=fieldNames;
            object["text"]=change.textChanged;
        }
        array.append(object);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}


// Построение списка изменений между старой local-публикацией и новой сборкой.
// Контент записей читается из каталогов (git не используется)
QList<DiffChange> buildChangesBetweenDirs(const QString &oldPublicationDir,
                                          const QString &newStagingDir)
{
    FragmentData oldFragment;
    FragmentData newFragment;

    QFile oldXml(oldPublicationDir+"/branch.xml");
    if(oldXml.open(QIODevice::ReadOnly))
    {
        oldFragment=BranchDiffEngine::parseFragment(QString::fromUtf8(oldXml.readAll()));
        oldXml.close();
        const QString oldDir=oldPublicationDir;
        BranchDiffEngine::fillTextShas(oldFragment, [oldDir](const DiffRecordData &recordData)
        {
            QFile f(oldDir+"/records/"+recordData.dir+"/"
                    +(recordData.file.isEmpty() ? QStringLiteral("text.html") : recordData.file));
            if(!f.open(QIODevice::ReadOnly))
                return QByteArray();
            return f.readAll();
        });
    }

    QFile newXml(newStagingDir+"/branch.xml");
    if(newXml.open(QIODevice::ReadOnly))
    {
        newFragment=BranchDiffEngine::parseFragment(QString::fromUtf8(newXml.readAll()));
        newXml.close();
        const QString newDir=newStagingDir;
        BranchDiffEngine::fillTextShas(newFragment, [newDir](const DiffRecordData &recordData)
        {
            QFile f(newDir+"/records/"+recordData.dir+"/"
                    +(recordData.file.isEmpty() ? QStringLiteral("text.html") : recordData.file));
            if(!f.open(QIODevice::ReadOnly))
                return QByteArray();
            return f.readAll();
        });
    }

    return BranchDiffEngine::buildChangeList(oldFragment, newFragment);
}
}


// Дописать запись версии в changelog.json публикации (append-only)
bool BranchPublisher::appendChangelog(const QString &publicationDir,
                                      int publishVersion,
                                      const QString &ownerName,
                                      const QString &changesJson)
{
    const QString changelogPath=publicationDir+"/"+PUBLICATION_CHANGELOG_FILE;

    QJsonArray entries;
    QFile existing(changelogPath);
    if(existing.open(QIODevice::ReadOnly))
    {
        const QJsonDocument doc=QJsonDocument::fromJson(existing.readAll());
        existing.close();
        if(doc.isObject())
            entries=doc.object().value("entries").toArray();
    }

    QJsonObject entry;
    entry["v"]=publishVersion;
    entry["at"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry["owner"]=ownerName;
    entry["changes"]=QJsonDocument::fromJson(changesJson.toUtf8()).array();

    entries.append(entry);

    QJsonObject root;
    root["schemaVersion"]=1;
    root["entries"]=entries;

    // Атомарная запись (tmp + rename)
    const QString partPath=changelogPath+QStringLiteral(".part-")
                           +QUuid::createUuid().toString().remove('{').remove('}');
    QFile part(partPath);
    if(!part.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    part.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    part.close();

    if(QFileInfo::exists(changelogPath) && !QFile::remove(changelogPath))
    {
        QFile::remove(partPath);
        return false;
    }

    if(!QDir().rename(partPath, changelogPath))
    {
        QFile::remove(partPath);
        return false;
    }

    return true;
}


// Promotion зеркала: copy-changed из local-публикации в sync-публикацию
bool BranchPublisher::promoteToSync(const QString &localPublicationDir,
                                    const QString &syncPublicationDir,
                                    QString *errorMessage)
{
    if(localPublicationDir.isEmpty() || !QFileInfo::exists(localPublicationDir+"/meta.json"))
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Local publication not found");
        return false;
    }

    QDir syncParent(QFileInfo(syncPublicationDir).absolutePath());
    if(!syncParent.exists() && !syncParent.mkpath(syncParent.absolutePath()))
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Cant create sync parent directory");
        return false;
    }

    QDir syncDir(syncPublicationDir);
    if(!syncDir.exists() && !QDir().mkpath(syncPublicationDir))
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Cant create sync publication directory");
        return false;
    }

    // Карта local: путь -> sha256 (по манифесту; fallback — посчитать с диска)
    QMap<QString, QString> localShas;
    int localVersion=0;
    BranchPublisher::readManifest(localPublicationDir, localShas, &localVersion);
    if(localShas.isEmpty())
    {
        const QMap<QString, QByteArray> digest=BranchPublisher::publicationContentDigest(localPublicationDir);
        for(auto it=digest.constBegin(); it!=digest.constEnd(); ++it)
            localShas.insert(it.key(), QString::fromLatin1(it.value().toHex()));
    }

    // Карта sync: путь -> sha256 (факт с диска)
    QMap<QString, QString> syncShas;
    {
        const QStringList syncFiles=publicationDataFiles(syncPublicationDir);
        for(const QString &relative : syncFiles)
        {
            if(relative=="meta.json"
               || relative==PUBLICATION_CHANGELOG_FILE
               || relative==PUBLICATION_MANIFEST_FILE)
                continue;
            QFile f(syncPublicationDir+"/"+relative);
            if(!f.open(QIODevice::ReadOnly))
                continue;
            syncShas.insert(relative,
                            QString::fromLatin1(QCryptographicHash::hash(f.readAll(),
                                                                         QCryptographicHash::Sha256).toHex()));
            f.close();
        }
    }

    // Копирование новых/изменённых файлов данных (кроме служебных — они последними)
    for(auto it=localShas.constBegin(); it!=localShas.constEnd(); ++it)
    {
        if(syncShas.value(it.key())==it.value())
            continue;

        if(!copyFileAtomic(localPublicationDir+"/"+it.key(), syncPublicationDir+"/"+it.key()))
        {
            if(errorMessage)
                *errorMessage=QStringLiteral("Copy to sync failed: ")+it.key();
            return false;
        }
    }

    // Удаление из sync/ файлов, убранных владельцем
    {
        const QStringList syncFiles=publicationDataFiles(syncPublicationDir);
        for(const QString &relative : syncFiles)
        {
            if(relative=="meta.json"
               || relative==PUBLICATION_CHANGELOG_FILE
               || relative==PUBLICATION_MANIFEST_FILE)
                continue;
            if(!localShas.contains(relative))
                QFile::remove(syncPublicationDir+"/"+relative);
        }
    }

    // Служебные файлы — последними: meta.json, changelog.json, manifest.json
    const QStringList serviceFiles=QStringList()
                                   << QStringLiteral("meta.json")
                                   << PUBLICATION_CHANGELOG_FILE
                                   << PUBLICATION_MANIFEST_FILE;
    for(const QString &serviceFile : serviceFiles)
    {
        if(!QFileInfo::exists(localPublicationDir+"/"+serviceFile))
            continue;
        if(!copyFileAtomic(localPublicationDir+"/"+serviceFile,
                           syncPublicationDir+"/"+serviceFile))
        {
            if(errorMessage)
                *errorMessage=QStringLiteral("Copy to sync failed: ")+serviceFile;
            return false;
        }
    }

    return true;
}


// Восстановить sync-публикацию из local-авторитета (перепромоут без git)
bool BranchPublisher::restoreSyncFromLocal(const QString &sharedDir,
                                           const QString &syncPublicationDir,
                                           QString *errorMessage)
{
    if(sharedDir.isEmpty() || syncPublicationDir.isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Empty path");
        return false;
    }

    // sync/<owner>/<branch> -> local/<owner>/<branch> (плоская схема — без owner)
    const QString syncPrefix=BranchPublisher::syncDir(sharedDir)+"/";
    if(!syncPublicationDir.startsWith(syncPrefix))
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Not a sync/ publication");
        return false;
    }

    const QString relative=syncPublicationDir.mid(syncPrefix.length());
    const QString localPublicationDir=BranchPublisher::localDir(sharedDir)+"/"+relative;

    if(!QFileInfo::exists(localPublicationDir+"/meta.json"))
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Local authority not found");
        return false;
    }

    return BranchPublisher::promoteToSync(localPublicationDir, syncPublicationDir, errorMessage);
}
bool BranchPublisher::moveTempToFinal(const QString &tempDir, const QString &finalDir,
                                      Operation operation)
{
    // Для обновления старый каталог сначала отводится в резерв
    QString backupDir;
    if(operation==Operation::Update && QFileInfo::exists(finalDir))
    {
        backupDir=finalDir+".rev-"+QUuid::createUuid().toString().remove('{').remove('}');
        if(!QDir().rename(finalDir, backupDir))
            return false;
    }

    // Перемещение собранного каталога на место (в пределах той же файловой системы)
    if(!QDir().rename(tempDir, finalDir))
    {
        // При неудаче восстанавливаем резервную копию
        if(!backupDir.isEmpty() && QFileInfo::exists(finalDir))
            QDir().rename(backupDir, finalDir);
        return false;
    }

    // Удаление резервной копии
    if(!backupDir.isEmpty() && QFileInfo::exists(backupDir))
    {
        QDir backup(backupDir);
        backup.removeRecursively();
    }

    return true;
}


// Основная операция публикации/обновления ветки
BranchPublisher::Result BranchPublisher::publishBranch(KnowTreeModel *model,
                                                       TreeItem *startItem,
                                                       TeamProfile &profile,
                                                       Operation operation)
{
    Result result;

    // Каталог обмена
    QString sharedDir=profile.getSharedDir();
    if(sharedDir.isEmpty())
    {
        result.errorMessage=QStringLiteral("Empty shared directory");
        return result;
    }

    QDir shared(sharedDir);
    if(!shared.exists())
    {
        result.errorMessage=QStringLiteral("Shared directory does not exist: ")+sharedDir;
        return result;
    }

    // Имена каталогов
    QString ownerDirName=BranchPublisher::buildOwnerDirName(profile);
    QString branchDirName=BranchPublisher::buildBranchDirName(startItem->getField("id"),
                                                              startItem->getField("name"));

    QString publicationDir=BranchPublisher::findPublicationDir(sharedDir, ownerDirName, branchDirName);

    // Устаревший per-branch макет (с .git внутри публикации): обновление на месте
    // в своём репозитории, как раньше
    if(!publicationDir.isEmpty() && QFileInfo::exists(publicationDir+"/.git"))
    {
        return BranchPublisher::publishLegacyBranch(model, startItem, profile,
                                                   operation, publicationDir,
                                                   ownerDirName, branchDirName, result);
    }

    // Новый макет (ADR-016): авторитет local/ + зеркало sync/.
    // Конечные каталоги в обеих зонах (имена владельца/ветки совпадают)
    QString localTargetDir;
    QString syncTargetDir;
    if(!ownerDirName.isEmpty())
    {
        localTargetDir=QFileInfo(BranchPublisher::localDir(sharedDir)+"/"+ownerDirName
                                 +"/"+branchDirName).absoluteFilePath();
        syncTargetDir=QFileInfo(BranchPublisher::syncDir(sharedDir)+"/"+ownerDirName
                                +"/"+branchDirName).absoluteFilePath();
    }
    else
    {
        localTargetDir=QFileInfo(BranchPublisher::localDir(sharedDir)+"/"+branchDirName).absoluteFilePath();
        syncTargetDir=QFileInfo(BranchPublisher::syncDir(sharedDir)+"/"+branchDirName).absoluteFilePath();
    }

    // Миграция: публикация была в sync/ до ADR-016, в local/ её нет —
    // засеиваем авторитет из зеркала перед первым обновлением
    const bool localExists=QFileInfo::exists(localTargetDir+"/meta.json");
    const bool syncExists=QFileInfo::exists(syncTargetDir+"/meta.json");
    if(operation==Operation::Update && !localExists && syncExists)
    {
        QDir localParent(QFileInfo(localTargetDir).absolutePath());
        if(!localParent.exists() && !localParent.mkpath(localParent.absolutePath()))
        {
            result.errorMessage=QStringLiteral("Cant create directory: ")+localParent.absolutePath();
            return result;
        }
        // Посев: полное копирование зеркала в авторитет (мало, однократно)
        if(!copyDirectoryRecursively(syncTargetDir, localTargetDir))
        {
            result.errorMessage=QStringLiteral("Seed local authority from sync failed");
            return result;
        }
    }

    const bool authorityExists=QFileInfo::exists(localTargetDir+"/meta.json")
                               || QFileInfo::exists(localTargetDir);

    // Явная проверка отсутствия/наличия публикации (по авторитету local/)
    if(operation==Operation::Publish && authorityExists)
    {
        result.errorMessage=QStringLiteral("Publication already exists: ")+localTargetDir;
        return result;
    }
    if(operation==Operation::Update && !authorityExists && !syncExists)
    {
        result.errorMessage=QStringLiteral("Publication not found: ")+localTargetDir;
        return result;
    }

    // Подготовка каталогов-родителей обеих зон
    QDir localParent(QFileInfo(localTargetDir).absolutePath());
    if(!localParent.exists() && !localParent.mkpath(localParent.absolutePath()))
    {
        result.errorMessage=QStringLiteral("Cant create directory: ")+localParent.absolutePath();
        return result;
    }
    QDir syncParent(QFileInfo(syncTargetDir).absolutePath());
    if(!syncParent.exists() && !syncParent.mkpath(syncParent.absolutePath()))
    {
        result.errorMessage=QStringLiteral("Cant create directory: ")+syncParent.absolutePath();
        return result;
    }
    BranchPublisher::ensureSyncIgnore(BranchPublisher::syncDir(sharedDir));

    // Временный каталог сборки — в shared/.tmp/ (вне sync/, та же файловая система)
    QDir stagingRoot(BranchPublisher::tmpDir(sharedDir));
    if(!stagingRoot.exists() && !stagingRoot.mkpath(stagingRoot.absolutePath()))
    {
        result.errorMessage=QStringLiteral("Cant create temp directory");
        return result;
    }
    QString tempDir=stagingRoot.absolutePath()
                    +"/.tmp-publish-"+branchDirName.section('_',0,0)
                    +"-"+QUuid::createUuid().toString().remove('{').remove('}');
    QDir tempRoot;
    if(!tempRoot.mkpath(tempDir))
    {
        result.errorMessage=QStringLiteral("Cant create temp directory: ")+tempDir;
        return result;
    }

    // Сборка: mytetra.xml + base/ (с расшифровкой при необходимости)
    if(!model->exportBranchToDirectory(startItem, tempDir))
    {
        QDir(tempDir).removeRecursively();
        result.errorMessage=QStringLiteral("Export branch to directory failed");
        return result;
    }

    // Преобразование mytetra.xml -> branch.xml
    if(!BranchPublisher::buildBranchXmlFromMytetraXml(tempDir))
    {
        QDir(tempDir).removeRecursively();
        result.errorMessage=QStringLiteral("Build branch.xml failed");
        return result;
    }

    // Каталог записей: base/ -> records/
    if(QFileInfo::exists(tempDir+"/base"))
    {
        if(!QDir(tempDir).rename("base", "records"))
        {
            QDir(tempDir).removeRecursively();
            result.errorMessage=QStringLiteral("Rename base/ to records/ failed");
            return result;
        }
    }
    else if(!QFileInfo::exists(tempDir+"/records"))
    {
        if(!QDir(tempDir).mkpath("records"))
        {
            QDir(tempDir).removeRecursively();
            result.errorMessage=QStringLiteral("Cant create records directory");
            return result;
        }
    }

    // meta.json (версия — по авторитету local/)
    int newPublishVersion=(operation==Operation::Update)
                            ? BranchPublisher::readPublishVersion(localTargetDir)+1
                            : 1;
    if(newPublishVersion<1)
        newPublishVersion=1;

    bool encryptedSource=model->isItemContainsCryptBranches(startItem);

    if(!BranchPublisher::writeMetaJson(tempDir, startItem, profile, newPublishVersion,
                                       encryptedSource))
    {
        QDir(tempDir).removeRecursively();
        result.errorMessage=QStringLiteral("Write meta.json failed");
        return result;
    }

    // Проверка объёма для предупреждения о росте .git
    quint64 recordsSize=BranchPublisher::calculateDirSizeBytes(tempDir+"/records");
    result.largeContent=(recordsSize > 20*1024*1024);

    // Для обновления: если содержимое не изменилось —
    // версия не трогается, коммиты не создаются (важно для автопубликации).
    // Но если зеркало sync/ отстало/побито — перепромоут без поднятия версии
    if(operation==Operation::Update
       && authorityExists
       && BranchPublisher::isPublicationContentEqual(localTargetDir, tempDir))
    {
        QDir(tempDir).removeRecursively();

        result.success=true;
        result.unchanged=true;
        result.publicationDir=syncTargetDir;
        result.publishVersion=BranchPublisher::readPublishVersion(localTargetDir);

        // Зеркало должно соответствовать авторитету даже при пустом обновлении
        QString promoteError;
        if(!BranchPublisher::publicationVersionComplete(syncTargetDir, result.publishVersion)
           && !BranchPublisher::promoteToSync(localTargetDir, syncTargetDir, &promoteError))
            qDebug() << "BranchPublisher: re-promote on unchanged failed:" << promoteError;

        return result;
    }

    // temp+rename в авторитет local/ (бэкап .rev — тоже в local/, вне sync/)
    if(!BranchPublisher::writePublicationManifest(tempDir, startItem->getField("id"),
                                                   newPublishVersion))
    {
        QDir(tempDir).removeRecursively();
        result.errorMessage=QStringLiteral("Write manifest.json failed");
        return result;
    }

    // История версии: дифф старого local/ против новой сборки (до замены)
    QString changesJson=QStringLiteral("[]");
    if(operation==Operation::Update && authorityExists)
    {
        const QList<DiffChange> versionChanges=buildChangesBetweenDirs(localTargetDir, tempDir);
        changesJson=serializeChangesToJson(versionChanges);
    }

    if(!BranchPublisher::moveTempToFinal(tempDir, localTargetDir, operation))
    {
        QDir(tempDir).removeRecursively();
        result.errorMessage=QStringLiteral("Move publication to local authority failed");
        return result;
    }

    if(!BranchPublisher::appendChangelog(localTargetDir, newPublishVersion,
                                         profile.getTeamName(), changesJson))
        qDebug() << "BranchPublisher: append changelog failed (non-fatal)";

    // git: единый журнал shared/.git. Дисциплина двух коммитов (ADR-016):
    // коммит 1 — только local/ (source of truth), затем promotion в sync/,
    // коммит 2 — только sync/ и только при целостной версии (гард от битого).
    // Коммиты не фатальны: данные уже записаны, дифф/импорт/детект работают
    // по диджестам; watcher доберёт snapshot позже.
    // Без git в PATH пропускается только журнал
    const QString branchKey=startItem->getField("id").section('_', 0, 0);
    if(!GitWrapper::isGitAvailable())
    {
        QString promoteError;
        if(!BranchPublisher::promoteToSync(localTargetDir, syncTargetDir, &promoteError))
        {
            result.success=false;
            result.errorMessage=promoteError;
            return result;
        }

        result.success=true;
        result.publicationDir=syncTargetDir;
        result.publishVersion=newPublishVersion;
        result.journalDisabled=true;

        return result;
    }

    const QString repoRoot=sharedDir;
    if(!QFileInfo::exists(repoRoot+"/.git"))
    {
        if(!GitWrapper::initRepository(repoRoot))
            qDebug() << "BranchPublisher: git init failed (non-fatal)";
    }
    GitWrapper::setLocalIdentity(repoRoot, profile.getTeamName(), profile.getTeamEmail());

    // Коммит 1: авторитет
    {
        const QString localRel=QDir(repoRoot).relativeFilePath(localTargetDir);
        const QString message=QStringLiteral("[%1] v%2: %3").arg(branchKey).arg(newPublishVersion)
                              .arg(startItem->getField("name"));
        QString commitHash;
        GitWrapper::commitIfChanged(repoRoot, message, localRel, &commitHash);
    }

    // Promotion зеркала
    {
        QString promoteError;
        if(!BranchPublisher::promoteToSync(localTargetDir, syncTargetDir, &promoteError))
        {
            result.success=false;
            result.errorMessage=promoteError;
            return result;
        }
    }

    // Гард: битое зеркало в журнал не попадает
    if(!BranchPublisher::publicationVersionComplete(syncTargetDir, newPublishVersion))
    {
        result.success=false;
        result.errorMessage=QStringLiteral("Sync mirror verification failed");
        return result;
    }

    // Коммит 2: зеркало
    {
        const QString syncRel=QDir(repoRoot).relativeFilePath(syncTargetDir);
        const QString message=QStringLiteral("mirror [%1] v%2: %3").arg(branchKey).arg(newPublishVersion)
                              .arg(startItem->getField("name"));
        QString commitHash;
        GitWrapper::commitIfChanged(repoRoot, message, syncRel, &commitHash);
    }

    result.success=true;
    result.publicationDir=syncTargetDir;
    result.publishVersion=newPublishVersion;

    return result;
}


// Рекурсивное копирование каталога целиком
bool BranchPublisher::copyDirectoryRecursively(const QString &sourceDir,
                                               const QString &destDir)
{
    QDir source(sourceDir);
    if(!source.exists())
        return false;

    QDir dest(destDir);
    if(!dest.exists() && !QDir().mkpath(destDir))
        return false;

    const QFileInfoList entries=source.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    for(const QFileInfo &entry : entries)
    {
        const QString destPath=destDir+"/"+entry.fileName();
        if(entry.isDir())
        {
            if(!BranchPublisher::copyDirectoryRecursively(entry.absoluteFilePath(), destPath))
                return false;
        }
        else
        {
            if(QFileInfo::exists(destPath) && !QFile::remove(destPath))
                return false;
            if(!QFile::copy(entry.absoluteFilePath(), destPath))
                return false;
        }
    }

    return true;
}


// Обновление устаревшей per-branch публикации (с .git внутри) на месте
BranchPublisher::Result BranchPublisher::publishLegacyBranch(KnowTreeModel *model,
                                                             TreeItem *startItem,
                                                             TeamProfile &profile,
                                                             Operation operation,
                                                             const QString &publicationDir,
                                                             const QString &ownerDirName,
                                                             const QString &branchDirName,
                                                             Result &result)
{
    Q_UNUSED(ownerDirName)

    const QString targetDir=publicationDir;

    // Временный каталог сборки — в shared/.tmp/ (вне sync/)
    const QString sharedDir=profile.getSharedDir();
    QDir stagingRoot(BranchPublisher::tmpDir(sharedDir));
    if(!stagingRoot.exists() && !stagingRoot.mkpath(stagingRoot.absolutePath()))
    {
        result.errorMessage=QStringLiteral("Cant create temp directory");
        return result;
    }
    QString tempDir=stagingRoot.absolutePath()
                    +"/.tmp-publish-"+branchDirName.section('_',0,0)
                    +"-"+QUuid::createUuid().toString().remove('{').remove('}');
    if(!QDir().mkpath(tempDir))
    {
        result.errorMessage=QStringLiteral("Cant create temp directory: ")+tempDir;
        return result;
    }

    if(!model->exportBranchToDirectory(startItem, tempDir))
    {
        QDir(tempDir).removeRecursively();
        result.errorMessage=QStringLiteral("Export branch to directory failed");
        return result;
    }

    if(!BranchPublisher::buildBranchXmlFromMytetraXml(tempDir))
    {
        QDir(tempDir).removeRecursively();
        result.errorMessage=QStringLiteral("Build branch.xml failed");
        return result;
    }

    if(QFileInfo::exists(tempDir+"/base"))
    {
        if(!QDir(tempDir).rename("base", "records"))
        {
            QDir(tempDir).removeRecursively();
            result.errorMessage=QStringLiteral("Rename base/ to records/ failed");
            return result;
        }
    }
    else if(!QFileInfo::exists(tempDir+"/records"))
    {
        if(!QDir(tempDir).mkpath("records"))
        {
            QDir(tempDir).removeRecursively();
            result.errorMessage=QStringLiteral("Cant create records directory");
            return result;
        }
    }

    int newPublishVersion=(operation==Operation::Update)
                            ? BranchPublisher::readPublishVersion(targetDir)+1
                            : 1;

    const bool encryptedSource=model->isItemContainsCryptBranches(startItem);
    if(!BranchPublisher::writeMetaJson(tempDir, startItem, profile, newPublishVersion,
                                       encryptedSource))
    {
        QDir(tempDir).removeRecursively();
        result.errorMessage=QStringLiteral("Write meta.json failed");
        return result;
    }

    result.largeContent=(BranchPublisher::calculateDirSizeBytes(tempDir+"/records") > 20*1024*1024);

    if(operation==Operation::Update
       && BranchPublisher::isPublicationContentEqual(targetDir, tempDir))
    {
        QDir(tempDir).removeRecursively();
        result.success=true;
        result.unchanged=true;
        result.publicationDir=targetDir;
        result.publishVersion=BranchPublisher::readPublishVersion(targetDir);
        return result;
    }

    if(!BranchPublisher::writePublicationManifest(tempDir, startItem->getField("id"),
                                                   newPublishVersion))
    {
        QDir(tempDir).removeRecursively();
        result.errorMessage=QStringLiteral("Write manifest.json failed");
        return result;
    }

    if(!BranchPublisher::moveTempToFinal(tempDir, targetDir, operation))
    {
        QDir(tempDir).removeRecursively();
        result.errorMessage=QStringLiteral("Move publication to final directory failed");
        return result;
    }

    if(GitWrapper::isGitAvailable())
    {
        const QString commitMessage=(operation==Operation::Update)
                                    ? QStringLiteral("Обновление ветки ")+startItem->getField("name")
                                    : QStringLiteral("Публикация ветки ")+startItem->getField("name");
        GitWrapper::setLocalIdentity(targetDir, profile.getTeamName(), profile.getTeamEmail());
        if(!GitWrapper::commitAll(targetDir, commitMessage))
        {
            result.success=false;
            result.errorMessage=QStringLiteral("git commit failed");
            return result;
        }
    }
    else
        result.journalDisabled=true;

    result.success=true;
    result.publicationDir=targetDir;
    result.publishVersion=newPublishVersion;

    return result;
}


// Отзыв публикации: удаляются в корзину каталоги и в local/, и в sync/
// (парный //.git legacy — целиком). Скрытые элементы переносятся явно
bool BranchPublisher::revokePublication(const QString &publicationDir)
{
    if(publicationDir.isEmpty() || !QFileInfo::exists(publicationDir))
        return false;

    // Каталог публикации содержит скрытый каталог ".git". Обычное удаление
    // в корзину (QDir::entryList скрытые элементы не возвращает) его не
    // захватывало, из-за чего пустой каталог не удалялся и публикация
    // продолжала обнаруживаться (бейдж «рука» не исчезал). Скрытые элементы
    // переносятся в корзину явно
    DiskHelper::removeDirectoryToTrash(publicationDir, true);

    // Парный каталог в соседней зоне (sync/ <-> local/): sync/<owner>/<branch>
    // соответствует local/<owner>/<branch>
    QString counterpart;
    if(publicationDir.contains("/sync/"))
        counterpart=QString(publicationDir).replace("/sync/", "/local/");
    else if(publicationDir.contains("/local/"))
        counterpart=QString(publicationDir).replace("/local/", "/sync/");

    if(!counterpart.isEmpty() && counterpart!=publicationDir
       && QFileInfo::exists(counterpart))
        DiskHelper::removeDirectoryToTrash(counterpart, true);

    return true;
}


// Карта «относительный путь -> sha256» содержимого публикации.
// Служебные файлы (meta.json, .git, временные каталоги) исключаются:
// они не являются данными публикации
QMap<QString, QByteArray> BranchPublisher::publicationContentDigest(const QString &dir)
{
    QMap<QString, QByteArray> digest;

    QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while(it.hasNext())
    {
        it.next();

        QString relative=QDir(dir).relativeFilePath(it.filePath());
        // Пропуск служебного: .git/, meta.json, manifest.json, changelog.json
        // и временных сборок
        if(relative=="meta.json"
           || relative==PUBLICATION_MANIFEST_FILE
           || relative==PUBLICATION_CHANGELOG_FILE
           || relative.startsWith(".git/")
           || relative.startsWith("."))
            continue;

        QFile f(it.filePath());
        if(!f.open(QIODevice::ReadOnly))
            continue;

        digest.insert(relative, QCryptographicHash::hash(f.readAll(),
                                                         QCryptographicHash::Sha256));
        f.close();
    }

    return digest;
}


// Сравнение содержимого двух каталогов публикации (branch.xml + records/)
bool BranchPublisher::isPublicationContentEqual(const QString &dirA, const QString &dirB)
{
    return publicationContentDigest(dirA)==publicationContentDigest(dirB);
}