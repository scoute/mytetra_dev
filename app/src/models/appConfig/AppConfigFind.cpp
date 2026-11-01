#include "AppConfigFind.h"


AppConfigFind::AppConfigFind(QObject *parent) : QObject(parent)
{

}


void AppConfigFind::setBackend(QSettings *conf, AppConfig *host)
{
    m_conf=conf;
    m_host=host;
}


int AppConfigFind::get_findscreen_wordregard(void)
{
    return m_conf->value("findscreen_wordregard", 0).toInt();
}


void AppConfigFind::set_findscreen_wordregard(int pos)
{
    m_conf->setValue("findscreen_wordregard",pos);
}


int AppConfigFind::get_findscreen_howextract(void)
{
    return m_conf->value("findscreen_howextract", 0).toInt();
}


void AppConfigFind::set_findscreen_howextract(int pos)
{
    m_conf->setValue("findscreen_howextract", pos);
}


int AppConfigFind::getFindScreenTreeSearchArea(void)
{
    return m_conf->value("findScreenTreeSearchArea", 0).toInt();
}


void AppConfigFind::setFindScreenTreeSearchArea(int pos)
{
    m_conf->setValue("findScreenTreeSearchArea", pos);
}


bool AppConfigFind::get_findscreen_find_in_field(QString fieldName)
{
    // Дефолт для отсутствующего ключа: искать везде, кроме текста записи.
    // Поиск по тексту медленный и шумит, остальные поля быстрые
    // и нужны для полноты (в том числе автокомплита)
    bool defaultValue=(fieldName!="text");

    return m_conf->value("findscreen_find_in"+fieldName, defaultValue).toBool();
}


void AppConfigFind::set_findscreen_find_in_field(QString fieldName, bool isChecked)
{
    m_conf->setValue("findscreen_find_in"+fieldName, isChecked);
}


bool AppConfigFind::get_findscreen_show(void)
{
    return m_conf->value("findscreen_show", 0).toBool();
}


void AppConfigFind::set_findscreen_show(bool isShow)
{
    m_conf->setValue("findscreen_show", isShow);
}
