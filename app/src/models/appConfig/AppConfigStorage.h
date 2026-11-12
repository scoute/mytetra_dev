#ifndef __APPCONFIGSTORAGE_H__
#define __APPCONFIGSTORAGE_H__

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

class AppConfigStorage : public QObject
{
    Q_OBJECT

public:

    explicit AppConfigStorage(QObject *parent=nullptr);
    void setBackend(QSettings *conf, AppConfig *host);

    bool set_tetradir(QString dirName);
    // Директория с базой знаний
    QString get_tetradir(void);
    bool set_trashdir(QString dirName);
    // Директория с корзиной
    QString get_trashdir(void);
    // Максимальный размер директории корзины в мегабайтах
    unsigned int get_trashsize(void);
    bool set_trashsize(unsigned int mbSize);
    // Максимально допустимое число файлов в корзине
    int get_trashmaxfilecount(void);
    bool set_trashmaxfilecount(int count);
    // Путь на диске по которому пользователь открывал файлы чтобы приаттачить их к записи
    QString getAttachAppendDir(void);
    void setAttachAppendDir(QString dir);
    // Путь на диске по которому пользователь сохранял (Сохранить как...) приаттаченные файлы
    QString getAttachSaveAsDir(void);
    void setAttachSaveAsDir(QString dir);
    // Разрешать ли для просмотра расшифровывать зашифрованные файлы в директорию корзины MyTetra
    bool getEnableDecryptFileToTrashDirectory(void);
    void setEnableDecryptFileToTrashDirectory(bool state);
    // Размер файла лога дейсвтвий
    unsigned int getActionLogMaximumSize();
    void setActionLogMaximumSize(unsigned int mbSize);
    // Разрешено ли логирование
    bool getEnableLogging(void);
    void setEnableLogging(bool state);
    // Человеко-читаемые имена вложенных файлов на диске (схема id_имя.ext
    // вместо id.ext). По умолчанию выключено. Включается кнопкой миграции
    // в настройках прикрепленных файлов, после чего новые вложения
    // сразу сохраняются по человеко-читаемой схеме.
    // Устаревший флаг: теперь схема задается режимом attachNameMode,
    // методы оставлены для совместимости (true = гибридный режим)
    bool get_attachHumanReadableNames(void);
    void set_attachHumanReadableNames(bool state);
    int get_attachNameMode(void);
    void set_attachNameMode(int mode);

    // Режим ссылок на папки: абсолютный путь, относительный
    // каталога записи или всегда спрашивать. Тот же unversioned-паттерн
    // что attachNameMode
    int get_attachLinkMode(void);
    void set_attachLinkMode(int mode);
    // Последний выбор меню дропа файлов (копия/ссылка): 0 - копия,
    // 1 - относительная ссылка, 2 - абсолютная ссылка. Unversioned,
    // как attachLinkMode: отсутствующий ключ - копия
    int get_attachDropDefault(void);
    void set_attachDropDefault(int mode);
    // Режим имен каталогов записей: системный (id) или гибридный
    // (id_имя). Тот же unversioned-паттерн что attachNameMode:
    // отсутствующий ключ покрывается дефолтом, миграция conf не нужна
    int get_recordDirNameMode(void);
    void set_recordDirNameMode(int mode);

private:

    QSettings *m_conf=nullptr;
    AppConfig *m_host=nullptr;
};

#endif // __APPCONFIGSTORAGE_H__
