#ifndef _TEAMPROFILE_H_
#define _TEAMPROFILE_H_

#include <QObject>
#include <QString>

// Профиль пользователя команды SyncTetra: имя, почта, идентификатор,
// каталог обмена. Хранится в conf.ini, в структуру базы не входит.
// Фактический каталог обмена: значение из конфига, иначе
// <каталог бинарника>/shared, если не писабелен - <рабочий>/shared

class TeamProfile : public QObject
{
    Q_OBJECT

public:
    TeamProfile(QObject *pobj=nullptr);
    ~TeamProfile();

    QString getTeamName(void);
    void setTeamName(const QString &name);

    QString getTeamEmail(void);
    void setTeamEmail(const QString &email);

    // Идентификатор. При первом обращении создается
    // автоматически и сохраняется
    QString getTeamId(void);
    bool isTeamIdSet(void);
    void generateNewTeamId(void);

    // Профиль настроен: задано имя пользователя
    bool isConfigured(void);

    QString getSharedDirConfigValue(void);
    QString getSharedDir(void);
    void setSharedDir(const QString &path);
};

#endif /* _TEAMPROFILE_H_ */
