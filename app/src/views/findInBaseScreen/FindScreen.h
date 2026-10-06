#ifndef _FINDSCREEN_H_
#define	_FINDSCREEN_H_

#include <QWidget>
#include <QMap>
#include <QSet>
#include <QIcon>

class QStandardItemModel;
class QCompleter;
class QTimer;
class QShowEvent;

class QLineEdit;
class QPushButton;
class QToolButton;
class QHBoxLayout;
class QVBoxLayout;
class QGridLayout;
class QLabel;
class QCheckBox;
class QProgressDialog;

class KnowTreeModel;
class TreeItem;

class FindTableWidget;

class MtComboBox;

// Виджет поиска по базе


class FindScreen : public QWidget
{
    Q_OBJECT

public:
    FindScreen(QWidget *parent=nullptr);
    virtual ~FindScreen(void);

public slots:

    void widgetShow(void);
    void widgetHide(void);
    void findClicked(void);
    void setFindText(QString text);

    // Разбить поле тегов записи на отдельные теги: разделители запятая
    // и точка с запятой, пробелы по краям отбрасываются
    static QStringList splitRecordTags(const QString &tagsField);

    // Рекурсивный сбор значений полей ветки и всех подветок в словари.
    // Ключи словарей совпадают с именами полей поиска. Зашифрованные
    // ветки без пароля пропускаются как и в самом поиске
    static void collectBranchValues(const TreeItem *curritem,
                                    QMap<QString, QStringList> &dictionaries,
                                    QSet<QString> &seen);

    // Добавить слово в словарь поля. Пустые значения отбрасываются,
    // повторы без учета регистра тоже: пишется первое встречное
    // написание. Дедуп в пределах поля: одно и то же слово из разных
    // полей показывается с каждой своей меткой типа
    static void addDictionaryWord(QMap<QString, QStringList> &dictionaries,
                                   QSet<QString> &seen,
                                   const QString &field,
                                   const QString &word);

    // Иконка типа значения для выпадашки. Рисуется из ресурсов,
    // от шрифтов системы не зависит
    static QIcon completionTypeIcon(const QString &field);


private slots:

    void enableFindButton(const QString &text);
    void toolsExpandClicked(void);

    void changedWordRegard(int pos);
    void changedHowExtract(int pos);
    void changedTreeSearchArea(int pos);

    void changedFindInName(int state);
    void changedFindInAuthor(int state);
    void changedFindInUrl(int state);
    void changedFindInTags(int state);
    void changedFindInText(int state);
    void changedFindInNameItem(int state);
    void changedFindInAttach(int state);

    void onFindTextEdited(const QString &text);
    void onFieldCompletion(const QString &completion);
    void refreshFieldCompleter(void);

    // Метаданные дерева изменились: отложить пересборку словаря подсказок
    void onTreeMetadataSaved(void);

signals:

    // Сигнал вырабатывается, когда обнаружено что в слоте setFindText()
    // был изменен текст для поиска
    void textChangedFromAnother(const QString&);

    void findClickedAfterAnotherTextChanged(void);

private:

    QHBoxLayout *toolsAreaFindTextAndButton;
    QLineEdit *findText;
    QPushButton *findStartButton;
    QToolButton *toolsExpand;

    QVBoxLayout *toolsAreaCloseButton;
    QToolButton *closeButton;

    QHBoxLayout *toolsAreaComboOption;
    MtComboBox *wordRegard;
    MtComboBox *howExtract;
    MtComboBox *treeSearchArea;

    QHBoxLayout *whereFindLine;
    QLabel *whereFindLabel;
    QCheckBox *findInName;
    QCheckBox *findInAuthor;
    QCheckBox *findInUrl;
    QCheckBox *findInTags;
    QCheckBox *findInText;
    QCheckBox *findInNameItem;
    QCheckBox *findInAttach;

    QHBoxLayout *toolsLine;
    QGridLayout *toolsGrid;

    QVBoxLayout *centralDesktopLayout;

    FindTableWidget *findTable;

    QProgressDialog *progress;

    // Автодополнение строки запроса словами из отмеченных полей.
    // Модель с иконками типов, привод ручной: QLineEdit ищет совпадение
    // всей строки и подсказка после пробела не появилась бы
    QStandardItemModel *fieldCompleterModel=nullptr;
    QCompleter *fieldCompleter=nullptr;
    bool fieldCompleterEnabled=false;

    // Пересборка словаря подсказок откладывается таймером, чтобы пакетная
    // операция не пересобирала его на каждый шаг
    QTimer *completerRefreshTimer=nullptr;
    bool treeMetadataConnected=false;

    void setupFieldCompleter(void);

protected:

    // Словарь подсказок пересобирается при каждом показе любым путем:
    // widgetShow(), прямой show() при восстановлении состояния на старте
    void showEvent(QShowEvent *event) override;

    void setupFindTextAndButton(void);
    void assemblyFindTextAndButton(void);

    void setupComboOption(void);
    void assemblyComboOption(void);

    void setupCloseButton(void);
    void assemblyCloseButton(void);

    void setupWhereFindLine(void);
    void assemblyWhereFindLine(void);

    void setupUI(void);
    void assembly(void);

    void setupSignals(void);

    void changedFindInField(QString fieldname, int state);

    void findStart(void);
    void findRecurse(const TreeItem* curritem);
    bool findInTextProcess(const QString& text);

    void switchToolsExpand(bool flag);

    QStringList textDelimiterDecompose(QString text);


    // Поля, где нужно искать (Заголовок, текст, теги...)
    QMap<QString, bool> searchArea;

    // Список слов, которые нужно искать
    QStringList searchWordList;

    int totalProgressCounter;

    int cancelFlag;

    bool isUnsearchCryptBranchPresent; // Флаг, определяющий, были ли непросмотренные ветки при поиске (зашированные ветки, но пароль небыл введен)
};

#endif	/* _FINDSCREEN_H_ */

