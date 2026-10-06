#ifndef _EDITORIMAGEOPENDIALOG_H_
#define _EDITORIMAGEOPENDIALOG_H_

#include <QDialog>
#include <QMap>

// Диалог выбора программы для открытия изображения.
// Список собирается из .desktop-файлов, умеющих картинки (только Linux),
// плюс кнопка обзора для произвольного исполнимого файла

class QListWidget;
class QPushButton;
class QDialogButtonBox;
class QListWidgetItem;

class EditorImageOpenDialog : public QDialog
{
    Q_OBJECT

public:

    EditorImageOpenDialog(const QString &fileName, QWidget *parent=nullptr);
    virtual ~EditorImageOpenDialog(void);

    // Выбранная команда запуска без файла: путь к бинарю или очищенный
    // Exec из .desktop. Пусто если диалог отклонен
    QString selectedProgram(void) const;

    // Программы из .desktop, умеющие открывать картинки.
    // Ключ - показываемое имя, значение - команда запуска без файла
    static QMap<QString, QString> availableImagePrograms(void);

private slots:

    void onBrowseClicked(void);
    void onItemDoubleClicked(QListWidgetItem *item);

    // Запомнить выбранную программу перед закрытием
    void accept(void);

private:

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    // Добавить программу в список и выделить ее
    void addProgram(const QString &displayName, const QString &command);

    // Убрать из Exec .desktop коды полей (%f, %U и прочие)
    static QString cleanExecCommand(const QString &exec);

    // Имя программы по умолчанию для картинок, пусто если не выяснено
    static QString defaultImageProgram(void);

    QString imageFileName;
    QString chosenProgram;

    QListWidget *programsList;
    QPushButton *browseButton;
    QDialogButtonBox *buttonBox;
};

#endif /* _EDITORIMAGEOPENDIALOG_H_ */
