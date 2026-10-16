#include <QWidget>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QGroupBox>

#include "AppConfigPage_Tree.h"
#include "models/appConfig/AppConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "libraries/helpers/ObjectHelper.h"
#include "views/tree/TreeScreen.h"


extern AppConfig mytetraConfig;


AppConfigPage_Tree::AppConfigPage_Tree(QWidget *parent) : ConfigPage(parent)
{
  setupUi();
  setupSignals();
  assembly();
}


AppConfigPage_Tree::~AppConfigPage_Tree()
{

}


void AppConfigPage_Tree::setupUi(void)
{
  qDebug() << "Create tree config page";

  // Вид счетчика записей в ветке: только свои [n] (по умолчанию)
  // либо общий итог [n][m] с учетом подветок
  ownCountOnly=new QRadioButton(this);
  ownCountOnly->setText(tr("Only its own notes [n] (default)"));

  subtreeCount=new QRadioButton(this);
  subtreeCount->setText(tr("Its own and sub-branch notes [n][m]"));

  if(mytetraConfig.get_showSubtreeCount()==false)
  {
    ownCountOnly->setChecked(true);
    subtreeCount->setChecked(false);
  }
  else
  {
    ownCountOnly->setChecked(false);
    subtreeCount->setChecked(true);
  }

  countsBox=new QGroupBox(this);
  countsBox->setTitle(tr("Show note count in branch"));
}


void AppConfigPage_Tree::setupSignals(void)
{

}


void AppConfigPage_Tree::assembly(void)
{
  QVBoxLayout *countsLayout=new QVBoxLayout;
  countsLayout->addWidget(ownCountOnly);
  countsLayout->addWidget(subtreeCount);
  countsBox->setLayout(countsLayout);

  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addWidget(countsBox);
  centralLayout->addStretch();

  setLayout(centralLayout);
}


void AppConfigPage_Tree::retranslateUi(void)
{
  ownCountOnly->setText(tr("Only its own notes [n] (default)"));
  subtreeCount->setText(tr("Its own and sub-branch notes [n][m]"));

  countsBox->setTitle(tr("Show note count in branch"));
}


// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Tree::applyChanges(void)
{
  qDebug() << "Apply changes tree";

  int result=0;

  // Выбор пишется в конфиг и применяется живьем:
  // отображаемые имена перечитываются без перезапуска
  if(mytetraConfig.get_showSubtreeCount()!=subtreeCount->isChecked())
  {
    mytetraConfig.set_showSubtreeCount(subtreeCount->isChecked());

    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

    if(treeScreen!=nullptr && treeScreen->knowTreeModel!=nullptr)
      treeScreen->knowTreeModel->refreshDisplayNames();
  }

  return result;
}
