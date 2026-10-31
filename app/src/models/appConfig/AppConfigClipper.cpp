#include "AppConfigClipper.h"
#include "AppConfig.h"


AppConfigClipper::AppConfigClipper(QObject *parent) : QObject(parent)
{

}


void AppConfigClipper::setBackend(QSettings *conf, AppConfig *host)
{
    m_conf=conf;
    m_host=host;
}


int AppConfigClipper::get_clipperMaxImages(void)
{
    return m_host->get_parameter("clipperMaxImages").toInt();
}


bool AppConfigClipper::set_clipperMaxImages(int count)
{
    if(count>0)
    {
        m_conf->setValue("clipperMaxImages", count);
        return true;
    }
    else
    {
        return false;
    }
}


// Максимальный размер одной картинки клиппера в мегабайтах


// Максимальный размер одной картинки клиппера в мегабайтах
int AppConfigClipper::get_clipperMaxImageSizeMb(void)
{
    return m_host->get_parameter("clipperMaxImageSizeMb").toInt();
}


bool AppConfigClipper::set_clipperMaxImageSizeMb(int mbSize)
{
    if(mbSize>0)
    {
        m_conf->setValue("clipperMaxImageSizeMb", mbSize);
        return true;
    }
    else
    {
        return false;
    }
}


// Веб-клиппер включен или нет


// Веб-клиппер включен или нет
bool AppConfigClipper::get_clipperenable(void)
{
    return m_conf->value("clipperenable").toBool();
}


void AppConfigClipper::set_clipperenable(bool flag)
{
    m_conf->setValue("clipperenable", flag);
}


// Глобальный хоткей клиппера в формате QKeySequence (работает под X11)


// Глобальный хоткей клиппера в формате QKeySequence (работает под X11)
QString AppConfigClipper::get_clipperhotkey(void)
{
    return m_host->get_parameter("clipperhotkey");
}


void AppConfigClipper::set_clipperhotkey(QString sequence)
{
    m_conf->setValue("clipperhotkey", sequence);
}


// Id ветки unsorted_notes (запоминается после создания,
// переживает переименования ветки)


// Id ветки unsorted_notes (запоминается после создания,
// переживает переименования ветки)
QString AppConfigClipper::get_clipperbranchid(void)
{
    return m_host->get_parameter("clipperbranchid");
}


void AppConfigClipper::set_clipperbranchid(QString id)
{
    m_conf->setValue("clipperbranchid", id);
}


// Режим целевой ветки клипа: 0 — по умолчанию, 1 — выбранная,
// 2 — спрашивать каждый раз всплывающим окном


// Режим целевой ветки клипа: 0 — по умолчанию, 1 — выбранная,
// 2 — спрашивать каждый раз всплывающим окном
int AppConfigClipper::get_clipperbranchmode(void)
{
    return m_host->get_parameter("clipperbranchmode").toInt();
}


void AppConfigClipper::set_clipperbranchmode(int mode)
{
    m_conf->setValue("clipperbranchmode", mode);
}


// Частые ветки клипов: пары id:count через ";", частые впереди


// Частые ветки клипов: пары id:count через ";", частые впереди
QString AppConfigClipper::get_clipperrecent(void)
{
    return m_host->get_parameter("clipperrecent");
}


void AppConfigClipper::set_clipperrecent(const QString &ids)
{
    m_conf->setValue("clipperrecent", ids);
}


// Какие 16 плиток внизу опроса: 0 — последние, 1 — частые


// Какие 16 плиток внизу опроса: 0 — последние, 1 — частые
int AppConfigClipper::get_clippertilemode(void)
{
    return m_host->get_parameter("clippertilemode").toInt();
}


void AppConfigClipper::set_clippertilemode(int mode)
{
    m_conf->setValue("clippertilemode", mode);
}


// При попытке вставить дубль открывать оригинал заметки


// При попытке вставить дубль открывать оригинал заметки
bool AppConfigClipper::get_clipperopendup(void)
{
    return m_conf->value("clipperopendup", true).toBool();
}


void AppConfigClipper::set_clipperopendup(bool open)
{
    m_conf->setValue("clipperopendup", open);
}


// Разрешена ли периодическая проверка файла базы на предмет изменения сторонней программой
