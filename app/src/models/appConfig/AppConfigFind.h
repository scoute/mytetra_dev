#ifndef __APPCONFIGFIND_H__
#define __APPCONFIGFIND_H__

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

class AppConfigFind : public QObject
{
    Q_OBJECT

public:

    explicit AppConfigFind(QObject *parent=nullptr);
    void setBackend(QSettings *conf, AppConfig *host);

    // Настройки виджета поиска по базе
    int get_findscreen_wordregard(void);
    void set_findscreen_wordregard(int pos);
    int get_findscreen_howextract(void);
    void set_findscreen_howextract(int pos);
    int getFindScreenTreeSearchArea(void);
    void setFindScreenTreeSearchArea(int pos);
    bool get_findscreen_find_in_field(QString fieldName);
    void set_findscreen_find_in_field(QString fieldName, bool isChecked);
    bool get_findscreen_show(void);
    void set_findscreen_show(bool isShow);

private:

    QSettings *m_conf=nullptr;
    AppConfig *m_host=nullptr;
};

#endif // __APPCONFIGFIND_H__
