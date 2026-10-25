#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QWidget>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>


#include "EditorImageProperties.h"

#include "libraries/helpers/DiskHelper.h"


EditorImageProperties::EditorImageProperties(QWidget *parent) : QDialog(parent)
{
 imageRealWidth=0;
 imageRealHeight=0;

 // Флаг, используемый для запрещения цикличного взаимного изменения
 // высоты и ширины
 isRelateSizeSetted=false;

 // Истинный ресайз по умолчанию запрещен: кнопка скрыта.
 // Разрешается мастер-тумблером из настроек редактора
 trueResizeAllowed=false;
 trueResizeFileExists=false;
 trueResizeApplied=false;

 setup_ui();
 setup_signals();
 assembly();
}


EditorImageProperties::~EditorImageProperties()
{

}


void EditorImageProperties::setup_ui()
{
 infoLabel=new QLabel(" ");

 percentSizeLabel=new QLabel(tr("Size (%): "));

 percentSizeSpin=new QSpinBox;
 percentSizeSpin->setRange(1,500);
 percentSizeSpin->setValue(100);


 widthLabel=new QLabel(tr("Width (pixels): "));

 widthSpin=new QSpinBox;
 widthSpin->setRange(1,4096);
 widthSpin->setValue(100);


 heightLabel=new QLabel(tr("Height (pixels): "));

 heightSpin=new QSpinBox;
 heightSpin->setRange(1,4096);
 heightSpin->setValue(100);


 resetSizeButton=new QPushButton(tr("Reset to real image size"));
 resetSizeButton->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed, QSizePolicy::ToolButton));

 trueResizeButton=new QPushButton(tr("Apply size to file"));
 trueResizeButton->setObjectName(QStringLiteral("trueResizeButton"));
 trueResizeButton->setToolTip(tr("Rewrite the image file to the size above. The original is moved to trash."));
 trueResizeButton->setVisible(false);


 buttonBox=new QDialogButtonBox(Qt::Horizontal);
 buttonBox->addButton(tr("OK"),QDialogButtonBox::AcceptRole);
 buttonBox->addButton(tr("Cancel"),QDialogButtonBox::RejectRole);


 this->setWindowTitle(tr("Image properties"));
}


void EditorImageProperties::setup_signals()
{    
 connect(percentSizeSpin, qOverload<int>(&QSpinBox::valueChanged),
         this,            &EditorImageProperties::on_changed_percent);
 connect(widthSpin,       qOverload<int>(&QSpinBox::valueChanged),
         this,            &EditorImageProperties::on_changed_width);
 connect(heightSpin,      qOverload<int>(&QSpinBox::valueChanged),
         this,            &EditorImageProperties::on_changed_height);

 connect(resetSizeButton, &QPushButton::pressed, this, &EditorImageProperties::on_click_reset_size);

 connect(trueResizeButton, &QPushButton::pressed, this, &EditorImageProperties::on_click_true_resize);

 connect(buttonBox, &QDialogButtonBox::accepted, this, &EditorImageProperties::accept);
 connect(buttonBox, &QDialogButtonBox::rejected, this, &EditorImageProperties::reject);
}


void EditorImageProperties::assembly()
{
 QVBoxLayout *mainLayout=new QVBoxLayout(this);


 // Область запроса размеров картинки
 QGridLayout *gridLayout=new QGridLayout();

 gridLayout->addWidget(percentSizeLabel, 0, 0);
 gridLayout->addWidget(percentSizeSpin,  0, 1);

 gridLayout->addWidget(widthLabel, 1, 0);
 gridLayout->addWidget(widthSpin,  1, 1);

 gridLayout->addWidget(heightLabel, 2, 0);
 gridLayout->addWidget(heightSpin,  2, 1);

 // Кнопка сбора размера к настоящему размеру картинки
 gridLayout->addWidget(resetSizeButton, 3, 1);

 // Кнопка перезаписи файла под заданный выше размер.
 // Видна только при разрешении из настроек
 gridLayout->addWidget(trueResizeButton, 4, 1);

 // Добавляется область запроса размеров картинки на основной слой
 mainLayout->addLayout(gridLayout);


 // Добавляется строка с информацией о картинке
 QGroupBox *groupBox = new QGroupBox(tr("Image info"));
 QVBoxLayout *vbox = new QVBoxLayout;
 vbox->addWidget(infoLabel);
 groupBox->setLayout(vbox);
 mainLayout->addWidget(groupBox);


 // Добавляется линейка кнопок OK и Cancel на основной слой
 mainLayout->addWidget(buttonBox);
}


void EditorImageProperties::set_info(QString infoText)
{
 infoLabel->setText(infoText);
}


void EditorImageProperties::set_real_width(int width)
{
 imageRealWidth=width;
}


void EditorImageProperties::set_real_height(int height)
{
 imageRealHeight=height;
}


void EditorImageProperties::set_width(int width)
{
 isRelateSizeSetted=true;
 widthSpin->setValue(width);
 isRelateSizeSetted=false;
}


void EditorImageProperties::set_height(int height)
{
 isRelateSizeSetted=true;
 heightSpin->setValue(height);
 isRelateSizeSetted=false;
}


int EditorImageProperties::get_width(void)
{
 return widthSpin->value();
}


int EditorImageProperties::get_height(void)
{
 return heightSpin->value();
}


// Расчет размеров картинки в процентах на основании реальных размеров
// и заданных, и обновление виджета, отображающего проценты
void EditorImageProperties::update_percent(void)
{
 if(imageRealWidth==0 || imageRealHeight==0) return;

 int calculatePercent;

 // Расчет процента далается на основании более длинной стороны рисунка
 if(imageRealWidth>imageRealHeight)
  calculatePercent=(widthSpin->value()*100)/imageRealWidth;
 else
  calculatePercent=(heightSpin->value()*100)/imageRealHeight;

 isRelateSizeSetted=true;
 percentSizeSpin->setValue(calculatePercent);
 isRelateSizeSetted=false;
}


void EditorImageProperties::on_changed_percent(int n)
{
 // Запрещается "циклическое" изменение ширины и высоты
 if(isRelateSizeSetted) return;
 
 int calculateWidth;
 int calculateHeight;

 calculateWidth=(n*imageRealWidth)/100;
 calculateHeight=(n*imageRealHeight)/100;

 isRelateSizeSetted=true;
 widthSpin->setValue(calculateWidth);
 heightSpin->setValue(calculateHeight);
 isRelateSizeSetted=false;
}


void EditorImageProperties::on_changed_width(int n)
{
 if(imageRealWidth==0 || imageRealHeight==0) return;

 // Запрещается "циклическое" изменение ширины и высоты
 if(isRelateSizeSetted) return;

 // Новая высота
 int h=(n*imageRealHeight)/imageRealWidth;

 // Новый процент
 int percent=(h*100)/imageRealHeight;

 isRelateSizeSetted=true;
 heightSpin->setValue(h);
 percentSizeSpin->setValue(percent);
 isRelateSizeSetted=false;
}


void EditorImageProperties::on_changed_height(int n)
{
 if(imageRealWidth==0 || imageRealHeight==0) return;

 // Запрещается "циклическое" изменение ширины и высоты
 if(isRelateSizeSetted) return;

 // Новая ширина
 int w=(n*imageRealWidth)/imageRealHeight;

 // Новый процент
 int percent=(w*100)/imageRealWidth;

 isRelateSizeSetted=true;
 widthSpin->setValue(w);
 percentSizeSpin->setValue(percent);
 isRelateSizeSetted=false;
}

void EditorImageProperties::on_click_reset_size()
{
 set_width(imageRealWidth);
 set_height(imageRealHeight);
 update_percent();
}


void EditorImageProperties::setTrueResizeAllowed(bool allowed)
{
 trueResizeAllowed=allowed;
 trueResizeButton->setVisible(allowed);
}


void EditorImageProperties::setTrueResizeFile(const QString &fileName, bool fileExists)
{
 trueResizeFileName=fileName;
 trueResizeFileExists=fileExists;
}


void EditorImageProperties::setSourceImage(const QImage &image)
{
 trueResizeSource=image;
 trueResizeApplied=false;
}


bool EditorImageProperties::trueResizeDone(void) const
{
 return trueResizeApplied;
}


QImage EditorImageProperties::trueResizedImage(void) const
{
 return trueResizeResult;
}


// Чистая операция: ужать файл-картинку до размера с сохранением пропорций.
// Формат берется по расширению файла. Возвращает успех записи
bool EditorImageProperties::resizeImageFile(const QString &fileName, int width, int height)
{
 if(width<=0 || height<=0)
  return false;

 QImage source(fileName);

 if(source.isNull())
  return false;

 QImage scaled=source.scaled(width, height,
                             Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);

 if(scaled.isNull())
  return false;

 const bool isJpg=fileName.endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive);
 const char *format=isJpg ? "JPG" : "PNG";

 return scaled.save(fileName, format, isJpg ? 90 : -1);
}


void EditorImageProperties::on_click_true_resize()
{
 if(!trueResizeAllowed)
  return;

 const int targetWidth=widthSpin->value();
 const int targetHeight=heightSpin->value();

 if(targetWidth<=0 || targetHeight<=0 || trueResizeSource.isNull())
  return;

 // Страшное предупреждение с цифрами: размеры и вес файла,
 // судьба оригинала, отказ по умолчанию
 QString fromText=QString::number(imageRealWidth)+QStringLiteral("x")+
                  QString::number(imageRealHeight);
 QString toText=QString::number(targetWidth)+QStringLiteral("x")+
                QString::number(targetHeight);

 if(trueResizeFileExists)
  {
   const qint64 fileBytes=QFileInfo(trueResizeFileName).size();
   fromText+=(QStringLiteral(" (")+QString::number(fileBytes)+QStringLiteral(" ")+tr("bytes")+")");
  }

 QString question=tr("Rewrite the image to %1?").arg(toText)+QStringLiteral("\n")+
                  tr("Current: %1").arg(fromText)+QStringLiteral("\n")+
                  tr("Quality loss is irreversible!")+QStringLiteral(" ");

 if(trueResizeFileExists)
  question+=tr("The original file will be moved to trash.");
 else
  question+=tr("The file is not saved yet, only the image in the note will change.");

 QMessageBox askBox(QMessageBox::Warning,
                    tr("Change true image size"),
                    question,
                    QMessageBox::NoButton,
                    this);
 QPushButton *applyButton=askBox.addButton(tr("Apply"), QMessageBox::AcceptRole);
 QPushButton *cancelButton=askBox.addButton(QMessageBox::Cancel);
 askBox.setDefaultButton(cancelButton);
 askBox.exec();

 if(askBox.clickedButton()!=applyButton)
  return;

 // Пересчет пикселей из исходника диалога
 QImage scaled=trueResizeSource.scaled(targetWidth, targetHeight,
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);

 if(scaled.isNull())
  {
   QMessageBox::warning(this,
                        tr("Change true image size"),
                        tr("Can not scale image."));
   return;
  }

 // Оригинал файла — в корзину, затем запись нового файла.
 // Без файла на диске меняется только картинка в памяти
 if(trueResizeFileExists)
  {
   if(!DiskHelper::removeFileToTrash(trueResizeFileName, false))
    {
     QMessageBox::warning(this,
                          tr("Change true image size"),
                          tr("Can not move original to trash, nothing changed."));
     return;
    }

   const bool isJpg=trueResizeFileName.endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive);
   const char *format=isJpg ? "JPG" : "PNG";

   if(!scaled.save(trueResizeFileName, format, isJpg ? 90 : -1))
    {
     QMessageBox::warning(this,
                          tr("Change true image size"),
                          tr("Can not write resized file. The original stays in trash."));
     return;
    }
  }

 // Реальный размер стал заданным: проценты сходятся в 100,
 // источник для форматера обновлен
 trueResizeResult=scaled;
 trueResizeApplied=true;

 set_real_width(scaled.width());
 set_real_height(scaled.height());
 update_percent();
 set_info(tr("Real image size ")+
          QString::number(scaled.width())+QStringLiteral(" x ")+
          QString::number(scaled.height())+QStringLiteral(" ")+
          tr("pixels"));
}
