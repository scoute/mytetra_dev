#include <QString>
#include <QWidget>
#include <QBoxLayout>
#include <QDir>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <QtDebug>
#include <QLabel>
#include <QGroupBox>
#include <QRadioButton>

#include "EditorConfigMisc.h"
#include "EditorConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/ConfigEditorHelper.h"

extern GlobalParameters globalParameters;


EditorConfigMisc::EditorConfigMisc(QWidget *parent) : ConfigPage(parent)
{
  // Устанавливается указатель на объект работы с конфигфайлом
  conf=find_object<EditorConfig>("editorconfig");

  setupUi();
  setupSignals();
  assembly();
}


void EditorConfigMisc::setupUi(void)
{
 // Шаг изменения отступа
 indentStepLabel=new QLabel(this);
 indentStepLabel->setText(tr("Indent step"));

 indentStepFlexion=new QLabel(this);
 indentStepFlexion->setText(tr("pixels"));

 indentStep=new QSpinBox(this);
 indentStep->setMinimum(1);
 indentStep->setMaximum(250);
 indentStep->setValue(conf->get_indent_step());

 // Размер табуляции для клавиши Tab
 tabStopDistanceLabel=new QLabel(this);
 tabStopDistanceLabel->setText(tr("Tab size"));

 tabStopDistanceFlexionLabel=new QLabel(this);
 tabStopDistanceFlexionLabel->setText(tr("letters"));

 tabStopDistanceSpinBox=new QSpinBox(this);
 tabStopDistanceSpinBox->setMinimum(1);
 tabStopDistanceSpinBox->setMaximum(50);
 tabStopDistanceSpinBox->setValue(conf->get_tab_size());

 // Формат сохранения новых картинок
 imageFormatLabel=new QLabel(this);
 imageFormatLabel->setText(tr("Image format"));

 imagePngButton=new QRadioButton(tr("PNG (lossless)"), this);
 imageJpgButton=new QRadioButton(tr("JPG (compact)"), this);

 const QString imageFormat=conf->get_imageSaveFormat();

 if(imageFormat==QStringLiteral("jpg"))
   imageJpgButton->setChecked(true);
 else
   imagePngButton->setChecked(true);

 const QString imageFormatHint=tr("Applies to newly inserted images");
 imageFormatLabel->setToolTip(imageFormatHint);
 imagePngButton->setToolTip(imageFormatHint);
 imageJpgButton->setToolTip(imageFormatHint);

 // Лимит длинной стороны новых картинок, 0 — без ограничения
 imageMaxSizeLabel=new QLabel(this);
 imageMaxSizeLabel->setText(tr("Max image size"));

 imageMaxSizeCombo=new QComboBox(this);
 imageMaxSizeCombo->addItem(QStringLiteral("640"), 640);
 imageMaxSizeCombo->addItem(QStringLiteral("800"), 800);
 imageMaxSizeCombo->addItem(QStringLiteral("1024"), 1024);
 imageMaxSizeCombo->addItem(QStringLiteral("1280"), 1280);
 imageMaxSizeCombo->addItem(QStringLiteral("1600"), 1600);
 imageMaxSizeCombo->addItem(tr("No limit"), 0);

 const int maxSizeIndex=imageMaxSizeCombo->findData(conf->get_imageMaxSize());

 if(maxSizeIndex>=0)
   imageMaxSizeCombo->setCurrentIndex(maxSizeIndex);
 else
   imageMaxSizeCombo->setCurrentIndex(imageMaxSizeCombo->findData(1280));

 imageMaxSizeLabel->setToolTip(imageFormatHint);
 imageMaxSizeCombo->setToolTip(imageFormatHint);

 // Мастер-тумблер изменения истинного размера файла картинки
 allowTrueImageResizeCheckBox=new QCheckBox(this);
 allowTrueImageResizeCheckBox->setText(tr("Allow changing true image file size"));
 allowTrueImageResizeCheckBox->setToolTip(tr("Shows a button in Image properties that rewrites the image file. The original is moved to trash. Off by default."));
 allowTrueImageResizeCheckBox->setChecked(conf->get_allowTrueImageResize());

 // Кнопка редактирования файла конфигурации WyEdit
 editWyEditConfigFile=new QPushButton(this);
 editWyEditConfigFile->setText(tr("Edit config file"));
 editWyEditConfigFile->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed, QSizePolicy::ToolButton));
}


void EditorConfigMisc::setupSignals(void)
{
  connect(editWyEditConfigFile, &QPushButton::clicked,
          this,                 &EditorConfigMisc::onClickedEditWyEditConfigFile);
}


void EditorConfigMisc::assembly(void)
{
  // Сборка всех блоков настройки отступа в окно
  QGridLayout *configLayout=new QGridLayout();
  configLayout->addWidget(indentStepLabel,   0, 0);
  configLayout->addWidget(indentStep,        0, 1);
  configLayout->addWidget(indentStepFlexion, 0, 2);

  // Компановка контролов "Шаг дистанции для клавиши Tab"
  configLayout->addWidget(tabStopDistanceLabel,         1, 0);
  configLayout->addWidget(tabStopDistanceSpinBox,       1, 1);
  configLayout->addWidget(tabStopDistanceFlexionLabel,  1, 2);

 // Формат картинок: подпись и две радиокнопки в своей строке
 configLayout->addWidget(imageFormatLabel, 2, 0);
 configLayout->addWidget(imagePngButton,   2, 1);
 configLayout->addWidget(imageJpgButton,   2, 2);

 // Лимит размера: подпись и комбобокс в своей строке
 configLayout->addWidget(imageMaxSizeLabel, 3, 0);
 configLayout->addWidget(imageMaxSizeCombo, 3, 1);

  // Группировщик виджетов для опасной зоны
  QGroupBox *dangerBox=new QGroupBox(this);
  dangerBox->setTitle(tr("Danger actions (Attention!)"));

  // Виджеты вставляются в группировщик опасной зоны
  QVBoxLayout *dangerLayout = new QVBoxLayout();
  dangerLayout->addWidget(editWyEditConfigFile);
  dangerLayout->addWidget(allowTrueImageResizeCheckBox);
  dangerBox->setLayout(dangerLayout);


  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addLayout(configLayout);
  centralLayout->addWidget(dangerBox);
  centralLayout->addStretch();

  setLayout(centralLayout);
}


void EditorConfigMisc::onClickedEditWyEditConfigFile(void)
{
  // Сбрасываются в файл конфига все возможные изменения, которые, возможно еще не были записаны
  conf->sync();

  ConfigEditorHelper::editConfigFile( globalParameters.getWorkDirectory()+"/editorconf.ini", 0.8f );
}


int EditorConfigMisc::applyChanges(void)
{
 qDebug() << "Apply changes editor misc";
 
 // Уровень сложности вносимых изменений
 // 0 - изменения можно делать на лету
 // 1 - для принятия изменений нужна перезагрузка
 int difficultChanges=0;

  // Если был изменен размер основного шрифта
 if(conf->get_indent_step()!=indentStep->value()) {
  conf->set_indent_step(indentStep->value());
 }

 // Если был изменен размер табуляции для клавиши Tab
 if(conf->get_tab_size()!=tabStopDistanceSpinBox->value()) {
   conf->set_tab_size(tabStopDistanceSpinBox->value());
 }

 // Если был изменен формат сохранения картинок
 const QString newImageFormat=imageJpgButton->isChecked() ? QStringLiteral("jpg")
                                                          : QStringLiteral("png");

 if(conf->get_imageSaveFormat()!=newImageFormat) {
   conf->set_imageSaveFormat(newImageFormat);
 }

 // Если был изменен лимит размера картинок
 const int newImageMaxSize=imageMaxSizeCombo->currentData().toInt();

 if(conf->get_imageMaxSize()!=newImageMaxSize) {
   conf->set_imageMaxSize(newImageMaxSize);
 }

 // Мастер-тумблер истинного размера
 if(conf->get_allowTrueImageResize()!=allowTrueImageResizeCheckBox->isChecked()) {
   conf->set_allowTrueImageResize(allowTrueImageResizeCheckBox->isChecked());
 }
 
 return difficultChanges;
}
