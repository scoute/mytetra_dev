#ifndef _TEAMPROFILE_H_
#define _TEAMPROFILE_H_

#include <QObject>
#include <QString>

// Объект, хранящий данные пользователя команды (имя, почту, идентификатор),
// которыми маркируются публикуемые в общий каталог ветки.
// Данные хранятся в conf.ini и не входят в структуру базы знаний

class TeamProfile : public QObject
{
    Q_OBJECT

public:
    TeamProfile(QObject *pobj=nullptr);
    ~TeamProfile();

    // Имя пользователя в команде
    QString getTeamName(void);
    void setTeamName(const QString &name);

    // Электронная почта пользователя в команде
    QString getTeamEmail(void);
    void setTeamEmail(const QString &email);

    // Идентификатор пользователя в команде.
    // При первом обращении, если идентификатор еще не задан,
    // он создается автоматически и сохраняется в конфиге
    QString getTeamId(void);

    // Признак, что идентификатор пользователя уже задан в конфиге
    bool isTeamIdSet(void);

    // Принудительная генерация нового идентификатора
    void generateNewTeamId(void);

    // Значение каталога обмена из конфига (может быть пустым —
    // тогда используется каталог по умолчанию)
    QString getSharedDirConfigValue(void);

    // Фактический каталог обмена.
    // Пустое значение в конфиге разрешается так:
    // <каталог бинарника>/shared; если каталог бинарника не писабелен —
    // фоллбэк <рабочий каталог>/shared
    QString getSharedDir(void);

    // Установка каталога обмена. Пустое значение означает каталог по умолчанию
    void setSharedDir(const QString &path);

    // Полная метка владельца для отображения, например "Иван <ivan@example.com>"
    QString getOwnerLabel(void);

    // Признак, что профиль настроен (задано имя пользователя)
    bool isConfigured(void);

signals:
    // Сигнал об изменении данных профиля
    void teamProfileChanged(void);

private:
    // Создание нового идентификатора пользователя
    QString createNewTeamId(void);
};

#endif // _TEAMPROFILE_H_