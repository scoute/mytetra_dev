#include <QStackedWidget>
#include <QDialog>

#include "views/appConfigWindow/ConfigDialog.h"

#include "EditorConfigDialog.h"
#include "EditorConfigFont.h"
#include "EditorConfigMathExpression.h"
#include "EditorConfigMisc.h"
#include "EditorConfigToolbars.h"
#include "views/appConfigWindow/AppConfigDialog.h"


EditorConfigDialog::EditorConfigDialog(QWidget *parent, const QString &page) : QWidget(parent)
{
 configDialog=new ConfigDialog();
 
 configDialog->setWindowTitle(tr("Editor settings"));
 
 configDialog->addWidget(new EditorConfigFont(this),tr("Fonts"));
 configDialog->addWidget(new EditorConfigToolbars(this),tr("Toolbars"));
 configDialog->addWidget(new EditorConfigMathExpression(this),tr("Formulas"));
 QListWidgetItem *miscItem=configDialog->addWidget(new EditorConfigMisc(this),tr("Misc"));

 // Прямой прыжок на страницу (из ПКМ-меню плавающего содержания):
 // выбор до exec, иначе диалог уже закрыт
 if(page==QStringLiteral("misc") && miscItem!=nullptr)
   configDialog->externalChangePage(miscItem);

 // Кнопка перехода в общие настройки слева внизу:
 // текущие правки применяются как по OK, затем открывается
 // диалог общих настроек
 configDialog->setAlterSettingsButton(tr("MyTetra settings..."));
 connect(configDialog, &ConfigDialog::alterSettingsRequested,
         this,          &EditorConfigDialog::onAlterToAppSettings);

 configDialog->exec();
}


void EditorConfigDialog::onAlterToAppSettings(void)
{
 configDialog->applyAndClose();

 AppConfigDialog appDialog("", nullptr);
 appDialog.exec();
}

