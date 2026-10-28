#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

#include <QDialog>

class QDialogButtonBox;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QListWidgetItem;
class QScrollArea;
class QPushButton;
class QShowEvent;


class ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    ConfigDialog(QWidget * parent = nullptr);
    virtual ~ConfigDialog();

    QListWidgetItem *addWidget(QWidget *inswidget, QString name);

    void updateListWidth(void);
    void externalChangePage(QListWidgetItem *item);
    void setMenuListVisible(bool visible);

    // Кнопка перехода в смежные настройки (слева внизу):
    // из общих - в настройки редактора и наоборот
    void setAlterSettingsButton(const QString &text);

    // Применить как по OK (с возможным выходом на перезапуск) и закрыть.
    // Нужно прыжкам между диалогами: голый accept() обошел бы applyChanges,
    // который висит на кнопке OK, и правки молча потерялись бы
    void applyAndClose(void);

signals:
    void alterSettingsRequested(void);

private slots:    
    void applyChanges(void);
    void changePage(QListWidgetItem *current, QListWidgetItem *previous);

private:
    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    QListWidgetItem *createItems(QString name);

    // Доращивание ширины под видимую страницу (см. showEvent):
    // вызывается при показе и при каждом переключении страниц
    void widenToFitCurrentPage(void);

    // Одноразовое доращивание ширины при показе: страницы лежат
    // в скроллах, чей sizeHint не отражает контент, поэтому диалог
    // открывается уже имеющейся ширины и длинные страницы (клиппер,
    // вложения, шифрование) уезжают в горизонтальный скролл.
    // Меряется только видимая страница (у скрытых вьюпорт схлопнут
    // и maximum мусорный). Доращивание ровно на выступ делает окно
    // шире само, одинаково для всех тем
    void showEvent(QShowEvent *event) override;


    QListWidget *contentsWidget;
    QStackedWidget *pagesWidget;
    QDialogButtonBox *confirmButtons;
    QPushButton *alterSettingsButton;
};

#endif // CONFIGDIALOG_H
