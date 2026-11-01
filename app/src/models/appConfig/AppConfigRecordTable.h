#ifndef __APPCONFIGRECORDTABLE_H__
#define __APPCONFIGRECORDTABLE_H__

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QList>
#include <QByteArray>

class AppConfig;

// P2: секция конфига. Владелец - AppConfig, бэкенд QSettings
// общий (перепривязывается при каждом обращении фасада,
// поэтому работает и в пробах с прямой установкой m_conf)

class AppConfigRecordTable : public QObject
{
    Q_OBJECT

public:

    explicit AppConfigRecordTable(QObject *parent=nullptr);
    void setBackend(QSettings *conf, AppConfig *host);

    // ID записи в таблице конечных записей, которая выделена на экране
    QString get_recordtable_selected_record_id(void);
    void set_recordtable_selected_record_id(QString id);
    // Перечень полей, отображаемых в таблице конечных записей
    QStringList getRecordTableShowFields(void);
    void setRecordTableShowFields(QStringList fields);
    // Нужно ли отображать горизонтальные заголовки в таблице конечных записей
    bool getRecordTableShowHorizontalHeaders(void);
    void setRecordTableShowHorizontalHeaders(bool flag);
    // Нужно ли отображать вертикальные заголовки в таблице конечных записей
    bool getRecordTableShowVerticalHeaders(void);
    void setRecordTableShowVerticalHeaders(bool flag);
    // Ширина полей, отображаемых в таблице конечных записей
    QStringList getRecordTableFieldsWidth(void);
    void setRecordTableFieldsWidth(QStringList fieldsWidth);
    // Глобальный режим только чтения: запрещены любые изменения базы,
    // разрешены навигация и просмотр. По умолчанию выключен
    bool get_readOnly(void);
    // Сырой глобальный флаг без учёта сессии. Им смотрят только
    // MCP-записи: они продолжают писать при заблокированном UI
    bool isGlobalReadOnly(void);
    void set_readOnly(bool state);
    // Разрешено ли создавать запись без текста
    bool getEnableCreateEmptyRecord(void);
    void setEnableCreateEmptyRecord(bool state);

private:

    QSettings *m_conf=nullptr;
    AppConfig *m_host=nullptr;
};

#endif // __APPCONFIGRECORDTABLE_H__
