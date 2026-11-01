#ifndef __APPCONFIGSERVICES_H__
#define __APPCONFIGSERVICES_H__

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

class AppConfigServices : public QObject
{
    Q_OBJECT

public:

    explicit AppConfigServices(QObject *parent=nullptr);
    void setBackend(QSettings *conf, AppConfig *host);

    // Разрешен ли запуск MCP-сервера внешним AI-клиентом
    bool get_mcpAllow(void);
    void set_mcpAllow(bool state);
    // Игнорировать ошибки проверки TLS при загрузке картинок/файлов.
    // По умолчанию выключено: невалидный/самоподписанный сертификат
    // отклоняется. Включение снижает защиту от MITM
    bool get_tlsIgnoreInvalidCertificates(void);
    void set_tlsIgnoreInvalidCertificates(bool state);
    // Раскладка импорта Telegram: "month" (заметка на месяц,
    // по умолчанию), "year" (заметка на год, прямо в чате),
    // "day" (заметка на день, ветки чат-год-месяц)
    // или "message" (заметка на сообщение, ветки чат-год-месяц-день)
    QString get_telegramImportMode(void);
    void set_telegramImportMode(const QString &mode);
    // Формат голосовых заметок по умолчанию: opus или mp3.
    // Выбирается радио на странице Voice notes, в диалоге
    // записи переключается. Неподдержанный бэкендом сбрасывается
    QString get_voiceNoteFormat(void);
    void set_voiceNoteFormat(QString format);
    // Подавление фонового шума в диалоге записи голосовых заметок.
    // По умолчанию включено: цепочка HP+гейт наTurn очищает гул
    bool get_voiceNoteNoiseSuppress(void);
    void set_voiceNoteNoiseSuppress(bool value);
    // Качество голосовых заметок по умолчанию: 0 low, 1 medium, 2 high
    int get_voiceNoteQuality(void);
    void set_voiceNoteQuality(int quality);

private:

    QSettings *m_conf=nullptr;
    AppConfig *m_host=nullptr;
};

#endif // __APPCONFIGSERVICES_H__
