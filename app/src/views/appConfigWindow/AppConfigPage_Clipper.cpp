#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QGroupBox>
#include <QToolButton>
#include <QCommonStyle>
#include <QWhatsThis>
#include <QCheckBox>
#include <QKeySequenceEdit>
#include <QPushButton>

#include "AppConfigPage_Clipper.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/ObjectHelper.h"
#include "views/mainWindow/MainWindow.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;


AppConfigPage_Clipper::AppConfigPage_Clipper(QWidget *parent) : ConfigPage(parent)
{
  setupUi();
  setupSignals();
  assembly();
}


AppConfigPage_Clipper::~AppConfigPage_Clipper()
{

}


void AppConfigPage_Clipper::setupUi(void)
{
  qDebug() << "Create clipper config page";

  // Веб-клиппер: вставка из буфера обмена в unsorted_notes по глобальному хоткею.
  // Сам хоткей работает только под X11; кнопка проверки — везде
  clipperEnable=new QCheckBox(this);
  clipperEnable->setText(tr("Web Clipper: paste clipboard to unsorted_notes by global hotkey (X11, Windows)"));
  clipperEnable->setChecked(mytetraConfig.get_clipperenable());

  clipperHotkeyEdit=new QKeySequenceEdit(this);
  clipperHotkeyEdit->setToolTip(tr("Global hotkey, for example Ctrl+Alt+V. Latin keys only."));
  clipperHotkeyEdit->setKeySequence(QKeySequence(mytetraConfig.get_clipperhotkey()));

  clipperStatusLabel=new QLabel(this);
  clipperStatusLabel->setWordWrap(true);

  clipperTestButton=new QPushButton(this);
  clipperTestButton->setText(tr("Clip now"));
  clipperTestButton->setToolTip(tr("Paste clipboard to unsorted_notes right now (works everywhere, no hotkey needed)"));
  clipperTestButton->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed, QSizePolicy::ToolButton));

  // Лимиты клиппера: сколько картинок забирать в одну заметку
  // и максимальный размер одной картинки. Пропущенные сверх лимитов
  // картинки остаются внешними ссылками, о чем клиппер сообщает сразу
  clipperMaxImages=new QSpinBox(this);
  clipperMaxImages->setMinimum(1);
  clipperMaxImages->setMaximum(1000);
  clipperMaxImages->setValue(mytetraConfig.get_clipperMaxImages());
  clipperMaxImages->setWhatsThis(tr("How many clipboard images go into one note. Extra images stay as external links, the clipper reports them at once."));
  clipperMaxImages->setToolTip(tr("Images per note, the rest stay as links"));

  clipperMaxImageSizeMb=new QSpinBox(this);
  clipperMaxImageSizeMb->setMinimum(1);
  clipperMaxImageSizeMb->setMaximum(100);
  clipperMaxImageSizeMb->setValue(mytetraConfig.get_clipperMaxImageSizeMb());
  clipperMaxImageSizeMb->setSuffix(tr(" MB"));
  clipperMaxImageSizeMb->setWhatsThis(tr("Images larger than this stay as external links instead of files."));
  clipperMaxImageSizeMb->setToolTip(tr("Larger images stay as links, not files"));

  // Знак вопроса у группы клиппера: виден сразу, клик мгновенно
  // показывает подсказку без задержки наведения
  clipperHelpButton=new QToolButton(this);
  QCommonStyle styleHelp;
  clipperHelpButton->setIcon(styleHelp.standardIcon(QStyle::SP_MessageBoxQuestion));
  clipperHelpButton->setToolTip(tr("How to use the clipper"));

  // Группировщик клиппера. Штатный заголовок пустой:
  // название рисуется своей строкой чтобы рядом встал знак вопроса
  clipperBox=new QGroupBox(this);
  clipperBox->setTitle(QString());
  clipperBox->setWhatsThis(tr("Clipper saves the OS clipboard into an unsorted_notes branch note.\nRun: mytetra --control --clipboard [--url]\nBind it to a global OS hotkey, for example: /path/to/start.sh --control --clipboard\nMyTetra must be running."));
  clipperBox->setToolTip(tr("Saves the OS clipboard into a note: mytetra --control --clipboard [--url]"));
}


void AppConfigPage_Clipper::setupSignals(void)
{
  connect(clipperHelpButton, &QToolButton::clicked, this, &AppConfigPage_Clipper::onClipperHelpButton);
  connect(clipperTestButton, &QPushButton::clicked, this, &AppConfigPage_Clipper::onClickedClipperTest);
}


void AppConfigPage_Clipper::assembly(void)
{
  QLabel *clipperMaxImagesLabel=new QLabel(tr("Maximum images per note:"), this);
  QLabel *clipperMaxImageSizeLabel=new QLabel(tr("Maximum size of one image:"), this);

  QVBoxLayout *clipperLayout=new QVBoxLayout;

  // Строка заголовка группы: название и знак вопроса рядом с ним
  QLabel *clipperTitleLabel=new QLabel(tr("Clipper"), this);
  QFont clipperTitleFont=clipperTitleLabel->font();
  clipperTitleFont.setBold(true);
  clipperTitleLabel->setFont(clipperTitleFont);

  QHBoxLayout *clipperTitleLayout=new QHBoxLayout;
  clipperTitleLayout->addWidget(clipperTitleLabel);
  clipperTitleLayout->addWidget(clipperHelpButton);
  clipperTitleLayout->addStretch();
  clipperLayout->addLayout(clipperTitleLayout);

  QFormLayout *clipperFormLayout=new QFormLayout;
  clipperFormLayout->addRow(clipperEnable);
  clipperFormLayout->addRow(tr("Hotkey:"), clipperHotkeyEdit);
  clipperFormLayout->addRow(tr("Status:"), clipperStatusLabel);
  clipperFormLayout->addRow(QString(), clipperTestButton);
  clipperFormLayout->addRow(clipperMaxImagesLabel, clipperMaxImages);
  clipperFormLayout->addRow(clipperMaxImageSizeLabel, clipperMaxImageSizeMb);
  clipperLayout->addLayout(clipperFormLayout);

  clipperBox->setLayout(clipperLayout);

  updateClipperStatus();

  // Собирается основной слой
  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addWidget(clipperBox);
  centralLayout->addStretch();

  // Основной слой устанавливается
  setLayout(centralLayout);
}


// Клик по знаку вопроса мгновенно показывает подсказку группы
// под кнопкой: ждать секунду наведения не нужно
void AppConfigPage_Clipper::onClipperHelpButton(void)
{
  QPoint showPoint=clipperHelpButton->mapToGlobal(QPoint(0, clipperHelpButton->height()));

  QWhatsThis::showText(showPoint, clipperBox->whatsThis(), clipperHelpButton);
}


void AppConfigPage_Clipper::updateClipperStatus(void)
{
  MainWindow *mainWindow=find_object<MainWindow>("mainwindow");
  if(mainWindow && mainWindow->getClipper())
    clipperStatusLabel->setText(mainWindow->getClipper()->backendStatus());
  else
    clipperStatusLabel->setText(tr("Clipper status is unavailable."));
}


void AppConfigPage_Clipper::onClickedClipperTest(void)
{
  // Применяем текущие значения полей, чтобы проверка шла по ним
  applyChanges();

  MainWindow *mainWindow=find_object<MainWindow>("mainwindow");
  if(mainWindow)
    mainWindow->runClipperNow();

  updateClipperStatus();
}


// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Clipper::applyChanges(void)
{
  qDebug() << "Apply changes clipper";

  int result=0;

  // Сохраняются лимиты клиппера
  if(mytetraConfig.get_clipperMaxImages()!=clipperMaxImages->value())
    mytetraConfig.set_clipperMaxImages(clipperMaxImages->value());

  if(mytetraConfig.get_clipperMaxImageSizeMb()!=clipperMaxImageSizeMb->value())
    mytetraConfig.set_clipperMaxImageSizeMb(clipperMaxImageSizeMb->value());

  // Веб-клиппер: включение и комбинация хоткея (перехват обновится
  // при закрытии настроек через Clipper::rereadSettings)
  if(mytetraConfig.get_clipperenable()!=clipperEnable->isChecked())
    mytetraConfig.set_clipperenable(clipperEnable->isChecked());

  if(mytetraConfig.get_clipperhotkey()!=clipperHotkeyEdit->keySequence().toString())
    mytetraConfig.set_clipperhotkey(clipperHotkeyEdit->keySequence().toString());

  return result;
}
