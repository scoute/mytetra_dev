#include "AppConfigStorage.h"
#include "AppConfig.h"
#include <QDir>


AppConfigStorage::AppConfigStorage(QObject *parent) : QObject(parent)
{

}


void AppConfigStorage::setBackend(QSettings *conf, AppConfig *host)
{
    m_conf=conf;
    m_host=host;
}


// Установка имени директории с данными (в которой находится mytetra.xml)
bool AppConfigStorage::set_tetradir(QString dirName)
{
    QDir directory(dirName);

    if (directory.exists() && directory.isReadable())
    {
        m_conf->setValue("tetradir", dirName);
        return true;
    }
    else
    {
        return false;
    }
}


// Получение имени директории с данными (в которой находится mytetra.xml)


// Получение имени директории с данными (в которой находится mytetra.xml)
QString AppConfigStorage::get_tetradir(void)
{
    return m_host->get_parameter("tetradir");
}


// Установка имени директории с корзиной


// Установка имени директории с корзиной
bool AppConfigStorage::set_trashdir(QString dirName)
{
    QDir directory(dirName);

    if (directory.exists() && directory.isReadable())
    {
        m_conf->setValue("trashdir", dirName);
        return true;
    }
    else
    {
        return false;
    }
}


// Получение имени директории с корзиной


// Получение имени директории с корзиной
QString AppConfigStorage::get_trashdir(void)
{
    return m_host->get_parameter("trashdir");
}


// Получение максимального размера директории корзины в мегабайтах


// Получение максимального размера директории корзины в мегабайтах
unsigned int AppConfigStorage::get_trashsize(void)
{
    return m_host->get_parameter("trashsize").toUInt();
}


// Установка максимального размера директории корзины в мегабайтах


// Установка максимального размера директории корзины в мегабайтах
bool AppConfigStorage::set_trashsize(unsigned int mbSize)
{
    if (mbSize>0)
    {
        m_conf->setValue("trashsize", mbSize);
        return true;
    }
    else
    {
        return false;
    }
}


// Получение максимально допустимого числа файлов в корзине


// Получение максимально допустимого числа файлов в корзине
int AppConfigStorage::get_trashmaxfilecount(void)
{
    return m_host->get_parameter("trashmaxfilecount").toInt();
}


// Установка максимально допустимого числа файлов в корзине


// Установка максимально допустимого числа файлов в корзине
bool AppConfigStorage::set_trashmaxfilecount(int count)
{
    if (count>0)
    {
        m_conf->setValue("trashmaxfilecount", count);
        return true;
    }
    else
    {
        return false;
    }
}


// Путь на диске по которому пользователь открывал файлы чтобы приаттачить их к записи
QString AppConfigStorage::getAttachAppendDir(void)
{
    return m_host->get_parameter("attachAppendDir");
}


void AppConfigStorage::setAttachAppendDir(QString dir)
{
    m_conf->setValue("attachAppendDir", dir);
}


// Путь на диске по которому пользователь сохранял (Сохранить как...)приаттаченные файлы


// Путь на диске по которому пользователь сохранял (Сохранить как...)приаттаченные файлы
QString AppConfigStorage::getAttachSaveAsDir(void)
{
    return m_host->get_parameter("attachSaveAsDir");
}


void AppConfigStorage::setAttachSaveAsDir(QString dir)
{
    m_conf->setValue("attachSaveAsDir", dir);
}


// Разрешать ли для просмотра расшифровывать зашифрованные файлы в директорию корзины MyTetra


// Разрешать ли для просмотра расшифровывать зашифрованные файлы в директорию корзины MyTetra
bool AppConfigStorage::getEnableDecryptFileToTrashDirectory(void)
{
    return m_conf->value("enableDecryptFileToTrashDirectory").toBool();
}


void AppConfigStorage::setEnableDecryptFileToTrashDirectory(bool state)
{
    m_conf->setValue("enableDecryptFileToTrashDirectory", state);
}


// Получение размера файла лога действий


// Получение размера файла лога действий
unsigned int AppConfigStorage::getActionLogMaximumSize(void)
{
    return m_host->get_parameter("actionLogMaximumSize").toUInt();
}


// Установка размера файла лога действий в мегабайтах


// Установка размера файла лога действий в мегабайтах
void AppConfigStorage::setActionLogMaximumSize(unsigned int mbSize)
{
    m_conf->setValue("actionLogMaximumSize", mbSize);
}


// Разрешено ли логирование


// Разрешено ли логирование
bool AppConfigStorage::getEnableLogging(void)
{
    return m_conf->value("enableLogging").toBool();
}


void AppConfigStorage::setEnableLogging(bool state)
{
    m_conf->setValue("enableLogging", state);
}

// Разрешена ли подсветка записей с прикрепленными файлами


// Человеко-читаемые имена вложенных файлов на диске.
// Параметр намеренно не входит в версионные таблицы (как noteFontColor):
// отсутствующий ключ покрывается значением по умолчанию, миграция conf не нужна
bool AppConfigStorage::get_attachHumanReadableNames(void)
{
    return get_attachNameMode()==AppConfig::AttachNameHybrid;
}


void AppConfigStorage::set_attachHumanReadableNames(bool state)
{
    set_attachNameMode(state ? AppConfig::AttachNameHybrid : AppConfig::AttachNameSystem);
}


// Режим имен вложенных файлов на диске.
// Параметр намеренно не входит в версионные таблицы (как noteFontColor):
// отсутствующий ключ покрывается значением по умолчанию, миграция conf не нужна


// Режим имен вложенных файлов на диске.
// Параметр намеренно не входит в версионные таблицы (как noteFontColor):
// отсутствующий ключ покрывается значением по умолчанию, миграция conf не нужна
int AppConfigStorage::get_attachNameMode(void)
{
    if(m_conf->contains("attachNameMode"))
        return m_conf->value("attachNameMode", AppConfig::AttachNameSystem).toInt();

    // Совместимость с версией 1.69: там был только флаг гибридного режима
    if(m_conf->value("attachHumanReadableNames", false).toBool())
        return AppConfig::AttachNameHybrid;

    // По умолчанию системные имена (id.extension): короткие ASCII-имена
    // без пробелов и национальных символов - надежнее для синхронизации,
    // бэкапов и внешних инструментов. Явный выбор в настройках
    // уважавается, старые файлы при любом режиме находятся поиском
    // по всем схемам (candidateFileNames)
    return AppConfig::AttachNameSystem;
}


void AppConfigStorage::set_attachNameMode(int mode)
{
    if(mode<AppConfig::AttachNameSystem || mode>AppConfig::AttachNameOriginal)
        mode=AppConfig::AttachNameSystem;

    m_conf->setValue("attachNameMode", mode);

    // Устаревший флаг держится в синхроне для чтения старыми версиями
    m_conf->setValue("attachHumanReadableNames", mode==AppConfig::AttachNameHybrid);
}


// Режим имен каталогов записей на диске. Unversioned-ключ как attachNameMode:
// отсутствующий ключ - системные имена как в обычной MyTetra
int AppConfigStorage::get_recordDirNameMode(void)
{
    if(m_conf->contains("recordDirNameMode"))
        return m_conf->value("recordDirNameMode", AppConfig::RecordDirSystem).toInt();

    return AppConfig::RecordDirSystem;
}


void AppConfigStorage::set_recordDirNameMode(int mode)
{
    if(mode<AppConfig::RecordDirSystem || mode>AppConfig::RecordDirHybrid)
        mode=AppConfig::RecordDirSystem;

    m_conf->setValue("recordDirNameMode", mode);
}
