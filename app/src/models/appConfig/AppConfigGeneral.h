#ifndef __APPCONFIGGENERAL_H__
#define __APPCONFIGGENERAL_H__

#include <QObject>
#include <QSettings>
#include <QString>

class AppConfig;

// P2: общие настройки без своей группы: расширяемый префикс,
// язык, версия формата. Хост - AppConfig, бэкенд общий QSettings
class AppConfigGeneral : public QObject
{
    Q_OBJECT

public:

    explicit AppConfigGeneral(QObject *parent=nullptr);
    void setBackend(QSettings *conf, AppConfig *host);

    // Номер последнего префикса в виде строки с ведущими нулями
    QString get_addnewrecord_expand_info(void);
    void set_addnewrecord_expand_info(QString state);

    // Нужно ли печатать отладочные сообщения в консоль
    bool get_printdebugmessages(void);
    bool set_printdebugmessages(bool isPrint);

    // Язык интерфейса
    QString get_interfacelanguage(void);
    bool set_interfacelanguage(QString language);

    // Версия формата конфигфайла
    int get_config_version(void);
    void set_config_version(int i);

private:

    QSettings *m_conf=nullptr;
    AppConfig *m_host=nullptr;
};

#endif // __APPCONFIGGENERAL_H__
