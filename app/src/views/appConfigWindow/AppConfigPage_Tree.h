#ifndef APPCONFIGPAGE_TREE_H
#define APPCONFIGPAGE_TREE_H

#include "ConfigPage.h"

class QWidget;
class QRadioButton;
class QGroupBox;

// Страница настроек дерева веток: живет после Notes area.
// Выбор вида счетчика записей: только свои [n] или общий [n][m]
class AppConfigPage_Tree : public ConfigPage
{
 Q_OBJECT

public:
  AppConfigPage_Tree(QWidget *parent = nullptr);
  virtual ~AppConfigPage_Tree(void);

  int applyChanges(void);
  void retranslateUi(void);

protected:

  void setupUi(void);
  void setupSignals(void);
  void assembly(void);

  // Только свои [n] (по умолчанию) либо общий итог [n][m] с подветками
  QRadioButton *ownCountOnly;
  QRadioButton *subtreeCount;

  QGroupBox *countsBox;
};


#endif // APPCONFIGPAGE_TREE_H
