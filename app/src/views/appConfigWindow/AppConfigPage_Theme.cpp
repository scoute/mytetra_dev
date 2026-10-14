#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QToolButton>
#include <QPushButton>
#include <QSpinBox>
#include <QColorDialog>
#include <QFontDialog>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>
#include <QColor>
#include <QFont>
#include <QDebug>

#include "AppConfigPage_Theme.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "libraries/helpers/CssHelper.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/MtComboBox.h"
#include "views/record/MetaEditor.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;
extern FixedParameters fixedParameters;


AppConfigPage_Theme::AppConfigPage_Theme(QWidget *parent) : ConfigPage(parent)
{
  setupUi();
  setupSignals();
  assembly();
}


AppConfigPage_Theme::~AppConfigPage_Theme()
{

}


// Квадратик-образец на кнопке выбора цвета
void AppConfigPage_Theme::setColorForButton(QToolButton *button,
                                            QColor *color,
                                            const QColor &value)
{
  QPixmap pix(16, 16);

  if(value.isValid())
    pix.fill(value);
  else
    pix.fill(Qt::transparent);

  button->setIcon(pix);

  *color=value;
}


void AppConfigPage_Theme::setupUi(void)
{
  qDebug() << "Create theme config page";

  setupThemeComboBox();
  setupIconSizeComboBox();

  baseBox=new QGroupBox(this);
  customHint=new QLabel(this);
  customHint->setWordWrap(true);

  // --- Заметка (переехало из Misc) ---
  noteBox=new QGroupBox(this);

  notePaperColorLabel=new QLabel(this);
  notePaperColorButton=new QToolButton(this);
  notePaperColor=new QColor();
  notePaperColorIsTheme=mytetraConfig.get_notePaperColor().isEmpty();
  setColorForButton(notePaperColorButton, notePaperColor,
    QColor(MetaEditor::effectiveNotePaperColor(mytetraConfig.getInterfaceTheme(),
                                               mytetraConfig.get_notePaperColor())));
  notePaperColorReset=new QPushButton(this);

  noteFontColorLabel=new QLabel(this);
  noteFontColorButton=new QToolButton(this);
  noteFontColor=new QColor();
  noteFontColorIsTheme=mytetraConfig.get_noteFontColor().isEmpty();
  setColorForButton(noteFontColorButton, noteFontColor,
    QColor(MetaEditor::effectiveNoteFontColor(mytetraConfig.getInterfaceTheme(),
                                              mytetraConfig.get_noteFontColor())));
  noteFontColorReset=new QPushButton(this);

  secretColorLabel=new QLabel(this);
  secretColorButton=new QToolButton(this);
  secretColor=new QColor();
  setColorForButton(secretColorButton, secretColor,
                    QColor(mytetraConfig.get_secretColor()));
  secretColorReset=new QPushButton(this);

  notePaperImageLabel=new QLabel(this);
  notePaperImageLoad=new QPushButton(this);
  notePaperImageClear=new QPushButton(this);
  notePaperImagePath=mytetraConfig.get_themeCustomNotePaperImage();

  // --- Дерево веток ---
  treeBox=new QGroupBox(this);

  treeBackgroundLabel=new QLabel(this);
  treeBackgroundButton=new QToolButton(this);
  treeBackground=new QColor();
  treeBackgroundIsTheme=mytetraConfig.get_themeCustomTreeBackground().isEmpty();
  setColorForButton(treeBackgroundButton, treeBackground,
                    QColor(mytetraConfig.get_themeCustomTreeBackground()));
  treeBackgroundReset=new QPushButton(this);

  treeTextLabel=new QLabel(this);
  treeTextButton=new QToolButton(this);
  treeText=new QColor();
  treeTextIsTheme=mytetraConfig.get_themeCustomTreeText().isEmpty();
  setColorForButton(treeTextButton, treeText,
                    QColor(mytetraConfig.get_themeCustomTreeText()));
  treeTextReset=new QPushButton(this);

  treeFontSizeLabel=new QLabel(this);
  treeFontSize=new QSpinBox(this);
  treeFontSize->setRange(-1, 48);
  treeFontSize->setSpecialValueText(tr("Theme"));
  treeFontSize->setValue(mytetraConfig.get_themeCustomTreeFontSize());

  treeIndentationLabel=new QLabel(this);
  treeIndentation=new QSpinBox(this);
  treeIndentation->setRange(-1, 80);
  treeIndentation->setSpecialValueText(tr("Theme"));
  treeIndentation->setValue(mytetraConfig.get_themeCustomTreeIndentation());

  // --- Список заметок ---
  tableBox=new QGroupBox(this);

  tableBackgroundLabel=new QLabel(this);
  tableBackgroundButton=new QToolButton(this);
  tableBackground=new QColor();
  tableBackgroundIsTheme=mytetraConfig.get_themeCustomTableBackground().isEmpty();
  setColorForButton(tableBackgroundButton, tableBackground,
                    QColor(mytetraConfig.get_themeCustomTableBackground()));
  tableBackgroundReset=new QPushButton(this);

  tableTextLabel=new QLabel(this);
  tableTextButton=new QToolButton(this);
  tableText=new QColor();
  tableTextIsTheme=mytetraConfig.get_themeCustomTableText().isEmpty();
  setColorForButton(tableTextButton, tableText,
                    QColor(mytetraConfig.get_themeCustomTableText()));
  tableTextReset=new QPushButton(this);

  tableAlternateLabel=new QLabel(this);
  tableAlternateButton=new QToolButton(this);
  tableAlternate=new QColor();
  tableAlternateIsTheme=mytetraConfig.get_themeCustomTableAlternate().isEmpty();
  setColorForButton(tableAlternateButton, tableAlternate,
                    QColor(mytetraConfig.get_themeCustomTableAlternate()));
  tableAlternateReset=new QPushButton(this);

  tableFontSizeLabel=new QLabel(this);
  tableFontSize=new QSpinBox(this);
  tableFontSize->setRange(-1, 48);
  tableFontSize->setSpecialValueText(tr("Theme"));
  tableFontSize->setValue(mytetraConfig.get_themeCustomTableFontSize());

  tableRowHeightLabel=new QLabel(this);
  tableRowHeight=new QSpinBox(this);
  tableRowHeight->setRange(-1, 200);
  tableRowHeight->setSpecialValueText(tr("Theme"));
  tableRowHeight->setValue(mytetraConfig.get_themeCustomTableRowHeight());

  // --- Шрифт интерфейса ---
  fontBox=new QGroupBox(this);

  interfaceFontLabel=new QLabel(this);
  interfaceFontButton=new QPushButton(this);
  interfaceFontFamily=mytetraConfig.get_themeCustomInterfaceFontFamily();
  interfaceFontSize=mytetraConfig.get_themeCustomInterfaceFontSize();
  refreshInterfaceFontButton();
  interfaceFontReset=new QPushButton(this);

  resetAllButton=new QPushButton(this);

  refreshNotePaperImageLabel();

  retranslateUi();
}


void AppConfigPage_Theme::refreshInterfaceFontButton(void)
{
  if(interfaceFontFamily.isEmpty() && interfaceFontSize<=0)
    interfaceFontButton->setText(tr("Theme"));
  else if(interfaceFontFamily.isEmpty())
    interfaceFontButton->setText(tr("Size %1").arg(interfaceFontSize));
  else if(interfaceFontSize<=0)
    interfaceFontButton->setText(interfaceFontFamily);
  else
    interfaceFontButton->setText(
      tr("%1, %2").arg(interfaceFontFamily).arg(interfaceFontSize));
}


// Выбор темы интерфейса (переехало из Appearance)
void AppConfigPage_Theme::setupThemeComboBox()
{
  themeLabel=new QLabel(this);
  themeLabel->setText(tr("Interface theme"));

  themeNameComboBox=new MtComboBox(this);

  QMap<QString, QString> translateNames;
  translateNames[ "default" ] = tr("Default");
  translateNames[ "dark" ]    = tr("Dark");
  translateNames[ "modern" ]  = tr("Modern");
  translateNames[ "midnight" ]= tr("Midnight");
  translateNames[ "custom" ]  = tr("Custom");

  int count = -1;
  int currentIndex = -1;

  for (auto name : fixedParameters.themesAvailableList )
  {
    QString translateName=translateNames.value(name, "Unknown");
    themeNameComboBox->addItem(translateName);

    count++;
    if ( name == mytetraConfig.getInterfaceTheme() )
      currentIndex = count;
  }

  themeNameComboBox->setCurrentIndex( currentIndex );
}


// Выбор размера иконок (переехало из Appearance)
void AppConfigPage_Theme::setupIconSizeComboBox()
{
  iconSizeLabel=new QLabel(this);
  iconSizeLabel->setText(tr("Icon size"));

  iconSizeComboBox=new MtComboBox(this);
  iconSizeComboBox->addItem( tr("System") );

  int count = 0;
  int currentIndex = -1;

  for ( auto sizeName : fixedParameters.interfaceIconSizeAvailableMap.keys() )
  {
    QString name = fixedParameters.interfaceIconSizeAvailableMap.at( sizeName ).first;
    iconSizeComboBox->addItem( name );

    count++;
    if ( sizeName == mytetraConfig.getInterfaceIconSize() )
      currentIndex = count;
  }

  if ( currentIndex==-1 or mytetraConfig.getInterfaceIconSize()=="" )
    currentIndex = 0;

  iconSizeComboBox->setCurrentIndex( currentIndex );
}


void AppConfigPage_Theme::setupSignals(void)
{
  connect(notePaperColorButton, &QToolButton::clicked, this, &AppConfigPage_Theme::onClickedNotePaperColor);
  connect(notePaperColorReset, &QPushButton::clicked, this, &AppConfigPage_Theme::onClickedNotePaperColorReset);
  connect(noteFontColorButton, &QToolButton::clicked, this, &AppConfigPage_Theme::onClickedNoteFontColor);
  connect(noteFontColorReset, &QPushButton::clicked, this, &AppConfigPage_Theme::onClickedNoteFontColorReset);
  connect(secretColorButton, &QToolButton::clicked, this, &AppConfigPage_Theme::onClickedSecretColor);
  connect(secretColorReset, &QPushButton::clicked, this, &AppConfigPage_Theme::onClickedSecretColorReset);
  connect(notePaperImageLoad, &QPushButton::clicked, this, &AppConfigPage_Theme::onClickedNotePaperImageLoad);
  connect(notePaperImageClear, &QPushButton::clicked, this, &AppConfigPage_Theme::onClickedNotePaperImageClear);
  connect(treeBackgroundButton, &QToolButton::clicked, this, &AppConfigPage_Theme::onClickedTreeBackground);
  connect(treeBackgroundReset, &QPushButton::clicked, this, &AppConfigPage_Theme::onClickedTreeBackgroundReset);
  connect(treeTextButton, &QToolButton::clicked, this, &AppConfigPage_Theme::onClickedTreeText);
  connect(treeTextReset, &QPushButton::clicked, this, &AppConfigPage_Theme::onClickedTreeTextReset);
  connect(tableBackgroundButton, &QToolButton::clicked, this, &AppConfigPage_Theme::onClickedTableBackground);
  connect(tableBackgroundReset, &QPushButton::clicked, this, &AppConfigPage_Theme::onClickedTableBackgroundReset);
  connect(tableTextButton, &QToolButton::clicked, this, &AppConfigPage_Theme::onClickedTableText);
  connect(tableTextReset, &QPushButton::clicked, this, &AppConfigPage_Theme::onClickedTableTextReset);
  connect(tableAlternateButton, &QToolButton::clicked, this, &AppConfigPage_Theme::onClickedTableAlternate);
  connect(tableAlternateReset, &QPushButton::clicked, this, &AppConfigPage_Theme::onClickedTableAlternateReset);
  connect(interfaceFontButton, &QPushButton::clicked, this, &AppConfigPage_Theme::onClickedInterfaceFont);
  connect(interfaceFontReset, &QPushButton::clicked, this, &AppConfigPage_Theme::onClickedInterfaceFontReset);
  connect(resetAllButton, &QPushButton::clicked, this, &AppConfigPage_Theme::onClickedResetAll);
}


// Строка выбора цвета: подпись, образец, сброс
static QHBoxLayout *colorRowLayout(QWidget *label, QWidget *button, QWidget *reset)
{
  QHBoxLayout *layout=new QHBoxLayout();
  layout->addWidget(label);
  layout->addWidget(button);
  layout->addWidget(reset);
  layout->addStretch();

  return layout;
}


void AppConfigPage_Theme::assembly(void)
{
  // Базовая тема и иконки
  QHBoxLayout *themeLayout=new QHBoxLayout();
  themeLayout->addWidget(themeLabel);
  themeLayout->addWidget(themeNameComboBox);
  themeLayout->addStretch();

  QHBoxLayout *iconSizeLayout=new QHBoxLayout();
  iconSizeLayout->addWidget(iconSizeLabel);
  iconSizeLayout->addWidget(iconSizeComboBox);
  iconSizeLayout->addStretch();

  QVBoxLayout *baseLayout=new QVBoxLayout();
  baseLayout->addLayout(themeLayout);
  baseLayout->addWidget(customHint);
  baseLayout->addLayout(iconSizeLayout);

  baseBox->setLayout(baseLayout);

  // Заметка
  QVBoxLayout *noteLayout=new QVBoxLayout();
  noteLayout->addLayout(colorRowLayout(notePaperColorLabel, notePaperColorButton, notePaperColorReset));
  noteLayout->addLayout(colorRowLayout(noteFontColorLabel, noteFontColorButton, noteFontColorReset));
  noteLayout->addLayout(colorRowLayout(secretColorLabel, secretColorButton, secretColorReset));

  QHBoxLayout *paperImageLayout=new QHBoxLayout();
  paperImageLayout->addWidget(notePaperImageLabel);
  paperImageLayout->addWidget(notePaperImageLoad);
  paperImageLayout->addWidget(notePaperImageClear);
  paperImageLayout->addStretch();
  noteLayout->addLayout(paperImageLayout);

  noteBox->setLayout(noteLayout);

  // Дерево
  QVBoxLayout *treeLayout=new QVBoxLayout();
  treeLayout->addLayout(colorRowLayout(treeBackgroundLabel, treeBackgroundButton, treeBackgroundReset));
  treeLayout->addLayout(colorRowLayout(treeTextLabel, treeTextButton, treeTextReset));

  QHBoxLayout *treeFontLayout=new QHBoxLayout();
  treeFontLayout->addWidget(treeFontSizeLabel);
  treeFontLayout->addWidget(treeFontSize);
  treeFontLayout->addStretch();
  treeLayout->addLayout(treeFontLayout);

  QHBoxLayout *treeIndentLayout=new QHBoxLayout();
  treeIndentLayout->addWidget(treeIndentationLabel);
  treeIndentLayout->addWidget(treeIndentation);
  treeIndentLayout->addStretch();
  treeLayout->addLayout(treeIndentLayout);

  treeBox->setLayout(treeLayout);

  // Список
  QVBoxLayout *tableLayout=new QVBoxLayout();
  tableLayout->addLayout(colorRowLayout(tableBackgroundLabel, tableBackgroundButton, tableBackgroundReset));
  tableLayout->addLayout(colorRowLayout(tableTextLabel, tableTextButton, tableTextReset));
  tableLayout->addLayout(colorRowLayout(tableAlternateLabel, tableAlternateButton, tableAlternateReset));

  QHBoxLayout *tableFontLayout=new QHBoxLayout();
  tableFontLayout->addWidget(tableFontSizeLabel);
  tableFontLayout->addWidget(tableFontSize);
  tableFontLayout->addStretch();
  tableLayout->addLayout(tableFontLayout);

  QHBoxLayout *tableRowLayout=new QHBoxLayout();
  tableRowLayout->addWidget(tableRowHeightLabel);
  tableRowLayout->addWidget(tableRowHeight);
  tableRowLayout->addStretch();
  tableLayout->addLayout(tableRowLayout);

  tableBox->setLayout(tableLayout);

  // Шрифт интерфейса
  QHBoxLayout *fontSelectLayout=new QHBoxLayout();
  fontSelectLayout->addWidget(interfaceFontLabel);
  fontSelectLayout->addWidget(interfaceFontButton);
  fontSelectLayout->addWidget(interfaceFontReset);
  fontSelectLayout->addStretch();

  QVBoxLayout *fontLayout=new QVBoxLayout();
  fontLayout->addLayout(fontSelectLayout);

  fontBox->setLayout(fontLayout);

  // Основной слой
  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addWidget(baseBox);
  centralLayout->addWidget(noteBox);
  centralLayout->addWidget(treeBox);
  centralLayout->addWidget(tableBox);
  centralLayout->addWidget(fontBox);
  centralLayout->addWidget(resetAllButton);
  centralLayout->addStretch();

  setLayout(centralLayout);
}


void AppConfigPage_Theme::onClickedNotePaperColor()
{
  QColor selectedColor=QColorDialog::getColor(*notePaperColor, this);

  if(selectedColor.isValid())
  {
    notePaperColorIsTheme=false;
    setColorForButton(notePaperColorButton, notePaperColor, selectedColor);
  }
}


void AppConfigPage_Theme::onClickedNotePaperColorReset()
{
  notePaperColorIsTheme=true;

  setColorForButton(notePaperColorButton, notePaperColor,
    QColor(MetaEditor::effectiveNotePaperColor(mytetraConfig.getInterfaceTheme(), "")));
}


void AppConfigPage_Theme::onClickedNoteFontColor()
{
  QColor selectedColor=QColorDialog::getColor(*noteFontColor, this);

  if(selectedColor.isValid())
  {
    noteFontColorIsTheme=false;
    setColorForButton(noteFontColorButton, noteFontColor, selectedColor);
  }
}


void AppConfigPage_Theme::onClickedNoteFontColorReset()
{
  noteFontColorIsTheme=true;

  setColorForButton(noteFontColorButton, noteFontColor,
    QColor(MetaEditor::effectiveNoteFontColor(mytetraConfig.getInterfaceTheme(), "")));
}


void AppConfigPage_Theme::onClickedSecretColor()
{
  QColor selectedColor=QColorDialog::getColor(*secretColor, this);

  if(selectedColor.isValid())
    setColorForButton(secretColorButton, secretColor, selectedColor);
}


void AppConfigPage_Theme::onClickedSecretColorReset()
{
  setColorForButton(secretColorButton, secretColor, QColor("#77767b"));
}


void AppConfigPage_Theme::refreshNotePaperImageLabel(void)
{
  if(notePaperImagePath.isEmpty())
    notePaperImageLabel->setText(tr("Paper image: none"));
  else
    notePaperImageLabel->setText(
      tr("Paper image: %1").arg(QFileInfo(notePaperImagePath).fileName()));
}


void AppConfigPage_Theme::onClickedNotePaperImageLoad(void)
{
  const QString from=QFileDialog::getOpenFileName(
    this, tr("Select paper image"),
    QDir::homePath(),
    tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.svg)"));

  if(from.isEmpty())
    return;

  // Тайл копируется в рабочий каталог: исходник могут удалить,
  // а путь в конфиге должен жить всегда
  QDir workDir(globalParameters.getWorkDirectory());

  if(!workDir.mkpath("custom-theme"))
    return;

  const QString target=workDir.absoluteFilePath(
    "custom-theme/paper."+QFileInfo(from).suffix().toLower());

  QFile::remove(target);

  if(!QFile::copy(from, target))
    return;

  notePaperImagePath=target;
  refreshNotePaperImageLabel();
}


void AppConfigPage_Theme::onClickedNotePaperImageClear(void)
{
  if(!notePaperImagePath.isEmpty())
    QFile::remove(notePaperImagePath);

  notePaperImagePath.clear();
  refreshNotePaperImageLabel();
}


void AppConfigPage_Theme::onClickedTreeBackground()
{
  QColor selectedColor=QColorDialog::getColor(*treeBackground, this);

  if(selectedColor.isValid())
  {
    treeBackgroundIsTheme=false;
    setColorForButton(treeBackgroundButton, treeBackground, selectedColor);
  }
}


void AppConfigPage_Theme::onClickedTreeBackgroundReset()
{
  treeBackgroundIsTheme=true;
  setColorForButton(treeBackgroundButton, treeBackground, QColor());
}


void AppConfigPage_Theme::onClickedTreeText()
{
  QColor selectedColor=QColorDialog::getColor(*treeText, this);

  if(selectedColor.isValid())
  {
    treeTextIsTheme=false;
    setColorForButton(treeTextButton, treeText, selectedColor);
  }
}


void AppConfigPage_Theme::onClickedTreeTextReset()
{
  treeTextIsTheme=true;
  setColorForButton(treeTextButton, treeText, QColor());
}


void AppConfigPage_Theme::onClickedTableBackground()
{
  QColor selectedColor=QColorDialog::getColor(*tableBackground, this);

  if(selectedColor.isValid())
  {
    tableBackgroundIsTheme=false;
    setColorForButton(tableBackgroundButton, tableBackground, selectedColor);
  }
}


void AppConfigPage_Theme::onClickedTableBackgroundReset()
{
  tableBackgroundIsTheme=true;
  setColorForButton(tableBackgroundButton, tableBackground, QColor());
}


void AppConfigPage_Theme::onClickedTableText()
{
  QColor selectedColor=QColorDialog::getColor(*tableText, this);

  if(selectedColor.isValid())
  {
    tableTextIsTheme=false;
    setColorForButton(tableTextButton, tableText, selectedColor);
  }
}


void AppConfigPage_Theme::onClickedTableTextReset()
{
  tableTextIsTheme=true;
  setColorForButton(tableTextButton, tableText, QColor());
}


void AppConfigPage_Theme::onClickedTableAlternate()
{
  QColor selectedColor=QColorDialog::getColor(*tableAlternate, this);

  if(selectedColor.isValid())
  {
    tableAlternateIsTheme=false;
    setColorForButton(tableAlternateButton, tableAlternate, selectedColor);
  }
}


void AppConfigPage_Theme::onClickedTableAlternateReset()
{
  tableAlternateIsTheme=true;
  setColorForButton(tableAlternateButton, tableAlternate, QColor());
}


void AppConfigPage_Theme::onClickedInterfaceFont()
{
  bool ok=false;
  QFont current;

  if(!interfaceFontFamily.isEmpty())
    current.setFamily(interfaceFontFamily);

  if(interfaceFontSize>0)
    current.setPointSize(interfaceFontSize);

  QFont selectedFont=QFontDialog::getFont(&ok, current, this);

  if(ok)
  {
    interfaceFontFamily=selectedFont.family();
    interfaceFontSize=selectedFont.pointSize();
    refreshInterfaceFontButton();
  }
}


void AppConfigPage_Theme::onClickedInterfaceFontReset()
{
  interfaceFontFamily.clear();
  interfaceFontSize=-1;
  refreshInterfaceFontButton();
}


void AppConfigPage_Theme::onClickedResetAll(void)
{
  // Всё к теме: цвета чистятся, размеры в -1, шрифт в тему
  notePaperColorIsTheme=true;  setColorForButton(notePaperColorButton, notePaperColor,
    QColor(MetaEditor::effectiveNotePaperColor(mytetraConfig.getInterfaceTheme(), "")));

  noteFontColorIsTheme=true;
  setColorForButton(noteFontColorButton, noteFontColor,
    QColor(MetaEditor::effectiveNoteFontColor(mytetraConfig.getInterfaceTheme(), "")));

  setColorForButton(secretColorButton, secretColor, QColor("#77767b"));

  if(!notePaperImagePath.isEmpty())
    QFile::remove(notePaperImagePath);

  notePaperImagePath.clear();
  refreshNotePaperImageLabel();

  treeBackgroundIsTheme=true;
  setColorForButton(treeBackgroundButton, treeBackground, QColor());
  treeTextIsTheme=true;
  setColorForButton(treeTextButton, treeText, QColor());
  treeFontSize->setValue(-1);
  treeIndentation->setValue(-1);

  tableBackgroundIsTheme=true;
  setColorForButton(tableBackgroundButton, tableBackground, QColor());
  tableTextIsTheme=true;
  setColorForButton(tableTextButton, tableText, QColor());
  tableAlternateIsTheme=true;
  setColorForButton(tableAlternateButton, tableAlternate, QColor());
  tableFontSize->setValue(-1);
  tableRowHeight->setValue(-1);

  onClickedInterfaceFontReset();
}


void AppConfigPage_Theme::retranslateUi(void)
{
  baseBox->setTitle(tr("Base theme"));
  themeLabel->setText(tr("Interface theme"));
  customHint->setText(tr("Custom theme builds on Default plus settings below."));

  QMap<QString, QString> translateNames;
  translateNames[ "default" ] = tr("Default");
  translateNames[ "dark" ]    = tr("Dark");
  translateNames[ "modern" ]  = tr("Modern");
  translateNames[ "midnight" ]= tr("Midnight");
  translateNames[ "custom" ]  = tr("Custom");

  for(int i=0; i<themeNameComboBox->count(); ++i)
  {
    QString name=fixedParameters.themesAvailableList.value(i, "Unknown");
    themeNameComboBox->setItemText(i, translateNames.value(name, "Unknown"));
  }

  iconSizeLabel->setText(tr("Icon size"));
  iconSizeComboBox->setItemText(0, tr("System"));

  noteBox->setTitle(tr("Note"));
  notePaperColorLabel->setText(tr("Note paper color: "));
  notePaperColorReset->setText(tr("Reset"));
  noteFontColorLabel->setText(tr("Note font color: "));
  noteFontColorReset->setText(tr("Reset"));
  secretColorLabel->setText(tr("Secret color: "));
  secretColorReset->setText(tr("Reset"));
  refreshNotePaperImageLabel();
  notePaperImageLoad->setText(tr("Load..."));
  notePaperImageClear->setText(tr("Clear"));

  treeBox->setTitle(tr("Branches tree"));
  treeBackgroundLabel->setText(tr("Tree background: "));
  treeBackgroundReset->setText(tr("Reset"));
  treeTextLabel->setText(tr("Tree text color: "));
  treeTextReset->setText(tr("Reset"));
  treeFontSizeLabel->setText(tr("Tree font size: "));
  treeFontSize->setSpecialValueText(tr("Theme"));
  treeIndentationLabel->setText(tr("Tree indentation: "));
  treeIndentation->setSpecialValueText(tr("Theme"));

  tableBox->setTitle(tr("Notes list"));
  tableBackgroundLabel->setText(tr("List background: "));
  tableBackgroundReset->setText(tr("Reset"));
  tableTextLabel->setText(tr("List text color: "));
  tableTextReset->setText(tr("Reset"));
  tableAlternateLabel->setText(tr("Alternate row color: "));
  tableAlternateReset->setText(tr("Reset"));
  tableFontSizeLabel->setText(tr("List font size: "));
  tableFontSize->setSpecialValueText(tr("Theme"));
  tableRowHeightLabel->setText(tr("Row height: "));
  tableRowHeight->setSpecialValueText(tr("Theme"));

  fontBox->setTitle(tr("Interface font"));
  interfaceFontLabel->setText(tr("Font: "));
  interfaceFontReset->setText(tr("Reset"));
  refreshInterfaceFontButton();

  resetAllButton->setText(tr("Reset all to theme"));
}


// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Theme::applyChanges(void)
{
  qDebug() << "Apply changes theme";

  // Смена базовой темы: перечитывается полностью, кастом ляжет следом
  if ( mytetraConfig.getInterfaceTheme() !=
       fixedParameters.themesAvailableList[ themeNameComboBox->currentIndex() ] )
  {
    if ( themeNameComboBox->currentText()!="Unknown")
    {
      mytetraConfig.setInterfaceTheme(
        fixedParameters.themesAvailableList[ themeNameComboBox->currentIndex() ] );

      CssHelper::loadCurrentTheme();
    }
  }

  // Размер иконок
  QString iconSizeName;

  if ( iconSizeComboBox->currentIndex()>0 )
    iconSizeName = fixedParameters.interfaceIconSizeAvailableMap.keys()[ iconSizeComboBox->currentIndex()-1 ];

  if ( mytetraConfig.getInterfaceIconSize() != iconSizeName )
  {
    mytetraConfig.setInterfaceIconSize( iconSizeName );
    CssHelper::loadCurrentTheme();
  }

  // Цвета заметки: режим темы пишется пустотой
  if(notePaperColorIsTheme)
  {
    if(!mytetraConfig.get_notePaperColor().isEmpty())
      mytetraConfig.set_notePaperColor("");
  }
  else if(mytetraConfig.get_notePaperColor()!=notePaperColor->name())
    mytetraConfig.set_notePaperColor(notePaperColor->name());

  if(noteFontColorIsTheme)
  {
    if(!mytetraConfig.get_noteFontColor().isEmpty())
      mytetraConfig.set_noteFontColor("");
  }
  else if(mytetraConfig.get_noteFontColor()!=noteFontColor->name())
    mytetraConfig.set_noteFontColor(noteFontColor->name());

  if(mytetraConfig.get_secretColor()!=secretColor->name())
    mytetraConfig.set_secretColor(secretColor->name());

  // Мозаичный фон заметок
  if(mytetraConfig.get_themeCustomNotePaperImage()!=notePaperImagePath)
    mytetraConfig.set_themeCustomNotePaperImage(notePaperImagePath);

  // Цвета дерева и списка: пусто означает цвет темы
  if(treeBackgroundIsTheme)
  {
    if(!mytetraConfig.get_themeCustomTreeBackground().isEmpty())
      mytetraConfig.set_themeCustomTreeBackground("");
  }
  else if(mytetraConfig.get_themeCustomTreeBackground()!=treeBackground->name())
    mytetraConfig.set_themeCustomTreeBackground(treeBackground->name());

  if(treeTextIsTheme)
  {
    if(!mytetraConfig.get_themeCustomTreeText().isEmpty())
      mytetraConfig.set_themeCustomTreeText("");
  }
  else if(mytetraConfig.get_themeCustomTreeText()!=treeText->name())
    mytetraConfig.set_themeCustomTreeText(treeText->name());

  if(tableBackgroundIsTheme)
  {
    if(!mytetraConfig.get_themeCustomTableBackground().isEmpty())
      mytetraConfig.set_themeCustomTableBackground("");
  }
  else if(mytetraConfig.get_themeCustomTableBackground()!=tableBackground->name())
    mytetraConfig.set_themeCustomTableBackground(tableBackground->name());

  if(tableTextIsTheme)
  {
    if(!mytetraConfig.get_themeCustomTableText().isEmpty())
      mytetraConfig.set_themeCustomTableText("");
  }
  else if(mytetraConfig.get_themeCustomTableText()!=tableText->name())
    mytetraConfig.set_themeCustomTableText(tableText->name());

  if(tableAlternateIsTheme)
  {
    if(!mytetraConfig.get_themeCustomTableAlternate().isEmpty())
      mytetraConfig.set_themeCustomTableAlternate("");
  }
  else if(mytetraConfig.get_themeCustomTableAlternate()!=tableAlternate->name())
    mytetraConfig.set_themeCustomTableAlternate(tableAlternate->name());

  // Шрифт интерфейса
  if(mytetraConfig.get_themeCustomInterfaceFontFamily()!=interfaceFontFamily)
    mytetraConfig.set_themeCustomInterfaceFontFamily(interfaceFontFamily);

  if(mytetraConfig.get_themeCustomInterfaceFontSize()!=interfaceFontSize)
    mytetraConfig.set_themeCustomInterfaceFontSize(interfaceFontSize);

  // Размеры дерева и списка (-1 означает размер темы)
  if(mytetraConfig.get_themeCustomTreeFontSize()!=treeFontSize->value())
    mytetraConfig.set_themeCustomTreeFontSize(treeFontSize->value());

  if(mytetraConfig.get_themeCustomTreeIndentation()!=treeIndentation->value())
    mytetraConfig.set_themeCustomTreeIndentation(treeIndentation->value());

  if(mytetraConfig.get_themeCustomTableFontSize()!=tableFontSize->value())
    mytetraConfig.set_themeCustomTableFontSize(tableFontSize->value());

  if(mytetraConfig.get_themeCustomTableRowHeight()!=tableRowHeight->value())
    mytetraConfig.set_themeCustomTableRowHeight(tableRowHeight->value());

  // Применение без перезапуска: кастом поверх темы и цвета заметки
  CssHelper::applyCustomDesign();

  MetaEditor *editorScreen=find_object<MetaEditor>("editorScreen");

  if(editorScreen!=nullptr)
    editorScreen->applyNoteColors();

  return 0;
}
