#include "AppConfigSync.h"
#include "AppConfig.h"


AppConfigSync::AppConfigSync(QObject *parent) : QObject(parent)
{

}


void AppConfigSync::setBackend(QSettings *conf, AppConfig *host)
{
    m_conf=conf;
    m_host=host;
}


bool AppConfigSync::get_autosaveEnable(void)
{
    return m_conf->value("autosaveEnable", true).toBool();
}


void AppConfigSync::set_autosaveEnable(bool flag)
{
    m_conf->setValue("autosaveEnable", flag);
}


int AppConfigSync::get_autosaveInterval(void)
{
    return m_conf->value("autosaveInterval", 10).toInt();
}


void AppConfigSync::set_autosaveInterval(int minutes)
{
    m_conf->setValue("autosaveInterval", minutes);
}


bool AppConfigSync::get_historySquashEnable(void)
{
    return m_conf->value("historySquashEnable", true).toBool();
}


void AppConfigSync::set_historySquashEnable(bool flag)
{
    m_conf->setValue("historySquashEnable", flag);
}


int AppConfigSync::get_historySquashKeep(void)
{
    return m_conf->value("historySquashKeep", 100).toInt();
}


void AppConfigSync::set_historySquashKeep(int count)
{
    m_conf->setValue("historySquashKeep", count);
}


int AppConfigSync::get_historySquashMode(void)
{
    return m_conf->value("historySquashMode", 0).toInt();
}


void AppConfigSync::set_historySquashMode(int mode)
{
    m_conf->setValue("historySquashMode", mode);
}


bool AppConfigSync::get_historyEnable(void)
{
    return m_conf->value("historyEnable", false).toBool();
}


void AppConfigSync::set_historyEnable(bool flag)
{
    m_conf->setValue("historyEnable", flag);
}


QString AppConfigSync::get_synchrocommand(void)
{
    return m_host->get_parameter("synchrocommand");
}


void AppConfigSync::set_synchrocommand(QString command)
{
    m_conf->setValue("synchrocommand", command);
}


bool AppConfigSync::get_synchroonstartup(void)
{
    return m_conf->value("synchroonstartup").toBool();
}


void AppConfigSync::set_synchroonstartup(bool flag)
{
    m_conf->setValue("synchroonstartup", flag);
}


bool AppConfigSync::get_synchroonexit(void)
{
    return m_conf->value("synchroonexit").toBool();
}


void AppConfigSync::set_synchroonexit(bool flag)
{
    m_conf->setValue("synchroonexit", flag);
}


bool AppConfigSync::getSyncroConsoleDetails(void)
{
    return m_conf->value("syncroConsoleDetails").toBool();
}


void AppConfigSync::setSyncroConsoleDetails(bool flag)
{
    m_conf->setValue("syncroConsoleDetails", flag);
}


// Разрешена ли периодическая проверка файла базы на предмет изменения сторонней программой
bool AppConfigSync::getEnablePeriodicCheckBase(void)
{
    return m_conf->value("enablePeriodicCheckBase").toBool();
}


void AppConfigSync::setEnablePeriodicCheckBase(bool state)
{
    m_conf->setValue("enablePeriodicCheckBase", state);
}


// Период проверки файла базы на предмет изменения сторонней программой


// Период проверки файла базы на предмет изменения сторонней программой
int AppConfigSync::getCheckBasePeriod(void)
{
    return m_host->get_parameter("checkBasePeriod").toInt();
}


void AppConfigSync::setCheckBasePeriod(int period)
{
    m_conf->setValue("checkBasePeriod", period);
}


// Необходимо ли выводить сообщение если база была изменена другой программой


// Необходимо ли выводить сообщение если база была изменена другой программой
bool AppConfigSync::getEnablePeriodicCheckMessage(void)
{
    return m_conf->value("enablePeriodicCheckMessage").toBool();
}


void AppConfigSync::setEnablePeriodicCheckMessage(bool state)
{
    m_conf->setValue("enablePeriodicCheckMessage", state);
}


// Разрешена ли периодическая синхронизация
bool AppConfigSync::getEnablePeriodicSyncro(void)
{
    return m_conf->value("enablePeriodicSyncro").toBool();
}


void AppConfigSync::setEnablePeriodicSyncro(bool state)
{
    m_conf->setValue("enablePeriodicSyncro", state);
}


// Период автоматической периодической синхронизации


// Период автоматической периодической синхронизации
int AppConfigSync::getPeriodicSyncroPeriod(void)
{
    return m_host->get_parameter("periodicSyncroPeriod").toInt();
}


void AppConfigSync::setPeriodicSyncroPeriod(int period)
{
    m_conf->setValue("periodicSyncroPeriod", period);
}


// Разрешено ли создавать запись без текста
