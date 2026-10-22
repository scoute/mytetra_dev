#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QString>
#include <QLabel>
#include <QGroupBox>
#include <QRadioButton>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QMessageBox>
#include <QProgressDialog>
#include "libraries/helpers/ObjectHelper.h"
#include "views/tree/TreeScreen.h"
#include "libraries/crypt/CryptMigrate.h"

#include "AppConfigPage_Crypt.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "libraries/crypt/Password.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;
extern DataBaseConfig dataBaseConfig;


AppConfigPage_Crypt::AppConfigPage_Crypt(QWidget *parent) : ConfigPage(parent)
{
  qDebug() << "Create crypt config page";

  setupUi();
  updateUi();
  setupSignals();
  assembly();
}


AppConfigPage_Crypt::~AppConfigPage_Crypt()
{
 
}


void AppConfigPage_Crypt::setupUi(void)
{
  // Группировщик виджетов работы с паролем
  passRetrieveBox=new QGroupBox(this);
  passRetrieveBox->setTitle(tr("Password settings"));

  // Строка состояния пароля
  passRetrieveStatus=new QLabel(this);
  passRetrieveStatus->setWordWrap(true);

  // Кнопка запроса введения пароля
  passRetrieveButton=new QPushButton(this);
  passRetrieveButton->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed, QSizePolicy::ToolButton));

  // Строка с аннотацией
  passRetrieveAnnotation=new QLabel(this);
  passRetrieveAnnotation->setWordWrap(true);

  // Виджеты вставляются в группировщик
  QVBoxLayout *passRetrieveLayout = new QVBoxLayout;
  passRetrieveLayout->addWidget(passRetrieveStatus);
  passRetrieveLayout->addWidget(passRetrieveButton);
  passRetrieveLayout->addWidget(passRetrieveAnnotation);
  passRetrieveBox->setLayout(passRetrieveLayout);


  // Группировщик виджетов выбора, как запрашивать пароль
  howPassRequestBox=new QGroupBox(this);
  howPassRequestBox->setTitle(tr("Access to encrypted data"));

  howPassRequestRadio1=new QRadioButton(tr("Ask the password when you click on an encrypted item"));
  howPassRequestRadio2=new QRadioButton(tr("Ask the password at MyTetra startup"));

  // Точка устанавливается возле того пункта, который настроен в конфиге
  if(mytetraConfig.get_howpassrequest()=="atClickOnCryptBranch")
   howPassRequestRadio1->setChecked(true);
  else
   howPassRequestRadio2->setChecked(true);


  storePassRequestBox=new QGroupBox(this);
  storePassRequestBox->setTitle(tr("Store password"));

  passwordSaveEnable=new QCheckBox(tr("Store password locally"));

  passwordSaveAnnotation=new QLabel(tr("Password will be saved at first next entered.<br>Stored password will be cleared if uncheck this checkbox."));

  if(mytetraConfig.getPasswordSaveFlag())
   {
    howPassRequestRadio1->setEnabled(false);
    howPassRequestRadio2->setEnabled(false);
    passwordSaveEnable->setChecked(true);
   }
  else
   {
    howPassRequestRadio1->setEnabled(true);
    howPassRequestRadio2->setEnabled(true);
    passwordSaveEnable->setChecked(false);
   }


  // Виджеты вставляются в группировщики
  QVBoxLayout *howPassRequestLayout=new QVBoxLayout;
  howPassRequestLayout->addWidget(howPassRequestRadio1);
  howPassRequestLayout->addWidget(howPassRequestRadio2);
  howPassRequestLayout->addWidget(passwordSaveEnable);
  howPassRequestBox->setLayout(howPassRequestLayout);


  QVBoxLayout *storePassRequestLayout=new QVBoxLayout;
  storePassRequestLayout->addWidget(passwordSaveEnable);
  storePassRequestLayout->addWidget(passwordSaveAnnotation);
  storePassRequestBox->setLayout(storePassRequestLayout);



  // Группировщик настойки автозакрытия окна пароля
  autoClosePasswordBox=new QGroupBox(this);
  autoClosePasswordBox->setTitle(tr("Auto closing password window"));

  autoClosePasswordEnable=new QCheckBox(tr("Enable auto closing password window, sec"), this);
  
  autoClosePasswordDelay=new QSpinBox(this);
  autoClosePasswordDelay->setValue(mytetraConfig.get_autoClosePasswordDelay());
  autoClosePasswordDelay->setRange(1, 999);

  // Устанавливается галка и активность виджета выбора задержки
  onAutoClosePasswordEnableToggle( mytetraConfig.get_autoClosePasswordEnable() );

  // Виджеты вставляются в группировщик
  QHBoxLayout *autoClosePasswordLayout=new QHBoxLayout;
  autoClosePasswordLayout->addWidget(autoClosePasswordEnable);
  autoClosePasswordLayout->addWidget(autoClosePasswordDelay);
  autoClosePasswordLayout->addStretch();
  autoClosePasswordBox->setLayout(autoClosePasswordLayout);


  // Разрешать ли расшифровывать зашифрованные файлы аттачей в каталог корзины при предпросмотре
  decryptFileToTrashDirectoryBox=new QGroupBox(this);
  decryptFileToTrashDirectoryBox->setTitle(tr("File decrypt method for attach preview"));

  decryptFileToTrashDirectoryEnable=new QCheckBox(tr("Enable temporary decrypt attach file to trash directory"));

  // Устанавливается галка согласно настройке из файла конфигурации
  decryptFileToTrashDirectoryEnable->setChecked( mytetraConfig.getEnableDecryptFileToTrashDirectory() );

  // Виджеты вставляются в группировщик
  QHBoxLayout *decryptFileToTrashDirectoryLayout=new QHBoxLayout;
  decryptFileToTrashDirectoryLayout->addWidget(decryptFileToTrashDirectoryEnable);
  decryptFileToTrashDirectoryBox->setLayout(decryptFileToTrashDirectoryLayout);


  // Группировщик выбора шифра новых данных.
  // Старые данные читаются всегда: расшифровка отличает шифры
  // по магии блоба, настройка влияет только на шифрование
  cryptMethodBox=new QGroupBox(this);
  cryptMethodBox->setTitle(tr("Encryption method"));

  cryptMethodCombo=new QComboBox(this);
  cryptMethodCombo->addItem(tr("RC5 (compatible)"), QStringLiteral("rc5"));
  cryptMethodCombo->addItem(tr("XChaCha20-Poly1305 (modern)"), QStringLiteral("xchacha"));

  reencryptButton=new QPushButton(this);
  reencryptButton->setText(tr("Re-encrypt data with selected method"));

  QHBoxLayout *cryptMethodLayout=new QHBoxLayout;
  cryptMethodLayout->addWidget(cryptMethodCombo);
  cryptMethodLayout->addWidget(reencryptButton);
  cryptMethodLayout->addStretch();
  cryptMethodBox->setLayout(cryptMethodLayout);
}


void AppConfigPage_Crypt::updateUi(void)
{
  // Строка состояния пароля
  passRetrieveStatus->setText(getRetrieveStatusText());

  // Кнопка запроса введения пароля
  passRetrieveButton->setText(getRetrieveButtonText());

  // Строка с аннотацией
  passRetrieveAnnotation->setText(getRetrieveAnnotationText());

  // Выбранный шифр из конфига
  const int methodIndex=cryptMethodCombo->findData(mytetraConfig.get_cryptMethod());

  cryptMethodCombo->blockSignals(true);
  cryptMethodCombo->setCurrentIndex(methodIndex>=0 ? methodIndex : 0);
  cryptMethodCombo->blockSignals(false);
}


void AppConfigPage_Crypt::setupSignals(void)
{
 // При нажатии кнопки работы с паролем
 connect(passRetrieveButton, &QPushButton::clicked,
         this, &AppConfigPage_Crypt::onPassRetrieveButtonClicked);

 // При клике на галку разрешения автозакрытия пароля
 connect(autoClosePasswordEnable, &QCheckBox::toggled,
         this, &AppConfigPage_Crypt::onAutoClosePasswordEnableToggle);

 connect(passwordSaveEnable, &QCheckBox::toggled,
         this, &AppConfigPage_Crypt::onPasswordSaveEnableToggle);

 // Смена шифра применяется сразу, действует на новые шифрования
 connect(cryptMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
         this, &AppConfigPage_Crypt::onCryptMethodChanged);

 connect(reencryptButton, &QPushButton::clicked,
         this, &AppConfigPage_Crypt::onReencryptButtonClicked);
}


void AppConfigPage_Crypt::assembly(void)
{
  // Собирается основной слой
  QVBoxLayout *central_layout=new QVBoxLayout();
  central_layout->addWidget(passRetrieveBox);
  central_layout->addWidget(howPassRequestBox);
  central_layout->addWidget(storePassRequestBox);
  central_layout->addWidget(autoClosePasswordBox);
  central_layout->addWidget(decryptFileToTrashDirectoryBox);
  central_layout->addWidget(cryptMethodBox);
  central_layout->addStretch();

  // Основной слой устанавливается
  setLayout(central_layout);
}


QString AppConfigPage_Crypt::getRetrieveStatusText(void)
{
 QString status=tr("<b>Status:</b> ");

 // Если в хранилище данных вообще не задан пароль
 if(dataBaseConfig.get_crypt_mode()==0)
  status=status+tr("No password is set. ");
 else
  status=status+tr("Password is set. ");

 // Если пароль (точнее хеш пароля) хранится локально
 if(mytetraConfig.getPasswordSaveFlag() &&
    mytetraConfig.getPasswordMiddleHash().length()>0)
  status=status+tr("Password is saved locally. ");

 return status;
}


QString AppConfigPage_Crypt::getRetrieveButtonText(void)
{
 // Если в хранилище данных вообще не задан пароль
 if(dataBaseConfig.get_crypt_mode()==0)
  return tr("Set a password");
 else
  return tr("Change password");
}


QString AppConfigPage_Crypt::getRetrieveAnnotationText(void)
{
 // Если в хранилище данных вообще не задан пароль
 if(dataBaseConfig.get_crypt_mode()==0)
  return tr("A password will be used to encrypt the item that you selected. Use \"Encrypt item\" or \"Decrypt item\" in context menu.");
 else
  return tr("If you change your password all encrypted item will be re-encrypted with a new password.");
}


// Действия при нажатии кнопки работы с паролем
void AppConfigPage_Crypt::onPassRetrieveButtonClicked(void)
{
 // Если в хранилище данных вообще не задан пароль
 if(dataBaseConfig.get_crypt_mode()==0)
  {
   // Включается диалог запроса пароля "с нуля"

   Password password;

   if(password.retrievePassword()==false) return;

   updateUi();
   return;
  }
 else
  {
   // Включается диалог изменения пароля
   Password password;

   password.replacePassword();
  }
 
}


void AppConfigPage_Crypt::onPasswordSaveEnableToggle(bool checked)
{
 if(checked)
  {
   howPassRequestRadio1->setEnabled(false);
   howPassRequestRadio2->setEnabled(false);
  }
 else
  {
   howPassRequestRadio1->setEnabled(true);
   howPassRequestRadio2->setEnabled(true);
  }
}


// Действие при клике на галку настройки автоматического закрытия окна пароля
void AppConfigPage_Crypt::onAutoClosePasswordEnableToggle(bool checked)
{
 // Устанавливается галка и активность виджета выбора задержки
 if(checked)
  {
   autoClosePasswordEnable->setChecked(true);
   autoClosePasswordDelay->setEnabled(true);
  }
 else
  {
   autoClosePasswordEnable->setChecked(false);
   autoClosePasswordDelay->setEnabled(false);
  }
}


void AppConfigPage_Crypt::onCryptMethodChanged(int index)
{
  const QString method=cryptMethodCombo->itemData(index).toString();

  if(!method.isEmpty())
    mytetraConfig.set_cryptMethod(method);
}


void AppConfigPage_Crypt::onReencryptButtonClicked(void)
{
  if(globalParameters.getCryptKey().isEmpty())
  {
    QMessageBox::information(this,
                             tr("Re-encrypt data"),
                             tr("Crypt is locked. Enter the password first."));
    return;
  }

  QMessageBox::StandardButton answer=QMessageBox::question(
    this,
    tr("Re-encrypt data"),
    tr("All encrypted data will be re-encrypted with the selected method \"%1\". Back up your database directory first. Continue?").arg(cryptMethodCombo->currentText()),
    QMessageBox::Yes | QMessageBox::No,
    QMessageBox::No);

  if(answer!=QMessageBox::Yes)
    return;

  TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

  if(treeScreen==nullptr || treeScreen->knowTreeModel==nullptr)
  {
    QMessageBox::warning(this,
                         tr("Re-encrypt data"),
                         tr("Database tree is unavailable."));
    return;
  }

  QProgressDialog progress(tr("Re-encrypting..."),
                           tr("Cancel"), 0, 0, this);
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(0);

  CryptMigrateResult result=CryptMigrate::reencryptAll(treeScreen->knowTreeModel,
                                                      &progress);

  progress.close();

  if(result.canceled)
    QMessageBox::information(this,
                             tr("Re-encrypt data"),
                             tr("Re-encryption canceled, converted branches: %1. Remaining data keeps working.").arg(result.branches));
  else
    QMessageBox::information(this,
                             tr("Re-encrypt data"),
                             tr("Done. Re-encrypted branches: %1.").arg(result.branches));
}


void AppConfigPage_Crypt::retranslateUi(void)
{
  passRetrieveBox->setTitle(tr("Password settings"));

  howPassRequestBox->setTitle(tr("Access to encrypted data"));
  howPassRequestRadio1->setText(tr("Ask the password when you click on an encrypted item"));
  howPassRequestRadio2->setText(tr("Ask the password at MyTetra startup"));

  storePassRequestBox->setTitle(tr("Store password"));
  passwordSaveEnable->setText(tr("Store password locally"));
  passwordSaveAnnotation->setText(tr("Password will be saved at first next entered.<br>Stored password will be cleared if uncheck this checkbox."));

  autoClosePasswordBox->setTitle(tr("Auto closing password window"));
  autoClosePasswordEnable->setText(tr("Enable auto closing password window, sec"));

  decryptFileToTrashDirectoryBox->setTitle(tr("File decrypt method for attach preview"));
  decryptFileToTrashDirectoryEnable->setText(tr("Enable temporary decrypt attach file to trash directory"));

  cryptMethodBox->setTitle(tr("Encryption method"));
  cryptMethodCombo->setItemText(0, tr("RC5 (compatible)"));
  cryptMethodCombo->setItemText(1, tr("XChaCha20-Poly1305 (modern)"));
  reencryptButton->setText(tr("Re-encrypt data with selected method"));

  // Динамическая строка состояния перечитывается текущим языком
  updateUi();
}


// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Crypt::applyChanges(void)
{
 qDebug() << "Apply changes crypt";

 if(howPassRequestRadio1->isChecked() &&
    mytetraConfig.get_howpassrequest()=="atStartProgram")
  mytetraConfig.set_howpassrequest("atClickOnCryptBranch");

 if(howPassRequestRadio2->isChecked() &&
    mytetraConfig.get_howpassrequest()=="atClickOnCryptBranch")
  mytetraConfig.set_howpassrequest("atStartProgram");

 if(autoClosePasswordEnable->isChecked()!=mytetraConfig.get_autoClosePasswordEnable())
  mytetraConfig.set_autoClosePasswordEnable( autoClosePasswordEnable->isChecked() );

 if(autoClosePasswordDelay->value()!=mytetraConfig.get_autoClosePasswordDelay())
  mytetraConfig.set_autoClosePasswordDelay( autoClosePasswordDelay->value() );

 if(passwordSaveEnable->isChecked()!=mytetraConfig.getPasswordSaveFlag())
  {
   mytetraConfig.setPasswordSaveFlag( passwordSaveEnable->isChecked() );
   
   // Если галка установлена что хранить локально пароль не нужно
   if(!passwordSaveEnable->isChecked())
    {
     // Промежуточный хеш пароля удаляется 
     mytetraConfig.setPasswordMiddleHash("");
    }
  }

 if(decryptFileToTrashDirectoryEnable->isChecked()!=mytetraConfig.getEnableDecryptFileToTrashDirectory())
   mytetraConfig.setEnableDecryptFileToTrashDirectory( decryptFileToTrashDirectoryEnable->isChecked() );

 return 0;
}
