#ifndef EDITORCONFIGDIALOG_H
#define EDITORCONFIGDIALOG_H

#include <QWidget>

class ConfigDialog;

class EditorConfigDialog : public QWidget
{
 Q_OBJECT

public:
 // page "misc" - открыться сразу на странице с прозрачностью содержания
 EditorConfigDialog(QWidget *parent=nullptr, const QString &page=QString());

private slots:
 void onAlterToAppSettings(void);
    
private:
 ConfigDialog *configDialog;
};

#endif // EDITORCONFIGDIALOG_H
