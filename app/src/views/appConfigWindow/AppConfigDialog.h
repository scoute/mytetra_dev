#ifndef APPCONFIGDIALOG_H
#define APPCONFIGDIALOG_H

#include <QString>
#include <QList>

// Объект, подготавливающий и отображающий диалог настройки MyTetra

class ConfigDialog;
class QListWidgetItem;
class QWidget;

class AppConfigDialog
{

public:
    explicit AppConfigDialog(const QString &firstPageName, QWidget *parent);
    virtual ~AppConfigDialog();

    void exec();

    void setMenuListVisible(bool visible);

    // Обновить прямыми текстами заголовок, список страниц и сами страницы
    void retranslateUi(void);

    // Открытые диалоги настроек: смена языка из страницы Main
    // переводит их на лету через retranslateUi
    static QList<AppConfigDialog *> openDialogs;
    
private:
    ConfigDialog *configDialog;

    QListWidgetItem *pageMain;
    QListWidgetItem *pageAppearance;
    QListWidgetItem *pageTheme;
    QListWidgetItem *pageCrypt;
    QListWidgetItem *pageSynchro;
    QListWidgetItem *pageRecordTable;
    QListWidgetItem *pageTree;
    QListWidgetItem *pageAttach;
    QListWidgetItem *pageKeyboard;
    QListWidgetItem *pageHistory;
    QListWidgetItem *pageSearch;
    QListWidgetItem *pageFavorites;
    QListWidgetItem *pageClipper;
    QListWidgetItem *pageMcp;
    QListWidgetItem *pageVoice;
    QListWidgetItem *pageMisc;

    void changePage(QString name);
};

#endif // APPCONFIGDIALOG_H
