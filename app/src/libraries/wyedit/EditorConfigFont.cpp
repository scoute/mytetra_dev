#include <QString>
#include <QWidget>
#include <QBoxLayout>
#include <QDir>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <QtDebug>
#include <QGroupBox>
#include <QColorDialog>

#include "EditorConfigFont.h"
#include "EditorConfig.h"

#include "libraries/helpers/ObjectHelper.h"


EditorConfigFont::EditorConfigFont(QWidget *parent) : ConfigPage(parent)
{
 qDebug() << "Create editor config font widget";
 
 // Устанавливается указатель на объект работы с конфигфайлом
 // conf=this->parent()->parent()->parent()->editorconfig;
 conf=find_object<EditorConfig>("editorconfig");
  
 setup_ui();
 setup_signals();
 assembly();
}


void EditorConfigFont::setup_ui(void)
{
 // Настройка основного шрифта
 defaultFontselectLabel=new QLabel(this);
 defaultFontselectLabel->setText(tr("Default font"));

 // Шрифт
 defaultFontselect=new QFontComboBox(this);
 QFont defaultFont;
 defaultFont.fromString(conf->get_default_font());
 defaultFontselect->setCurrentFont(defaultFont);
 
 // Размер
 defaultFontsize=new QSpinBox(this);
 defaultFontsize->setMinimum(1);
 defaultFontsize->setMaximum(250);
 defaultFontsize->setValue(conf->get_default_font_size());
 

 // Настройка моноширинного шрифта
 monospaceFontselectLabel=new QLabel(this);
 monospaceFontselectLabel->setText(tr("Monospace font"));

 // Шрифт
 monospaceFontselect=new QFontComboBox(this);
 QFont monoFont;
 monoFont.fromString(conf->get_monospace_font());
 monospaceFontselect->setCurrentFont(monoFont);

 // Размер
 monospaceFontsize=new QSpinBox(this);
 monospaceFontsize->setMinimum(1);
 monospaceFontsize->setMaximum(250);
 monospaceFontsize->setValue(conf->get_monospace_font_size());
 monospaceFontsize->setEnabled( conf->get_monospace_font_size_apply() );

 // Галка применения размера при нажатии кнопки моноширинного форматирования
 monospaceFontsizeapply=new QCheckBox(tr("Font size follows formatting"),this);
 monospaceFontsizeapply->setChecked( conf->get_monospace_font_size_apply() );


 // Настройка шрифта форматирования кода
 codeFontselectLabel=new QLabel(this);
 codeFontselectLabel->setText(tr("Code font"));

 // Шрифт
 codeFontselect=new QFontComboBox(this);
 QFont codeFont;
 codeFont.fromString(conf->get_code_font());
 codeFontselect->setCurrentFont(codeFont);

 // Размер
 codeFontsize=new QSpinBox(this);
 codeFontsize->setMinimum(1);
 codeFontsize->setMaximum(250);
 codeFontsize->setValue(conf->get_code_font_size());
 codeFontsize->setEnabled( conf->get_code_font_size_apply() );

 // Галка применения размера при нажатии кнопки форматирования кода
 codeFontsizeapply=new QCheckBox(tr("Font size follows formatting"),this);
 codeFontsizeapply->setChecked( conf->get_code_font_size_apply() );

 // Размер отступа
 codeIndentsize=new QSpinBox(this);
 codeIndentsize->setMinimum(1);
 codeIndentsize->setMaximum(250);
 codeIndentsize->setValue(conf->get_code_indent_size());
 codeIndentsize->setEnabled( conf->get_code_indent_size_apply() );
 
 // Галка применения размера отступа
 codeIndentsizeapply=new QCheckBox(tr("Indent size follows formatting"),this);
 codeIndentsizeapply->setChecked( conf->get_code_indent_size_apply() );


 // Кнопка выбора цвета текста кода
 codeSelectColorLabel=new QLabel(this);
 codeSelectColorLabel->setText(tr("Code color"));

 codeSelectColorButton=new QToolButton(this);
 codeColor = new QColor(conf->get_code_font_color());

 // Квадратик на кнопке выбора цвета кода
 QPixmap pix(16, 16);
 pix.fill(codeColor->rgb());
 codeSelectColorButton->setIcon(pix);

 // Клик по кнопке цвета или её горячая клавиша применяет последний выбранный цвет без пикера
 applyLastColorCheckBox=new QCheckBox(tr("Apply last text/background color"), this);
 applyLastColorCheckBox->setToolTip(tr("Clicking the text or background color button or its hotkey paints in the last used color without opening the picker"));
 applyLastColorCheckBox->setChecked(conf->get_applyLastColorOnClick());
}


void EditorConfigFont::setup_signals(void)
{
 connect(monospaceFontsizeapply, &QCheckBox::stateChanged,
         this,                   &EditorConfigFont::on_monospace_applysize_state_changed);

 connect(codeFontsizeapply, &QCheckBox::stateChanged,
         this,              &EditorConfigFont::on_code_applysize_state_changed);
 connect(codeIndentsizeapply, &QCheckBox::stateChanged,
         this,                &EditorConfigFont::on_code_applyindent_state_changed);

 connect(codeSelectColorButton, &QToolButton::clicked,
         this,                  &EditorConfigFont::on_code_select_color_button_click);

 connect(linkColorExternalButton, &QToolButton::clicked,
         this,                    &EditorConfigFont::on_linkcolor_external_button_click);
 connect(linkColorInternalButton, &QToolButton::clicked,
         this,                    &EditorConfigFont::on_linkcolor_internal_button_click);
}


void EditorConfigFont::assembly(void)
{
 // Сборка всех блоков в окно

 // Основной шрифт
 QHBoxLayout *defaultFontLayout=new QHBoxLayout();
 defaultFontLayout->addWidget(defaultFontselectLabel);
 defaultFontLayout->addWidget(defaultFontselect);
 defaultFontLayout->addWidget(defaultFontsize);


 // Моноширинный шрифт
 QHBoxLayout *monospaceFontLayout=new QHBoxLayout();
 monospaceFontLayout->addWidget(monospaceFontselectLabel);
 monospaceFontLayout->addWidget(monospaceFontselect);

 QGridLayout *monospaceGridLayout=new QGridLayout();
 monospaceGridLayout->addWidget(monospaceFontsizeapply,   0, 0);
 monospaceGridLayout->addWidget(monospaceFontsize,        0, 1);
 
 QVBoxLayout *monospaceLayout=new QVBoxLayout();
 monospaceLayout->addLayout(monospaceFontLayout);
 monospaceLayout->addLayout(monospaceGridLayout);
 
 QGroupBox *monospaceGroupBox=new QGroupBox(tr("Monospace font options"));
 monospaceGroupBox->setLayout(monospaceLayout);


 // Шрифт форматирования кода
 QHBoxLayout *codeFontLayout=new QHBoxLayout();
 codeFontLayout->addWidget(codeFontselectLabel);
 codeFontLayout->addWidget(codeFontselect);

 QGridLayout *codeGridLayout=new QGridLayout();
 codeGridLayout->addWidget(codeSelectColorLabel,   0, 0);
 codeGridLayout->addWidget(codeSelectColorButton,  0, 1);
 codeGridLayout->addWidget(codeFontsizeapply,      1, 0);
 codeGridLayout->addWidget(codeFontsize,           1, 1);
 codeGridLayout->addWidget(codeIndentsizeapply,    2, 0);
 codeGridLayout->addWidget(codeIndentsize,         2, 1);

 QVBoxLayout *codeLayout=new QVBoxLayout();
 codeLayout->addLayout(codeFontLayout);
 codeLayout->addLayout(codeGridLayout);

 QGroupBox *codeGroupBox=new QGroupBox(tr("Code font options"));
 codeGroupBox->setLayout(codeLayout);


 // Цвета ссылок: внешние (http, файлы) и внутренние (на другие заметки)
 linkColorExternalLabel=new QLabel(this);
 linkColorExternalLabel->setText(tr("External link color"));

 linkColorExternalButton=new QToolButton(this);
 linkColorExternal=new QColor(conf->get_linkColorExternal());

 linkColorInternalLabel=new QLabel(this);
 linkColorInternalLabel->setText(tr("Internal link color"));

 linkColorInternalButton=new QToolButton(this);
 linkColorInternal=new QColor(conf->get_linkColorInternal());

 QPixmap linkPix(16, 16);
 linkPix.fill(linkColorExternal->rgb());
 linkColorExternalButton->setIcon(linkPix);
 linkPix.fill(linkColorInternal->rgb());
 linkColorInternalButton->setIcon(linkPix);

 QGridLayout *linkGridLayout=new QGridLayout();
 linkGridLayout->addWidget(linkColorExternalLabel,  0, 0);
 linkGridLayout->addWidget(linkColorExternalButton, 0, 1);
 linkGridLayout->addWidget(linkColorInternalLabel,  1, 0);
 linkGridLayout->addWidget(linkColorInternalButton, 1, 1);

 QGroupBox *linkGroupBox=new QGroupBox(tr("Link colors"));
 linkGroupBox->setLayout(linkGridLayout);


 // Общая сборка
 QVBoxLayout *centralLayout=new QVBoxLayout();
 centralLayout->addLayout(defaultFontLayout);
 centralLayout->addWidget(monospaceGroupBox);
 centralLayout->addWidget(codeGroupBox);
 centralLayout->addWidget(linkGroupBox);
 centralLayout->addWidget(applyLastColorCheckBox);
 centralLayout->addStretch();
 
 setLayout(centralLayout);
}


void EditorConfigFont::on_monospace_applysize_state_changed(int i)
{
 if(i==Qt::Checked) monospaceFontsize->setEnabled(true);
 else               monospaceFontsize->setEnabled(false);
}


void EditorConfigFont::on_code_applysize_state_changed(int i)
{
 if(i==Qt::Checked) codeFontsize->setEnabled(true);
 else               codeFontsize->setEnabled(false);
}


void EditorConfigFont::on_code_applyindent_state_changed(int i)
{
 if(i==Qt::Checked) codeIndentsize->setEnabled(true);
 else               codeIndentsize->setEnabled(false);
}


void EditorConfigFont::on_code_select_color_button_click()
{
 // Исходный цвет
 QColor color(conf->get_code_font_color());

 // Диалог запроса цвета
 QColor selectedColor=QColorDialog::getColor(color, this, QString(), QColorDialog::ShowAlphaChannel);

 // Если цвет выбран, и он правильный
 if(selectedColor.isValid())
  {
   // Меняется цвет кнопки (не работает в Ubuntu 10.04)
   // codeSelectColorButton->setPalette(QPalette(selectedColor));

   // Меняется цвет квадратика на кнопке
   QPixmap pix(16, 16);
   pix.fill(selectedColor);
   codeSelectColorButton->setIcon(pix);

   // Запоминается новый цвет
   codeColor->setNamedColor(selectedColor.name());
  }
}


void EditorConfigFont::on_linkcolor_external_button_click()
{
 on_linkcolor_button_click(linkColorExternalButton, linkColorExternal);
}


void EditorConfigFont::on_linkcolor_internal_button_click()
{
 on_linkcolor_button_click(linkColorInternalButton, linkColorInternal);
}


void EditorConfigFont::on_linkcolor_button_click(QToolButton *button, QColor *color)
{
 // Исходный цвет
 QColor startColor(*color);

 // Диалог запроса цвета
 QColor selectedColor=QColorDialog::getColor(startColor, this, QString(), QColorDialog::ShowAlphaChannel);

 // Если цвет выбран, и он правильный
 if(selectedColor.isValid())
  {
   // Меняется цвет квадратика на кнопке
   QPixmap pix(16, 16);
   pix.fill(selectedColor);
   button->setIcon(pix);

   // Запоминается новый цвет
   color->setNamedColor(selectedColor.name());
  }
}


int EditorConfigFont::applyChanges(void)
{
 qDebug() << "Apply changes editor font";
 
 // Уровень сложности вносимых изменений
 // 0 - изменения можно делать на лету
 // 1 - для принятия изменений нужна перезагрузка
 int difficultChanges=0;

 
 // Если был изменен основной шрифт
 if(conf->get_default_font()!=defaultFontselect->currentFont().toString()) {
  conf->set_default_font(defaultFontselect->currentFont().toString());
 }
 
 // Если был изменен размер основного шрифта
 if(conf->get_default_font_size()!=defaultFontsize->value()) {
  conf->set_default_font_size(defaultFontsize->value());
 }


 // Если был изменен моноширинный шрифт
 if(conf->get_monospace_font()!=monospaceFontselect->currentFont().toString()) {
  conf->set_monospace_font(monospaceFontselect->currentFont().toString());
 }
 
 // Если был изменен размер моноширинного шрифта
 if(conf->get_monospace_font_size()!=monospaceFontsize->value()) {
  conf->set_monospace_font_size(monospaceFontsize->value());
 }

 // Если была изменена галка разрешения установки размера моноширинного шрифта
 if(conf->get_monospace_font_size_apply()!=monospaceFontsizeapply->isChecked()) {
  conf->set_monospace_font_size_apply(monospaceFontsizeapply->isChecked());
 }
 

 // Если был изменен шрифт форматирования кода
 if(conf->get_code_font()!=codeFontselect->currentFont().toString()) {
  conf->set_code_font(codeFontselect->currentFont().toString());
 }

 // Если был изменен размер шрифта форматирования кода
 if(conf->get_code_font_size()!=codeFontsize->value()) {
  conf->set_code_font_size(codeFontsize->value());
 }

 // Если была изменена галка разрешения установки размера шрифта кода
 if(conf->get_code_font_size_apply()!=codeFontsizeapply->isChecked()) {
  conf->set_code_font_size_apply(codeFontsizeapply->isChecked());
 }

 // Если был изменен отступ шрифта кода
 if(conf->get_code_indent_size()!=codeIndentsize->value()) {
  conf->set_code_indent_size(codeIndentsize->value());
 }

 // Если была изменена галка разрешения установки отступа шрифта кода
 if(conf->get_code_indent_size_apply()!=codeIndentsizeapply->isChecked()) {
  conf->set_code_indent_size_apply(codeIndentsizeapply->isChecked());
 }

 // Если был изменен цвет начертания кода
 if(conf->get_code_font_color()!=codeColor->name())
  conf->set_code_font_color(codeColor->name());

 // Если были изменены цвета ссылок
 if(conf->get_linkColorExternal()!=linkColorExternal->name())
  conf->set_linkColorExternal(linkColorExternal->name());

 if(conf->get_linkColorInternal()!=linkColorInternal->name())
  conf->set_linkColorInternal(linkColorInternal->name());

 // Клик по кнопке цвета: последний цвет без пикера
 if(conf->get_applyLastColorOnClick()!=applyLastColorCheckBox->isChecked()) {
  conf->set_applyLastColorOnClick(applyLastColorCheckBox->isChecked());
 }

 return difficultChanges;
}
