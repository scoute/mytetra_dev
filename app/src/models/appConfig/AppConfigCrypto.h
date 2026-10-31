#ifndef __APPCONFIGCRYPTO_H__
#define __APPCONFIGCRYPTO_H__

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

class AppConfigCrypto : public QObject
{
    Q_OBJECT

public:

    explicit AppConfigCrypto(QObject *parent=nullptr);
    void setBackend(QSettings *conf, AppConfig *host);

    QString get_howpassrequest(void);
    void set_howpassrequest(QString mode);
    bool get_autoClosePasswordEnable(void);
    void set_autoClosePasswordEnable(bool flag);
    int get_autoClosePasswordDelay(void);
    void set_autoClosePasswordDelay(int delay);
    QString getPasswordMiddleHash(void);
    void setPasswordMiddleHash(QString hash);
    bool getPasswordSaveFlag(void);
    void setPasswordSaveFlag(bool flag);
    bool getRememberCursorAtHistoryNavigation(void);
    void setRememberCursorAtHistoryNavigation(bool flag);
    bool getRememberCursorAtOrdinarySelection(void);
    void setRememberCursorAtOrdinarySelection(bool flag);
    // Метод шифрования: rc5 (совместимый) или xchacha (современный)
    QString get_cryptMethod(void);
    void set_cryptMethod(const QString &method);

private:

    QSettings *m_conf=nullptr;
    AppConfig *m_host=nullptr;
};

#endif // __APPCONFIGCRYPTO_H__
