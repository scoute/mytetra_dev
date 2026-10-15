#include <QFileInfo>
#include <QCoreApplication>

#include "TeamProfile.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/UniqueIdHelper.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;


TeamProfile::TeamProfile(QObject *pobj) : QObject(pobj)
{

}


TeamProfile::~TeamProfile()
{

}


QString TeamProfile::getTeamName(void)
{
    return mytetraConfig.get_teamName();
}


void TeamProfile::setTeamName(const QString &name)
{
    mytetraConfig.set_teamName(name);
    mytetraConfig.sync();
}


QString TeamProfile::getTeamEmail(void)
{
    return mytetraConfig.get_teamEmail();
}


void TeamProfile::setTeamEmail(const QString &email)
{
    mytetraConfig.set_teamEmail(email);
    mytetraConfig.sync();
}


QString TeamProfile::getTeamId(void)
{
    QString id=mytetraConfig.get_teamId();

    if(id.isEmpty())
    {
        id=QStringLiteral("U")+getUniqueId();
        mytetraConfig.set_teamId(id);
        mytetraConfig.sync();
    }

    return id;
}


bool TeamProfile::isTeamIdSet(void)
{
    return !mytetraConfig.get_teamId().isEmpty();
}


bool TeamProfile::isConfigured(void)
{
    return !this->getTeamName().isEmpty();
}


void TeamProfile::generateNewTeamId(void)
{
    mytetraConfig.set_teamId(QStringLiteral("U")+getUniqueId());
    mytetraConfig.sync();
}


QString TeamProfile::getSharedDirConfigValue(void)
{
    return mytetraConfig.get_sharedDir();
}


QString TeamProfile::getSharedDir(void)
{
    const QString configured=mytetraConfig.get_sharedDir();

    if(!configured.isEmpty())
        return configured;

    // Каталог по умолчанию рядом с бинарником. Если не писабелен
    // (установка в /usr/local/bin) - фоллбэк в рабочий каталог
    const QString binaryDir=QCoreApplication::applicationDirPath();

    if(QFileInfo(binaryDir).isWritable())
        return binaryDir+QStringLiteral("/shared");

    return globalParameters.getWorkDirectory()+QStringLiteral("/shared");
}


void TeamProfile::setSharedDir(const QString &path)
{
    mytetraConfig.set_sharedDir(path);
    mytetraConfig.sync();
}
