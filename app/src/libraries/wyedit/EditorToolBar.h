#ifndef EDITORTOOLBAR_H
#define EDITORTOOLBAR_H

#include <QWidget>
#include <QToolBar>
#include <QVBoxLayout>

// Виджет для отрисовки кнопок форматирования текста
// Является базовым для EditorToolBarAssistant


class EditorFontSizeComboBox;
class EditorFontFamilyComboBox;
class EditorDropDownButton;


class EditorToolBar : public QWidget
{
    Q_OBJECT

public:

    explicit EditorToolBar(QWidget *parent = nullptr);
    virtual ~EditorToolBar();

    // Область, содержащая линейки с кнопками форматирования
    QVBoxLayout textformatButtonsLayout;

    // Тулбары с кнопками форматирования текста
    QToolBar    toolsLine1;
    QToolBar    toolsLine2;

    // Кнопки форматирования текста
    QAction *bold;
    QAction *italic;
    QAction *underline;
    QAction *strikeout; // Зачеркивание текста
    QAction *monospace;
    QAction *superscript;
    QAction *subscript;
    EditorDropDownButton *code; // Кнопка кода: меню выбора языка подсветки
    EditorDropDownButton *heading; // Кнопка заголовков: H1, H2, H3, обычный текст
    QAction *headingH1Action;
    QAction *headingH2Action;
    QAction *headingH3Action;
    QAction *headingNormalAction;
    QAction *clear; // Очистка начертания текста
    QAction *textOnly; // Полная очистка текста (оставить один текст)
    QAction *fixBreakSymbol;
    QAction *tabsToSpaces;

    QAction *undo;
    QAction *redo;

    QAction *numericList;
    QAction *dotList;

    QAction *indentPlus;
    QAction *indentMinus;

    QAction *alignLeft;
    QAction *alignCenter;
    QAction *alignRight;
    QAction *alignWidth;

    QAction *settings;

    EditorFontFamilyComboBox *fontSelect;
    EditorFontSizeComboBox   *fontSize;
    // QAction                  *fontColor;
    // QAction                  *backgroundColor;
    EditorDropDownButton *fontColor;
    EditorDropDownButton *backgroundColor;

    // Пункты выпадашек цвета: анонимные, указатели нужны для перевода
    QAction *fontColorSelectAction;
    QAction *fontColorNoColorAction;
    QAction *backgroundColorSelectAction;
    QAction *backgroundColorNoColorAction;

    // Пункты выпадашки кода: порядок совпадает с TypefaceFormatter::CodeLanguage
    // (пункт 0 - обычный код без подсветки, дальше языки по алфавиту)
    QAction *codePlainAction;
    QAction *codeAngularAction;
    QAction *codeBashAction;
    QAction *codeCAction;
    QAction *codeCMakeAction;
    QAction *codeCssAction;
    QAction *codeCxxAction;
    QAction *codeCSharpAction;
    QAction *codeGlslAction;
    QAction *codeGoAction;
    QAction *codeHtmlAction;
    QAction *codeIniAction;
    QAction *codeJavaAction;
    QAction *codeJsAction;
    QAction *codeJsonAction;
    QAction *codeLuaAction;
    QAction *codeMakeAction;
    QAction *codePhpAction;
    QAction *codePythonAction;
    QAction *codeQmlAction;
    QAction *codeReactAction;
    QAction *codeRustAction;
    QAction *codeSqlAction;
    QAction *codeTypescriptAction;
    QAction *codeXmlAction;
    QAction *codeYamlAction;

    QAction *reference;
    QAction *secret;

    QAction *showHtml;

    QAction *findText;

    QAction *showFormatting;

    QAction *createTable;
    QAction *tableRemoveRow;
    QAction *tableRemoveCol;
    QAction *tableAddRow;
    QAction *tableAddCol;
    QAction *tableMergeCells;
    QAction *tableSplitCell;
    QAction *tableProperties;

    QAction *insertImageFromFile;
    QAction *openImage;
    QAction *micRecord;
    QAction *insertHorizontalLine;
    QAction *specialChar;
    QAction *mathExpression;
    QAction *expandEditArea;
    QAction *expandToolsLines;
    QAction *toggleTools;
    QAction *save;

    QAction *mobileBack;
    QAction *mobileFindInBase;

    QAction *showText;

    QAction *toAttach;

    QIcon         iconAttachExists; // Иконка, когда аттачи есть
    QIcon         iconAttachNotExists; // Иконка, когда аттачей нет

    void init();

    void initDisableToolList(QStringList toolNames); // Список кнопок, которые нельзя использовать - они будут невидны (не добавлены)
    void setEnableModifyTextButton(bool state); // Установка или снятие доступности кнопок, модифицирующих текст

    void initToolsLine1(QStringList toolsLine);
    void initToolsLine2(QStringList toolsLine);

    void switchAttachIconExists(bool isExists); // Переключение иконки аттачей в режим есть аттачи или нет аттачей

    // Список названий всех контролов (команд) панелей инструментов
    QStringList getCommandNameList();

    // Иконка, присвоенная инструменту (по имени инструмента)
    QIcon getIcon(const QString &name);

public slots:

    void setupShortcuts(void);

    // Прямые тексты выпадашек цвета и подсказок. Остальное освежает
    // setupShortcuts по сигналу updateWidgetShortcut
    void retranslateUi(void);

protected:

    bool isInit;

    // Список имен инструментов на тулбарах 1 и 2
    QStringList toolsListInLine1;
    QStringList toolsListInLine2;

    // Список имен инструментов, которые не нужно подгружать
    QStringList disableToolList;

    QVector<QAction *> actions; // Делать массив объектов вместо указателей на объект нельзя, невозможно работать с QAction в контейнере QVector

    void setupSignals(void);
    void setupToolBarTools(void);
    void assemblyButtons(void);

    void insertButtonToToolsLine(QString toolName, QToolBar &line);
    void registryActionsToToolBarWidget();

    void hideAllToolsElements(void);

    void clearToolsLines(void);
    void updateToolsLines(void);

    QAction* generateAction(QIcon icon=QIcon());

    // Размер иконок на панели инструментов редактора
    QSize getIconSize() const {return toolsLine1.iconSize();}

};

#endif // EDITORTOOLBAR_H
