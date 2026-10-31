#include "AppConfigRecordTable.h"
#include "AppConfig.h"
#include "libraries/FixedParameters.h"


AppConfigRecordTable::AppConfigRecordTable(QObject *parent) : QObject(parent)
{

}


void AppConfigRecordTable::setBackend(QSettings *conf, AppConfig *host)
{
    m_conf=conf;
    m_host=host;
}


// ID записи в таблице конечных записей, которая выделена на экране
QString AppConfigRecordTable::get_recordtable_selected_record_id(void)
{
    return m_conf->value("recordtableSelectedRecordId", 0).toString();
}


// ID записи в таблице конечных записей, которая выделена на экране


// ID записи в таблице конечных записей, которая выделена на экране
void AppConfigRecordTable::set_recordtable_selected_record_id(QString id)
{
    m_conf->setValue("recordtableSelectedRecordId", id);
}


// Перечень полей, отображаемых в таблице конечных записей.
// Служебные поля без подписи (favorite) из старых конфигов
// отрезаются здесь: колонками они не являются, пустой столбец
// с пустым заголовком не нужен. При следующем apply список
// перезапишется уже чистым
QStringList AppConfigRecordTable::getRecordTableShowFields(void)
{
    QStringList fields=(m_conf->value("recordTableShowFields", "name")).toString().split(",");
    QStringList columns;

    foreach(const QString &field, fields)
      if(FixedParameters::isRecordFieldColumn(field))
        columns << field;

    if(columns.isEmpty())
      columns << "name";

    return columns;
}


void AppConfigRecordTable::setRecordTableShowFields(QStringList fields)
{
    // Не должно быть так, чтобы был пустой список
    if (fields.size()==0)
    {
        fields << "name";
    }

    m_conf->setValue("recordTableShowFields", fields.join(","));
}


bool AppConfigRecordTable::getRecordTableShowHorizontalHeaders(void)
{
    return m_conf->value("recordTableShowHorizontalHeaders").toBool();
}


void AppConfigRecordTable::setRecordTableShowHorizontalHeaders(bool flag)
{
    m_conf->setValue("recordTableShowHorizontalHeaders", flag);
}


bool AppConfigRecordTable::getRecordTableShowVerticalHeaders(void)
{
    return m_conf->value("recordTableShowVerticalHeaders").toBool();
}


void AppConfigRecordTable::setRecordTableShowVerticalHeaders(bool flag)
{
    m_conf->setValue("recordTableShowVerticalHeaders", flag);
}


// Ширина полей, отображаемых в таблице конечных записей


// Ширина полей, отображаемых в таблице конечных записей
QStringList AppConfigRecordTable::getRecordTableFieldsWidth(void)
{
    return (m_conf->value("recordTableFieldsWidth", "256")).toString().split(",");
}


void AppConfigRecordTable::setRecordTableFieldsWidth(QStringList fields)
{
    // В списке с одним элементом должна стоять стандартная величина (всеравно она не влияет, и начнет влиять только после появления второй колонки)
    if (fields.size()==1)
    {
        fields[0]="256";
    }

    // Не должно быть так, чтобы был пустой список
    if (fields.size()==0)
    {
        fields << "256";
    }

    m_conf->setValue("recordTableFieldsWidth", fields.join(","));
}


// Показывать ли сплешскрин при старте программы


bool AppConfigRecordTable::get_readOnly(void)
{
    return m_conf->value("readOnly", false).toBool() || m_host->isMcpUiLocked();
}


bool AppConfigRecordTable::isGlobalReadOnly(void)
{
    return m_conf->value("readOnly", false).toBool();
}


void AppConfigRecordTable::set_readOnly(bool state)
{
    m_conf->setValue("readOnly", state);
}


// Разрешено ли создавать запись без текста
bool AppConfigRecordTable::getEnableCreateEmptyRecord(void)
{
    return m_conf->value("enableCreateEmptyRecord").toBool();
}


void AppConfigRecordTable::setEnableCreateEmptyRecord(bool state)
{
    m_conf->setValue("enableCreateEmptyRecord", state);
}


// Разрешен ли доступ внешнего AI-клиента через MCP.
// По умолчанию запрещен: включается галочкой на странице MCP
