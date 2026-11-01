#ifndef __APPCONFIGCLIPPER_H__
#define __APPCONFIGCLIPPER_H__

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

class AppConfigClipper : public QObject
{
    Q_OBJECT

public:

    explicit AppConfigClipper(QObject *parent=nullptr);
    void setBackend(QSettings *conf, AppConfig *host);

    // Лимиты клиппера: сколько картинок забирать в одну заметку
    // и максимальный размер одной картинки в мегабайтах
    int get_clipperMaxImages(void);
    bool set_clipperMaxImages(int count);
    int get_clipperMaxImageSizeMb(void);
    bool set_clipperMaxImageSizeMb(int mbSize);
    // Веб-клиппер: включение, глобальный хоткей, запомненная ветка
    bool get_clipperenable(void);
    void set_clipperenable(bool flag);
    QString get_clipperhotkey(void);
    void set_clipperhotkey(QString sequence);
    QString get_clipperbranchid(void);
    void set_clipperbranchid(QString id);
    // Режим целевой ветки клипа: 0 — по умолчанию (unsorted_notes),
    // 1 — выбранная ветка (clipperbranchid), 2 — спрашивать каждый раз
    int get_clipperbranchmode(void);
    void set_clipperbranchmode(int mode);
    // Частые ветки клипов: пары id:count через ";", частые впереди,
    // лимит 16. Нижние плитки опроса показывают их вместо полного списка
    QString get_clipperrecent(void);
    void set_clipperrecent(const QString &ids);
    // Какие 16 плиток внизу опроса: 0 — последние, 1 — частые
    int get_clippertilemode(void);
    void set_clippertilemode(int mode);
    // При попытке вставить дубль открывать оригинал заметки
    bool get_clipperopendup(void);
    void set_clipperopendup(bool open);

private:

    QSettings *m_conf=nullptr;
    AppConfig *m_host=nullptr;
};

#endif // __APPCONFIGCLIPPER_H__
