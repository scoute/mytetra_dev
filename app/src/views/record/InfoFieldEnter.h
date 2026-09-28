#ifndef INFOFIELDENTER_H_
#define INFOFIELDENTER_H_

#include <QWidget>

// Виджет ввода инфополей Title, Author, Url, Tags...

class QLabel;
class QLineEdit;
class QPushButton;
class QGridLayout;
class QToolButton;
class QCompleter;
class QStringListModel;
class QShowEvent;

class InfoFieldEnter : public QWidget
{
    Q_OBJECT

public:
    InfoFieldEnter(QWidget *parent=nullptr);
    ~InfoFieldEnter();

    void setFocusToStart(void);

    bool checkFieldName(QString name);
    QString getField(QString name);
    void setField(QString name,QString value);

    void setReadOnly(bool state);
    bool isReadOnly();

    // Последний недопечатанный тег: хвост после крайней запятой
    // или точки с запятой. Разделитель в конце значит тег допечатан
    static QString lastTagToken(const QString &text);

    // Подставить выбранное дополнение вместо последнего тега.
    // В конец добавляется запятая с пробелом для следующего тега
    static QString applyTagCompletion(const QString &text, const QString &completion);

    // Срезать висячий хвост разделителей в конце строки
    static QString trimTagTail(const QString &text);

public slots:

    void expandInfoClick(void);

private slots:

    void onTagsEdited(const QString &text);
    void onTagCompletion(const QString &completion);
    void onTagsReturn(void);

private:

    // Ввод названия записи
    QLabel    *recordNameLabel;
    QLineEdit *recordName;

    // Ввод автора
    QLabel    *recordAuthorLabel;
    QLineEdit *recordAuthor;

    // Ввод Url
    QLabel    *recordUrlLabel;
    QLineEdit *recordUrl;

    // Ввод текстовых меток
    QLabel    *recordTagsLabel;
    QLineEdit *recordTags;

    // Кнопка, раскрывающая и скрывающая поля author, url, tags
    QToolButton *expandInfo;

    // Размещалка элементов
    QGridLayout *infoFieldLayout;

    void setup_ui(void);
    void setup_signals(void);
    void assembly(void);

    void expandInfoOnDisplay(QString expand);

    // Подсказка тегов из базы. Словарь собирается при каждом показе:
    // теги могли измениться. Выпадашка с двух букв, не больше десяти
    // строк, без совпадений прячется
    void setupTagsCompleter(void);
    void refreshTagsCompleter(void);

    // Подсказка автодополнения и ее словарь. Комплитер живет все время,
    // на поле тегов вешается только когда есть что подсказывать
    QCompleter *tagsCompleter;
    QStringListModel *tagsCompleterModel;

protected:

    void showEvent(QShowEvent *event) override;

};

#endif /* INFOFIELDENTER_H_ */
