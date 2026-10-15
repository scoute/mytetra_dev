#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFileDialog>

#include "AppConfigPage_Team.h"
#include "models/teamProfile/TeamProfile.h"


extern TeamProfile teamProfile;


AppConfigPage_Team::AppConfigPage_Team(QWidget *parent) : ConfigPage(parent)
{
    setupUi();
    setupSignals();
    assembly();
}


AppConfigPage_Team::~AppConfigPage_Team()
{

}


void AppConfigPage_Team::setupUi(void)
{
    // Поле ввода имени пользователя в команде
    teamNameEdit=new QLineEdit(this);
    teamNameEdit->setToolTip(tr("For example: Ivan Petrov"));

    // Поле ввода электронной почты пользователя в команде
    teamEmailEdit=new QLineEdit(this);
    teamEmailEdit->setToolTip(tr("For example: ivan@example.com"));

    // Поле отображения идентификатора пользователя в команде
    teamIdEdit=new QLineEdit(this);
    teamIdEdit->setReadOnly(true);

    // Кнопка генерации нового идентификатора
    regenerateIdButton=new QPushButton(this);
    regenerateIdButton->setText(tr("Generate new ID"));
    regenerateIdButton->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed, QSizePolicy::ToolButton));

    // Поле ввода каталога обмена
    sharedDirEdit=new QLineEdit(this);
    sharedDirEdit->setToolTip(tr("Directory for publishing branches, synchronized via Syncthing. Empty means the default directory near the program binary."));

    // Кнопка выбора каталога обмена
    sharedDirBrowseButton=new QPushButton(this);
    sharedDirBrowseButton->setText(tr("Browse..."));
    sharedDirBrowseButton->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed, QSizePolicy::ToolButton));
}


void AppConfigPage_Team::setupSignals(void)
{
    connect(regenerateIdButton, &QPushButton::clicked, this, &AppConfigPage_Team::onClickedRegenerateTeamId);
    connect(sharedDirBrowseButton, &QPushButton::clicked, this, &AppConfigPage_Team::onClickedBrowseSharedDir);
}


void AppConfigPage_Team::assembly(void)
{
    QString infoText=tr("These settings identify you as the owner of branches "
                        "published to the shared directory. "
                        "The name and email are shown to your colleagues when they view a shared branch.");

    // Поясняющая надпись
    QLabel *infoLabel=new QLabel(infoText, this);
    infoLabel->setWordWrap(true);

    // Подсказка про Syncthing: что синкается, а что — никогда
    QLabel *syncthingHint=new QLabel(this);
    syncthingHint->setWordWrap(true);
    syncthingHint->setText(tr("Syncthing tip: share <b>only</b> the <i>sync/</i> folder "
                              "inside the shared directory between devices. "
                              "Your personal database, the local journal and owner "
                              "copies are never synchronized — only published branches travel."));
    syncthingHint->setToolTip(tr("One Syncthing folder = shared/sync/. "
                                 "Do not share the whole shared/ directory, your database or trash."));

    // Группировщик полей профиля
    QGroupBox *profileBox=new QGroupBox(this);
    profileBox->setTitle(tr("Profile"));

    // Сборка формы профиля
    QFormLayout *profileLayout=new QFormLayout;
    profileLayout->addRow(tr("Name:"), teamNameEdit);
    profileLayout->addRow(tr("Email:"), teamEmailEdit);
    profileLayout->addRow(tr("ID:"), teamIdEdit);
    profileLayout->addRow(tr(""), regenerateIdButton);
    profileBox->setLayout(profileLayout);

    // Группировщик настроек каталога обмена
    QGroupBox *sharedDirBox=new QGroupBox(this);
    sharedDirBox->setTitle(tr("Shared directory"));

    // Строка каталога обмена с кнопкой выбора
    QWidget *sharedDirRow=new QWidget(this);
    QHBoxLayout *sharedDirRowLayout=new QHBoxLayout;
    sharedDirRowLayout->setContentsMargins(0,0,0,0);
    sharedDirRowLayout->addWidget(sharedDirEdit);
    sharedDirRowLayout->addWidget(sharedDirBrowseButton);
    sharedDirRow->setLayout(sharedDirRowLayout);

    QFormLayout *sharedDirLayout=new QFormLayout;
    sharedDirLayout->addRow(tr("Path:"), sharedDirRow);
    sharedDirBox->setLayout(sharedDirLayout);

    // Сборка основного слоя
    QVBoxLayout *centralLayout=new QVBoxLayout();
    centralLayout->addWidget(infoLabel);
    centralLayout->addWidget(profileBox);
    centralLayout->addWidget(sharedDirBox);
    centralLayout->addWidget(syncthingHint);
    centralLayout->addStretch();

    setLayout(centralLayout);

    // Заполнение полей данными текущего профиля.
    // Идентификатор не создается при открытии окна настроек,
    // он появится автоматически при первой публикации ветки
    teamNameEdit->setText(teamProfile.getTeamName());
    teamEmailEdit->setText(teamProfile.getTeamEmail());
    if(teamProfile.isTeamIdSet())
        teamIdEdit->setText(teamProfile.getTeamId());
    else
        teamIdEdit->setText(tr("(will be generated on first publication)"));

    sharedDirEdit->setText(teamProfile.getSharedDir());
}


void AppConfigPage_Team::onClickedRegenerateTeamId(void)
{
    QMessageBox::StandardButton answer
        =QMessageBox::warning(this,
                              tr("New ID generation"),
                              tr("The ID is used to mark your branches in the shared directory for your colleagues. "
                                 "Changing the ID will make previously published branches shown as owned by an unknown user. "
                                 "Do you want to continue?"),
                              QMessageBox::Yes | QMessageBox::Cancel,
                              QMessageBox::Cancel);

    if(answer!=QMessageBox::Yes)
    {
        return;
    }

    teamProfile.generateNewTeamId();
    teamIdEdit->setText(teamProfile.getTeamId());
}


void AppConfigPage_Team::onClickedBrowseSharedDir(void)
{
    QString currentPath=sharedDirEdit->text();
    if(currentPath.isEmpty())
        currentPath=teamProfile.getSharedDir();

    QString selectedDir=QFileDialog::getExistingDirectory(this,
                                                          tr("Select shared directory"),
                                                          currentPath);

    if(!selectedDir.isEmpty())
    {
        sharedDirEdit->setText(selectedDir);
    }
}


// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Team::applyChanges(void)
{
    teamProfile.setTeamName(teamNameEdit->text().trimmed());
    teamProfile.setTeamEmail(teamEmailEdit->text().trimmed());
    teamProfile.setSharedDir(sharedDirEdit->text().trimmed());

    return 0;
}