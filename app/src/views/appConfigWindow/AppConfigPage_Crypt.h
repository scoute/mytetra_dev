#ifndef _APPCONFIGPAGE_CRYPT_H_
#define	_APPCONFIGPAGE_CRYPT_H_

#include "ConfigPage.h"

class QWidget;
class QLabel;
class QGroupBox;
class QRadioButton;
class QPushButton;
class QCheckBox;
class QSpinBox;
class MtComboBox;
class QPushButton;

class AppConfigPage_Crypt : public ConfigPage
{
 Q_OBJECT

public:
  AppConfigPage_Crypt(QWidget *parent = nullptr);
  virtual ~AppConfigPage_Crypt();

  int applyChanges(void);
  void retranslateUi(void);

private slots:

  void onPassRetrieveButtonClicked(void);
  void onAutoClosePasswordEnableToggle(bool checked);
  void onPasswordSaveEnableToggle(bool checked);
  void onCryptMethodChanged(int index);
  void onReencryptButtonClicked(void);

protected:

  QString getRetrieveStatusText(void);
  QString getRetrieveButtonText(void);
  QString getRetrieveAnnotationText(void);

  void setupUi(void);
  void updateUi(void);
  void setupSignals(void);
  void assembly(void);


  QGroupBox *passRetrieveBox;
  QLabel *passRetrieveStatus;
  QPushButton *passRetrieveButton;
  QLabel *passRetrieveAnnotation;

  QGroupBox *howPassRequestBox;
  QRadioButton *howPassRequestRadio1;
  QRadioButton *howPassRequestRadio2;

  QGroupBox *storePassRequestBox;
  QCheckBox *passwordSaveEnable;
  QLabel *passwordSaveAnnotation;


  QGroupBox *autoClosePasswordBox;
  QCheckBox *autoClosePasswordEnable;
  QSpinBox  *autoClosePasswordDelay;

  QGroupBox *decryptFileToTrashDirectoryBox;
  QCheckBox *decryptFileToTrashDirectoryEnable;

  QGroupBox *cryptMethodBox;
  MtComboBox *cryptMethodCombo;
  QPushButton *reencryptButton;

};


#endif	// _APPCONFIGPAGE_CRYPT_H_

