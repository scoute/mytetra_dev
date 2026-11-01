#include "WindowSwitcher.h"

#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "views/tree/TreeScreen.h"
#include "views/record/MetaEditor.h"
#include "views/recordTable/RecordTableScreen.h"
#include "views/findInBaseScreen/FindScreen.h"
#include "libraries/AppContext.h"

WindowSwitcher::WindowSwitcher(QObject *parent) : QObject(parent)
{
 enableSwitch();

 // Редактор является встраиваемым, поэтому работа кнопки Back у него идет через callback функцию
 // MetaEditor *edView=find_object<MetaEditor>("editorScreen"); // Выясняется указатель на объект редактирования текста записи
 MetaEditor *metaEditor=AppContext::instance().globalParameters()->getMetaEditor();
 metaEditor->setBackCallback( this->switchFromRecordToRecordtable ); // Устанавливается функция обратного вызова при клике на кнопку Back
}

void WindowSwitcher::enableSwitch(void)
{
 // Для десктопа переключение окон виджетов вообще не должно включаться
 if(AppContext::instance().appConfig()->getInterfaceMode()=="desktop")
 {
   enableSwitcher=false;
   return;
 }

 enableSwitcher=true;
}

void WindowSwitcher::disableSwitch(void)
{
 enableSwitcher=false;
}

bool WindowSwitcher::getSwitchStatus(void)
{
 return enableSwitcher;
}

void WindowSwitcher::switchFromTreeToRecordtable(void)
{
 if(!enableSwitcher)
   return;

 // Скрываются все прочие области
 AppContext::instance().globalParameters()->getTreeScreen()->hide();
 AppContext::instance().globalParameters()->getMetaEditor()->hide();
 // AppContext::instance().globalParameters()->getFindScreen()->hide();

 QWidget *object=static_cast<QWidget *>( AppContext::instance().globalParameters()->getRecordTableScreen() );
 object->show();
 AppContext::instance().appConfig()->setFocusWidget(object->objectName());
}

void WindowSwitcher::switchFromTreeToFindInBase(void)
{
 if(!enableSwitcher)
   return;

}

// Статическая функция, используется редактором как callback функция при нажатии кнопки back в редакторе конечной записи
void WindowSwitcher::switchFromRecordToRecordtable(void)
{
 if(AppContext::instance().globalParameters()->getWindowSwitcher()==NULL)
   return;

 // Если переключение запрещено
 if(!AppContext::instance().globalParameters()->getWindowSwitcher()->getSwitchStatus())
   return;

 if(AppContext::instance().appConfig()->getInterfaceMode()!="mobile") // В статическом методе использовать нестатическую переменну enableSwitcher нельзя
   return;

 // Скрываются все прочие области
 AppContext::instance().globalParameters()->getTreeScreen()->hide();
 AppContext::instance().globalParameters()->getMetaEditor()->hide();
 // AppContext::instance().globalParameters()->getFindScreen()->hide();

 QWidget *object=static_cast<QWidget *>( AppContext::instance().globalParameters()->getRecordTableScreen() );
 object->show();
 AppContext::instance().appConfig()->setFocusWidget(object->objectName());
}

void WindowSwitcher::switchFromRecordToFindInBase(void)
{
 if(!enableSwitcher)
   return;

}

void WindowSwitcher::switchFromRecordtableToRecord(void)
{
 if(!enableSwitcher)
   return;

 // Скрываются все прочие области
 AppContext::instance().globalParameters()->getTreeScreen()->hide();
 AppContext::instance().globalParameters()->getRecordTableScreen()->hide();
 // AppContext::instance().globalParameters()->getFindScreen()->hide();

 QWidget *object=static_cast<QWidget *>( AppContext::instance().globalParameters()->getMetaEditor() );
 object->show();
 AppContext::instance().appConfig()->setFocusWidget(object->objectName());
}

void WindowSwitcher::switchFromRecordtableToFindInBase(void)
{
 if(!enableSwitcher)
   return;

}

void WindowSwitcher::switchFromRecordtableToTree(void)
{
 if(!enableSwitcher)
   return;

 // Скрываются все прочие области
 AppContext::instance().globalParameters()->getMetaEditor()->hide();
 AppContext::instance().globalParameters()->getRecordTableScreen()->hide();
 // AppContext::instance().globalParameters()->getFindScreen()->hide();

 QWidget *object=static_cast<QWidget *>( AppContext::instance().globalParameters()->getTreeScreen() );
 object->show();
 AppContext::instance().appConfig()->setFocusWidget(object->objectName());
}

void WindowSwitcher::restoreFocusWidget()
{
  if(!enableSwitcher)
    return;

  // Скрываются все прочие области
  AppContext::instance().globalParameters()->getTreeScreen()->hide();
  AppContext::instance().globalParameters()->getRecordTableScreen()->hide();
  AppContext::instance().globalParameters()->getMetaEditor()->hide();
  // AppContext::instance().globalParameters()->getFindScreen()->hide();

  QString widgetName=AppContext::instance().appConfig()->getFocusWidget();

  // Оформить через case
  if(widgetName=="treeScreen")
  {
    AppContext::instance().globalParameters()->getTreeScreen()->show();
    AppContext::instance().globalParameters()->getTreeScreen()->setFocus();
    return;
  }

  if(widgetName=="recordTableScreen")
  {
    AppContext::instance().globalParameters()->getRecordTableScreen()->show();
    AppContext::instance().globalParameters()->getRecordTableScreen()->setFocus();
    return;
  }

  if(widgetName=="editorScreen")
  {
    AppContext::instance().globalParameters()->getMetaEditor()->show();
    AppContext::instance().globalParameters()->getMetaEditor()->setFocus();
    return;
  }

  // Доработать
  if(widgetName=="findTableView")
  {
    AppContext::instance().globalParameters()->getFindScreen()->show();
    AppContext::instance().globalParameters()->getFindScreen()->setFocus();
    return;
  }

  // Остальные случаи - активируется дерево записей
  AppContext::instance().globalParameters()->getTreeScreen()->show();
  AppContext::instance().globalParameters()->getTreeScreen()->setFocus();
}
