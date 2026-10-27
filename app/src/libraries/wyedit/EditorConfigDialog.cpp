#include <QStackedWidget>
#include <QDialog>

#include "views/appConfigWindow/ConfigDialog.h"

#include "EditorConfigDialog.h"
#include "EditorConfigFont.h"
#include "EditorConfigMathExpression.h"
#include "EditorConfigMisc.h"
#include "EditorConfigToolbars.h"
#include "views/appConfigWindow/AppConfigDialog.h"


EditorConfigDialog::EditorConfigDialog(QWidget *parent) : QWidget(parent)
{
 configDialog=new ConfigDialog();
 
 configDialog->setWindowTitle(tr("Editor settings"));
 
 configDialog->addWidget(new EditorConfigFont(this),tr("Fonts"));
 configDialog->addWidget(new EditorConfigToolbars(this),tr("Toolbars"));
 configDialog->addWidget(new EditorConfigMathExpression(this),tr("Formulas"));
 configDialog->addWidget(new EditorConfigMisc(this),tr("Misc"));

 // Кнопка перехода в общие настройки слева внизу:
 // текущие правки применяются как по OK, затем открывается
 // диалог общих настроек
 configDialog->setAlterSettingsButton(tr("MyTetra settings..."));
 connect(configDialog, &ConfigDialog::alterSettingsRequested,
         this,          &EditorConfigDialog::onAlterToAppSettings);

 // Страница шрифтов чуть больше вьюпорта по sizeHint
 // (~15px в ширину, ~45px в высоту с тремя цветами ссылок),
 // из-за чего появляются скроллы. Небольшой запас убирает их
 // с учетом разных локалей
 configDialog->adjustSize();
 configDialog->resize(configDialog->width()+60, configDialog->height()+60);

 configDialog->exec();
}


void EditorConfigDialog::onAlterToAppSettings(void)
{
 configDialog->applyAndClose();

 AppConfigDialog appDialog("", nullptr);
 appDialog.exec();
}

