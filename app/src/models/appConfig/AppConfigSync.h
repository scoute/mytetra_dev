#ifndef __APPCONFIGSYNC_H__
#define __APPCONFIGSYNC_H__

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

class AppConfigSync : public QObject
{
    Q_OBJECT

public:

    explicit AppConfigSync(QObject *parent=nullptr);
    void setBackend(QSettings *conf, AppConfig *host);

    // Умное автосохранение: только при реальных правках,
    // без изменений ни файла ни коммита. По умолчанию вкл, 10 мин
    bool get_autosaveEnable(void);
    void set_autosaveEnable(bool flag);
    int get_autosaveInterval(void);
    void set_autosaveInterval(int minutes);
    // Сквош истории изменений: автосжатие снизу по лимиту.
    // По умолчанию включено, хранить 100 (genesis плюс свежие)
    bool get_historySquashEnable(void);
    void set_historySquashEnable(bool flag);
    int get_historySquashKeep(void);
    void set_historySquashKeep(int count);
    // Стратегия сжатия (GitHelper::SquashMode).
    // По умолчанию удалять старые
    int get_historySquashMode(void);
    void set_historySquashMode(int mode);
    // Мастер-тумблер ведения истории заметок. Выключенная история
    // не снимается и диалог не открывается. По умолчанию выключена:
    // нужен установленный git, проверяется при включении
    bool get_historyEnable(void);
    void set_historyEnable(bool flag);
    QString get_synchrocommand(void);
    void set_synchrocommand(QString command);
    bool get_synchroonstartup(void);
    void set_synchroonstartup(bool flag);
    bool get_synchroonexit(void);
    void set_synchroonexit(bool flag);
    bool getSyncroConsoleDetails(void);
    void setSyncroConsoleDetails(bool flag);
    // Разрешена ли периодическая проверка файла базы на предмет изменения сторонней программой
    bool getEnablePeriodicCheckBase(void);
    void setEnablePeriodicCheckBase(bool state);
    // Период проверки файла базы на предмет изменения сторонней программой
    int getCheckBasePeriod();
    void setCheckBasePeriod(int period);
    // Необходимо ли выводить сообщение если база была изменена другой программой
    bool getEnablePeriodicCheckMessage(void);
    void setEnablePeriodicCheckMessage(bool state);
    // Разрешена ли периодическая синхронизация
    bool getEnablePeriodicSyncro(void);
    void setEnablePeriodicSyncro(bool state);
    // Период автоматической периодической синхронизации
    int getPeriodicSyncroPeriod(void);
    void setPeriodicSyncroPeriod(int period);

private:

    QSettings *m_conf=nullptr;
    AppConfig *m_host=nullptr;
};

#endif // __APPCONFIGSYNC_H__
