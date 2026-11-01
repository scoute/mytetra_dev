#include <QDebug>

#include "AppConfigGeneral.h"
#include "AppConfig.h"
#include "libraries/helpers/DebugHelper.h"


AppConfigGeneral::AppConfigGeneral(QObject *parent) : QObject(parent)
{

}


void AppConfigGeneral::setBackend(QSettings *conf, AppConfig *host)
{
    m_conf=conf;
    m_host=host;
}


QString AppConfigGeneral::get_addnewrecord_expand_info(void)
{
    return m_host->get_parameter("addnewrecord_expand_info");
}


void AppConfigGeneral::set_addnewrecord_expand_info(QString state)
{
    if (state=="0" || state=="1")
    {
        m_conf->setValue("addnewrecord_expand_info", state);
    }
    else
    {
        criticalError("Set unavailable value for addnewrecord_expand_info "+state);
    }
}


bool AppConfigGeneral::get_printdebugmessages(void)
{
    return m_conf->value("printdebugmessages").toBool();
}


bool AppConfigGeneral::set_printdebugmessages(bool isPrint)
{
    m_conf->setValue("printdebugmessages", isPrint);
    return true;
}


QString AppConfigGeneral::get_interfacelanguage(void)
{
    QString language = m_host->get_parameter("interfacelanguage");
    qDebug() << "Get interface language" << language;
    return language;
}


bool AppConfigGeneral::set_interfacelanguage(QString language)
{
    m_conf->setValue("interfacelanguage", language);
    return true;
}


int AppConfigGeneral::get_config_version(void)
{
    int v = m_conf->contains("version") ? m_conf->value("version").toInt() : 0;

    return v;
}


void AppConfigGeneral::set_config_version(int i)
{
    m_conf->setValue("version", i);
}
