#ifndef _FINDSCREEN_H_
#define	_FINDSCREEN_H_

#include <QWidget>
#include <QMap>

class QLineEdit;
class QPushButton;
class QToolButton;
class QShowEvent;
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

    // Поиск кликнутого тега только в поле тегов: остальные галки
    // на время поиска снимаются и возвращаются обратно, иначе клик
    // ищет слово везде (заголовок, текст...), что неожиданно
    void setFindTag(QString tag);

    // Разбить поле тегов записи на отдельные теги: разделители запятая
    // и точка с запятой, пробелы по краям отбрасываются
    static QStringList splitRecordTags(const QString &tagsField);

    // Обновить прямые тексты новым языком. Вызывается из
    // MainWindow::retranslateUi
    void retranslateUi(void);


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
    QPushButton *closeButton;

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

    void setupFindTextAndButton(void);
    void assemblyFindTextAndButton(void);

    void setupComboOption(void);
    void assemblyComboOption(void);

    void setupCloseButton(void);
    void assemblyCloseButton(void);
    void fixCloseButtonSize(void);

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

    // Разовый режим строгого совпадения тега (клик по тегу):
    // ищется тег целиком без учета регистра, подстроки не катят
    bool exactTagMode;
    QString exactTag;

    int totalProgressCounter;

    int cancelFlag;

    bool isUnsearchCryptBranchPresent; // Флаг, определяющий, были ли непросмотренные ветки при поиске (зашированные ветки, но пароль небыл введен)

protected:
    void showEvent(QShowEvent *event) override;
};

#endif	/* _FINDSCREEN_H_ */

