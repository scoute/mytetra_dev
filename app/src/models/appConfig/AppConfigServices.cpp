#include "AppConfigServices.h"
#include "AppConfig.h"
#include <QtGlobal>


AppConfigServices::AppConfigServices(QObject *parent) : QObject(parent)
{

}


void AppConfigServices::setBackend(QSettings *conf, AppConfig *host)
{
    m_conf=conf;
    m_host=host;
}


// Разрешен ли доступ внешнего AI-клиента через MCP.
// По умолчанию запрещен: включается галочкой на странице MCP
bool AppConfigServices::get_mcpAllow(void)
{
    return m_conf->value("mcpAllow", false).toBool();
}


void AppConfigServices::set_mcpAllow(bool state)
{
    m_conf->setValue("mcpAllow", state);
}


// Игнорировать ошибки проверки TLS при сетевой загрузке.
// По умолчанию false: самоподписанный/просроченный сертификат
// отклоняется, иначе MITM может подменить картинку или файл


// Игнорировать ошибки проверки TLS при сетевой загрузке.
// По умолчанию false: самоподписанный/просроченный сертификат
// отклоняется, иначе MITM может подменить картинку или файл
bool AppConfigServices::get_tlsIgnoreInvalidCertificates(void)
{
    return m_conf->value("tlsIgnoreInvalidCertificates", false).toBool();
}


void AppConfigServices::set_tlsIgnoreInvalidCertificates(bool state)
{
    m_conf->setValue("tlsIgnoreInvalidCertificates", state);
}


// Показывать ли общий итог записей с учетом подветок.
// По умолчанию выключено: только свои [n]


// Раскладка импорта Telegram. Неизвестное значение лечится
// в дефолт: импорт всегда рабочий
QString AppConfigServices::get_telegramImportMode(void)
{
    const QString mode=m_conf->value("telegramImportMode", QStringLiteral("month")).toString();

    if(mode==QStringLiteral("message") || mode==QStringLiteral("day") ||
       mode==QStringLiteral("year"))
        return mode;

    return QStringLiteral("month");
}


void AppConfigServices::set_telegramImportMode(const QString &mode)
{
    if(mode==QStringLiteral("message") || mode==QStringLiteral("day") ||
       mode==QStringLiteral("year"))
        m_conf->setValue("telegramImportMode", mode);
    else
        m_conf->setValue("telegramImportMode", QStringLiteral("month"));
}


// Метод шифрования. rc5 если не задано или лежит мусор:
// неизвестное значение никогда не должно молча ронять данные


// Формат голосовых заметок по умолчанию. Opus если не задано
// или лежит мусор: диалог записи все равно проверяет поддержку
QString AppConfigServices::get_voiceNoteFormat(void)
{
    const QString format=m_conf->value("voiceNoteFormat", "opus").toString();

    if(format==QStringLiteral("mp3") || format==QStringLiteral("opus"))
        return format;

    return QStringLiteral("opus");
}


void AppConfigServices::set_voiceNoteFormat(QString format)
{
    if(format!=QStringLiteral("mp3") && format!=QStringLiteral("opus"))
        format=QStringLiteral("opus");

    m_conf->setValue("voiceNoteFormat", format);
}


// Подавление фонового шума (HP+вырезки гула+гейт) в диалоге записи
// голосовых заметок. Включено по умолчанию


// Подавление фонового шума (HP+вырезки гула+гейт) в диалоге записи
// голосовых заметок. Включено по умолчанию
bool AppConfigServices::get_voiceNoteNoiseSuppress(void)
{
    return m_conf->value("voiceNoteNoiseSuppress", true).toBool();
}


void AppConfigServices::set_voiceNoteNoiseSuppress(bool value)
{
    m_conf->setValue("voiceNoteNoiseSuppress", value);
}


// Качество Opus голосовых заметок по умолчанию: 0 super low (12 кбит),
// 1 extra low (16 кбит), 2 low (24 кбит), 3 middle (32 кбит, дефолт),
// 4 high (48 кбит), 5 extra high (64 кбит).


// Качество Opus голосовых заметок по умолчанию: 0 super low (12 кбит),
// 1 extra low (16 кбит), 2 low (24 кбит), 3 middle (32 кбит, дефолт),
// 4 high (48 кбит), 5 extra high (64 кбит).
int AppConfigServices::get_voiceNoteQuality(void)
{
    const int quality=m_conf->value("voiceNoteQuality", 3).toInt();

    return qBound(0, quality, 5);
}


void AppConfigServices::set_voiceNoteQuality(int quality)
{
    m_conf->setValue("voiceNoteQuality", qBound(0, quality, 5));
}
