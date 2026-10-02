#ifndef _FINDSCREEN_H_
#define	_FINDSCREEN_H_

#include <QWidget>
#include <QMap>
#include <QSet>
#include <QStringList>

class QLineEdit;
class QPushButton;
class QToolButton;
class QHBoxLayout;
class QVBoxLayout;
class QGridLayout;
class QLabel;
class QCheckBox;
class QProgressDialog;
class QStandardItemModel;
class QShowEvent;
class QCompleter;
class QTimer;

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

    // Разбивка запроса в режиме подстроки: режется только по пробелам,
    // дефисы и прочие знаки остаются внутри слов ("333-1" ищется целиком).
    // Двойные кавычки как в textDelimiterDecompose: фраза в кавычках одно слово
    static QStringList splitQuerySubstring(const QString &query);

    // Рекурсивный сбор значений полей ветки и всех подветок в словари.
    // Ключи словарей совпадают с именами полей поиска. Зашифрованные
    // ветки без пароля пропускаются как и в самом поиске
    static void collectBranchValues(const TreeItem *curritem,
                                    QMap<QString, QStringList> &dictionaries,
                                    QSet<QString> &seen);

    // Добавить слово в словарь поля. Пустые значения и повторы
    // без учета регистра отбрасываются. Дедуп в пределах поля:
    // одно и то же слово из разных полей показывается с каждой
    // своей меткой типа
    static void addDictionaryWord(QMap<QString, QStringList> &dictionaries,
                                   QSet<QString> &seen,
                                   const QString &field,
                                   const QString &word);

    // Иконка типа значения для выпадашки. Рисуется из ресурсов,
    // от шрифтов системы не зависит (эмодзи там превращались в квадраты)
    static QIcon completionTypeIcon(const QString &field);

public slots:

    void widgetShow(void);
    void widgetHide(void);
    void findClicked(void);
    void setFindText(QString text);

public:

    // Разбить поле тегов записи на отдельные теги: разделители запятая
    // и точка с запятой, пробелы по краям отбрасываются
    static QStringList splitRecordTags(const QString &tagsField);

    // Совпадение слов запроса с тегами записи. Теги атомарны: каждое слово
    // запроса должно совпасть с ЦЕЛЫМ тегом без учета регистра, подстрока
    // внутри тега совпадением не считается. В matchCount возвращается число
    // совпавших пар слово-тег для столбца совпадений. matchAll=false значит
    // режим "любое слово", matchAll=true значит режим "все слова"
    static bool matchTags(const QStringList &queryWords, const QStringList &recordTags, bool matchAll, int &matchCount);


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

    QHBoxLayout *toolsLine;
    QGridLayout *toolsGrid;

    QVBoxLayout *centralDesktopLayout;

    FindTableWidget *findTable;

    QProgressDialog *progress;

    // Подсказка автодополнения и ее словарь. Комплитер привязан к строке
    // через setWidget, а не setCompleter: иначе QLineEdit ищет совпадение
    // всей строки и подсказка после пробела не появляется. Флаг включает
    // ручной привод когда словарь непуст
    QCompleter *fieldCompleter;
    QStandardItemModel *fieldCompleterModel;
    bool fieldCompleterEnabled;

    // Отложенная пересборка словаря подсказок при изменении метаданных
    // дерева. Таймер сбрасывается на каждом сохранении, поэтому пакетная
    // операция вроде вставки веток дает одну пересборку, а не десять
    QTimer *completerRefreshTimer;
    bool treeMetadataConnected;

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

    // Автодополнение запроса словами из отмеченных полей (теги, названия
    // и прочие, кроме полнотекстового поля Text). Словарь собирается
    // одним проходом по дереву, подсказка показывает объединение
    // словарей всех отмеченных галочек. Выпадашка появляется начиная
    // с двух букв и показывает не больше десяти вариантов
    void setupFieldCompleter(void);

    void findStart(void);
    void findRecurse(const TreeItem* curritem);
    bool findInTextProcess(const QString& text);

    // Подсчет количества совпадений в тексте по тем же правилам что и поиск
    // (целые слова или подстрока). Нужен для столбца совпадений
    int countMatchesInText(const QString& text);

    void switchToolsExpand(bool flag);

    QStringList textDelimiterDecompose(QString text);


    // Поля, где нужно искать (Заголовок, текст, теги...)
    QMap<QString, bool> searchArea;

    // Список слов, которые нужно искать
    QStringList searchWordList;

    int totalProgressCounter;

    int cancelFlag;

    bool isUnsearchCryptBranchPresent; // Флаг, определяющий, были ли непросмотренные ветки при поиске (зашированные ветки, но пароль небыл введен)

protected:

    // Словарь подсказок пересобирается при каждом показе: при старте
    // виджет не проходит через widgetShow, а просто восстанавливается
    // видимым, и без этого подсказки молчат пока не тронешь галочки
    void showEvent(QShowEvent *event) override;
};

#endif	/* _FINDSCREEN_H_ */

