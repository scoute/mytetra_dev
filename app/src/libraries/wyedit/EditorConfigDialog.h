#ifndef EDITORCONFIGDIALOG_H
#define EDITORCONFIGDIALOG_H

#include <QWidget>

class ConfigDialog;

class EditorConfigDialog : public QWidget
{
 Q_OBJECT

public:
 EditorConfigDialog(QWidget *parent=nullptr);

private slots:
 void onAlterToAppSettings(void);
    
private:
 ConfigDialog *configDialog;
};

#endif // EDITORCONFIGDIALOG_H
