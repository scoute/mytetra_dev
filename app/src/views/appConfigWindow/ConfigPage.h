#ifndef CONFIGPAGE_H
#define CONFIGPAGE_H

#include <QWidget>


class ConfigPage : public QWidget
{
    Q_OBJECT

public:
    ConfigPage(QWidget *parent = nullptr);
    virtual ~ConfigPage();

    virtual int applyChanges(void);

    // Обновить прямые тексты новым языком. По умолчанию нечего:
    // переопределяют страницы со статичными надписями
    virtual void retranslateUi(void) {}
};


#endif // CONFIGPAGE_H
