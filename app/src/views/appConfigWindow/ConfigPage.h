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

    // Откат несогласованных изменений при нажатии Cancel. Нужен страницам
    // которые применяют что-то живьем до закрытия диалога (предпросмотр)
    virtual void cancelChanges(void);
};


#endif // CONFIGPAGE_H
