#include "TeamProfile.h"

#include <QCoreApplication>
#include <QFileInfo>

#include "models/appConfig/AppConfig.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "libraries/GlobalParameters.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;


TeamProfile::TeamProfile(QObject *pobj) : QObject(pobj)
{

}


TeamProfile::~TeamProfile()
{

}


// Получение имени пользователя в команде
QString TeamProfile::getTeamName(void)
{
    return mytetraConfig.get_teamname();
}


// Установка имени пользователя в команде
void TeamProfile::setTeamName(const QString &name)
{
    if(mytetraConfig.get_teamname()==name)
    {
        return;
    }

    mytetraConfig.set_teamname(name);
    mytetraConfig.sync();

    emit teamProfileChanged();
}


// Получение электронной почты пользователя в команде
QString TeamProfile::getTeamEmail(void)
{
    return mytetraConfig.get_teamemail();
}


// Установка электронной почты пользователя в команде
void TeamProfile::setTeamEmail(const QString &email)
{
    if(mytetraConfig.get_teamemail()==email)
    {
        return;
    }

    mytetraConfig.set_teamemail(email);
    mytetraConfig.sync();

    emit teamProfileChanged();
}


// Получение идентификатора пользователя в команде.
// Если идентификатор в конфиге пустой, он создается автоматически
QString TeamProfile::getTeamId(void)
{
    QString id=mytetraConfig.get_teamid();

    if(id.isEmpty())
    {
        id=this->createNewTeamId();
        mytetraConfig.set_teamid(id);
        mytetraConfig.sync();
    }

    return id;
}


// Признак, что идентификатор пользователя уже задан в конфиге
bool TeamProfile::isTeamIdSet(void)
{
    return !mytetraConfig.get_teamid().isEmpty();
}


// Принудительная генерация нового идентификатора пользователя
void TeamProfile::generateNewTeamId(void)
{
    mytetraConfig.set_teamid(this->createNewTeamId());
    mytetraConfig.sync();
}


// Значение каталога обмена из конфига (может быть пустым)
QString TeamProfile::getSharedDirConfigValue(void)
{
    return mytetraConfig.get_shareddir();
}


// Фактический каталог обмена
QString TeamProfile::getSharedDir(void)
{
    QString configured=mytetraConfig.get_shareddir();
    if(!configured.isEmpty())
    {
        return configured;
    }

    // Каталог по умолчанию — рядом с бинарником
    QString binaryDir=QCoreApplication::applicationDirPath();
    QString defaultPath=binaryDir+"/shared";

    // Если каталог бинарника недоступен для записи —
    // фоллбэк в рабочий каталог с уведомлением
    if(QFileInfo(binaryDir).isWritable())
    {
        return defaultPath;
    }

    return globalParameters.getWorkDirectory()+"/shared";
}


// Установка каталога обмена.
// Пустое значение означает каталог по умолчанию
void TeamProfile::setSharedDir(const QString &path)
{
    QString trimmed=path.trimmed();

    if(mytetraConfig.get_shareddir()==trimmed)
    {
        return;
    }

    mytetraConfig.set_shareddir(trimmed);
    mytetraConfig.sync();

    emit teamProfileChanged();
}


// Полная метка владельца для отображения
QString TeamProfile::getOwnerLabel(void)
{
    QString name=this->getTeamName();
    QString email=this->getTeamEmail();

    if(name.isEmpty())
    {
        return email;
    }

    if(email.isEmpty())
    {
        return name;
    }

    return name+" <"+email+">";
}


// Признак, что профиль настроен
bool TeamProfile::isConfigured(void)
{
    return !this->getTeamName().isEmpty();
}


// Создание нового идентификатора пользователя.
// Префикс "U" отделяет пространство идентификаторов пользователей
// от пространства идентификаторов веток и записей
QString TeamProfile::createNewTeamId(void)
{
    return "U"+getUniqueId();
}