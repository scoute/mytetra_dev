#ifndef _APPCONFIGPAGE_TEAM_H_
#define _APPCONFIGPAGE_TEAM_H_

#include <QLineEdit>
#include <QPushButton>

#include "ConfigPage.h"

// Страница настроек участника команды.
// Здесь задаются имя, почта и идентификатор пользователя,
// которыми маркируются публикуемые в общий каталог ветки

class AppConfigPage_Team : public ConfigPage
{
    Q_OBJECT

public:
    AppConfigPage_Team(QWidget *parent=nullptr);
    ~AppConfigPage_Team();

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    // Применение изменений страницы настроек
    virtual int applyChanges(void);

private slots:
    // Слот реакции на кнопку генерации нового идентификатора
    void onClickedRegenerateTeamId(void);

    // Слот реакции на кнопку выбора каталога обмена
    void onClickedBrowseSharedDir(void);

private:
    QLineEdit *teamNameEdit;
    QLineEdit *teamEmailEdit;
    QLineEdit *teamIdEdit;
    QPushButton *regenerateIdButton;

    QLineEdit *sharedDirEdit;
    QPushButton *sharedDirBrowseButton;
};

#endif // _APPCONFIGPAGE_TEAM_H_