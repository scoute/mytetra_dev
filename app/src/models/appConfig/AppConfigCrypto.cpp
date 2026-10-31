#include "AppConfigCrypto.h"
#include "AppConfig.h"
#include "libraries/helpers/DebugHelper.h"


AppConfigCrypto::AppConfigCrypto(QObject *parent) : QObject(parent)
{

}


void AppConfigCrypto::setBackend(QSettings *conf, AppConfig *host)
{
    m_conf=conf;
    m_host=host;
}


QString AppConfigCrypto::get_howpassrequest(void)
{
    return m_host->get_parameter("howpassrequest");
}


void AppConfigCrypto::set_howpassrequest(QString mode)
{
    if (mode=="atClickOnCryptBranch" || mode=="atStartProgram")
    {
        m_conf->setValue("howpassrequest", mode);
    }
    else
    {
        criticalError("Set unavailable value for howpassrequest "+mode);
    }
}


bool AppConfigCrypto::get_autoClosePasswordEnable(void)
{
    return m_conf->value("autoClosePasswordEnable", 0).toBool();
}


void AppConfigCrypto::set_autoClosePasswordEnable(bool flag)
{
    m_conf->setValue("autoClosePasswordEnable", flag);
}


int AppConfigCrypto::get_autoClosePasswordDelay(void)
{
    return m_conf->value("autoClosePasswordDelay", 0).toInt();
}


void AppConfigCrypto::set_autoClosePasswordDelay(int delay)
{
    m_conf->setValue("autoClosePasswordDelay", delay);
}


QString AppConfigCrypto::getPasswordMiddleHash(void)
{
    return m_host->get_parameter("passwordMiddleHash");
}


void AppConfigCrypto::setPasswordMiddleHash(QString hash)
{
    m_conf->setValue("passwordMiddleHash", hash);
}


// Нужно ли локально хранить пароль (точнее, промежуточный хеш пароля)


// Нужно ли локально хранить пароль (точнее, промежуточный хеш пароля)
bool AppConfigCrypto::getPasswordSaveFlag(void)
{
    return m_conf->value("passwordSaveFlag").toBool();
}


void AppConfigCrypto::setPasswordSaveFlag(bool flag)
{
    m_conf->setValue("passwordSaveFlag", flag);
}


bool AppConfigCrypto::getRememberCursorAtHistoryNavigation(void)
{
    return m_conf->value("rememberCursorAtHistoryNavigation").toBool();
}


void AppConfigCrypto::setRememberCursorAtHistoryNavigation(bool flag)
{
    m_conf->setValue("rememberCursorAtHistoryNavigation", flag);
}


bool AppConfigCrypto::getRememberCursorAtOrdinarySelection(void)
{
    return m_conf->value("rememberCursorAtOrdinarySelection").toBool();
}


void AppConfigCrypto::setRememberCursorAtOrdinarySelection(bool flag)
{
    m_conf->setValue("rememberCursorAtOrdinarySelection", flag);
}


// Метод шифрования. rc5 если не задано или лежит мусор:
// неизвестное значение никогда не должно молча ронять данные
QString AppConfigCrypto::get_cryptMethod(void)
{
    const QString method=m_conf->value("cryptMethod", QStringLiteral("rc5")).toString();

    if(method==QStringLiteral("xchacha"))
        return method;

    return QStringLiteral("rc5");
}


void AppConfigCrypto::set_cryptMethod(const QString &method)
{
    if(method==QStringLiteral("xchacha"))
        m_conf->setValue("cryptMethod", method);
    else
        m_conf->setValue("cryptMethod", QStringLiteral("rc5"));
}


// Формат голосовых заметок по умолчанию. Opus если не задано
// или лежит мусор: диалог записи все равно проверяет поддержку
