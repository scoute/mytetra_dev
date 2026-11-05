#ifndef APPCONFIGPAGE_SEARCH_H
#define APPCONFIGPAGE_SEARCH_H

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QRadioButton;
class QLabel;
class QPushButton;

class AppConfigPage_Search : public ConfigPage
{
    Q_OBJECT

public:
    AppConfigPage_Search(QWidget *parent = nullptr);
    virtual ~AppConfigPage_Search();

    int applyChanges(void);
  void retranslateUi(void) override;

  protected slots:

    void onFtsVacuumClicked(void);

  protected:

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    // Объединяющая рамка
    QGroupBox *findInNoteBox;

    QRadioButton *findBarRadio; // Поиск полоской внутри заметки
    QRadioButton *findWindowRadio; // Поиск отдельным окном

    // Ручное обслуживание FTS-индекса текстов
    QGroupBox *ftsIndexBox;
    QLabel *ftsIndexSizeLabel; // Текущий размер файла индекса
    QPushButton *ftsVacuumButton; // Уплотнить файл (VACUUM)

    // Текст размера файла индекса: обновляется при показе и после уплотнения
    void updateFtsSizeLabel(void);
};

#endif // APPCONFIGPAGE_SEARCH_H
