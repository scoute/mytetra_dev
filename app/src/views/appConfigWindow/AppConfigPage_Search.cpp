#include <QDebug>
#include <QVBoxLayout>
#include <QWidget>
#include <QGroupBox>
#include <QRadioButton>

#include "AppConfigPage_Search.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"

extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;


AppConfigPage_Search::AppConfigPage_Search(QWidget *parent) : ConfigPage(parent)
{
    setupUi();
    setupSignals();
    assembly();
}


AppConfigPage_Search::~AppConfigPage_Search()
{

}


void AppConfigPage_Search::setupUi()
{
    qDebug() << "Create search config page";

    // Поиск внутри заметки: строкой-полоской или отдельным окном.
    // По умолчанию полоска, окно выбирается откреплением или здесь
    findBarRadio=new QRadioButton(this);
    findBarRadio->setText(tr("Embedded bar in note"));

    findWindowRadio=new QRadioButton(this);
    findWindowRadio->setText(tr("Separate window"));

    if(mytetraConfig.get_editorFindEmbedded())
      findBarRadio->setChecked(true);
    else
      findWindowRadio->setChecked(true);
}


void AppConfigPage_Search::setupSignals()
{

}


void AppConfigPage_Search::assembly()
{
    // Группировщик виджетов для поиска внутри заметки
    findInNoteBox=new QGroupBox(this);
    findInNoteBox->setTitle(tr("Find in note"));

    // Виджеты вставляются в группировщик поиска внутри заметки
    QVBoxLayout *findInNoteLayout = new QVBoxLayout;
    findInNoteLayout->addWidget(findBarRadio);
    findInNoteLayout->addWidget(findWindowRadio);
    findInNoteBox->setLayout(findInNoteLayout);

    // Собирается основной слой
    QVBoxLayout *centralLayout=new QVBoxLayout();
    centralLayout->addWidget(findInNoteBox);
    centralLayout->addStretch();

    // Основной слой устанавливается
    setLayout(centralLayout);
}


void AppConfigPage_Search::retranslateUi(void)
{
  findBarRadio->setText(tr("Embedded bar in note"));
  findWindowRadio->setText(tr("Separate window"));
  findInNoteBox->setTitle(tr("Find in note"));
}


// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Search::applyChanges()
{
    qDebug() << "Apply changes search";

    int result=0;

    // Сохраняется выбранный вариант поиска в заметке: полоска или окно
    bool embedded=findBarRadio->isChecked();
    if(mytetraConfig.get_editorFindEmbedded()!=embedded)
      mytetraConfig.set_editorFindEmbedded(embedded);

    return result;
}
