#ifndef MYTETRA_EDITOR
#define MYTETRA_EDITOR

#include <QBoxLayout>
#include <QToolButton>
#include <QFontComboBox>
#include <QSpinBox>
#include <QTextEdit>
#include <QWidget>
#include <QLabel>
#include <QTextCodec>
#include <QToolBar>
#include <QSlider>
#include <QStringList>

#include "EditorFindDialog.h"
#include "EditorFindBar.h"
#include "formatters/Formatter.h"
#include "formatters/PlacementFormatter.h"
#include "formatters/TypefaceFormatter.h"
#include "formatters/ListFormatter.h"
#include "formatters/TableFormatter.h"
#include "formatters/ImageFormatter.h"
#include "formatters/AudioFormatter.h"
#include "formatters/MathExpressionFormatter.h"
#include "formatters/ReferenceFormatter.h"
#include "formatters/SecretFormatter.h"


// ----------------------------------------------------------
// WyEdit - визуальный редактор для MyTetra
// Волгодонск, 2010 г.
// Контакты: xintrea@gmail.com, www.webhamster.ru
// Данный исходный код распространяется под лицензией GPL v.3
// © Степанов С. М. 2010
// ----------------------------------------------------------


#define WYEDIT_VERSION "WyEdit v.1.10 / 20.01.2016"

class EditorConfig;
class EditorTextEdit;
class EditorContextMenu;
class EditorTextArea;
class EditorIndentSliderAssistant;
class EditorToolBarAssistant;
class EditorStructurePanel;
class Formatter;
class MetaEditor;
class EditorCursorPositionDetector;

class Editor : public QWidget
{
 Q_OBJECT

 friend class MetaEditor;

 friend class Formatter;
 friend class PlacementFormatter;
 friend class TypefaceFormatter;
 friend class ListFormatter;
 friend class TableFormatter;
 friend class ImageFormatter;
 friend class AudioFormatter;
 friend class MathExpressionFormatter;
 friend class ReferenceFormatter;
 friend class SecretFormatter;

 friend class EditorContextMenu;

public:
 Editor(QWidget *parent=nullptr);
 ~Editor(void);

 // Объект, хранящий настройки редактора
 EditorConfig  *editorConfig=nullptr;

 // Ассистент панели кнопок
 EditorToolBarAssistant *editorToolBarAssistant=nullptr; // todo: Переименовать в toolBarAssistant?

 // Ассистент виджета горизонтальной линейки отступов
 EditorIndentSliderAssistant  *indentSliderAssistant=nullptr;

 // Вертикальная группировалка линеек кнопок и области редактирования
 QVBoxLayout   *buttonsAndEditLayout=nullptr;

 // Контекстное меню редактора
 EditorContextMenu *editorContextMenu=nullptr;

 EditorCursorPositionDetector *cursorPositionDetector=nullptr;

 const char *getVersion(void);

 void initEnableAssembly(bool flag);
 void initConfigFileName(QString name);
 void initEnableRandomSeed(bool flag);
 void initDisableToolList(QStringList toolNames);
 void init(int mode);

 // Методы работы с textarea
 void setTextarea(QString text);

 // Предпросмотр чужого html в этом документе: как setTextarea,
 // но сохраняет URL документа, чтобы относительные картинки
 // продолжали резолвиться. Коммитов и файлов не трогает
 void previewTextarea(const QString &text);

 void setTextareaEditable(bool editable);
 QString getTextarea(void);

 // Стабильный текст для сравнения и хранения: баблы голосовых
 // отдаются ссылками (как при сохранении). Без баблов совпадает
 // с getTextarea(). Сырой toHtml баблы не round-trip'ит
 QString getTextareaStable(void);
 QTextDocument *getTextareaDocument(void);

 // Ссылка голосового бабла под курсором, пусто если курсор не на бабле
 QString voiceBubbleHrefAtCursor(void);
 void setTextareaModified(bool modify);
 bool getTextareaModified(void);

 // Свой stylesheet области текста (цвета поля заметки из настроек).
 // Нужен окнам с собственным редактором вроде создания записи
 void setTextAreaStyleSheet(const QString &styleSheet);

 // Перекрасить секреты открытого документа глобальным цветом.
 // Для меню Вид: конфиг уже обновлен, документ трогать не надо
 void repaintSecrets(void);

 // Виджет полоски поиска для вставки в сетку MetaEditor
 EditorFindBar *findBarWidget(void);

 // Мост из глобального поиска: показать поиск в заметке с запросом
 // и прыгнуть к первому совпадению. Из нескольких слов выбирается то,
 // что встречается раньше всех в тексте, либо только первое слово.
 // Возвращает было ли совпадение: нет - вызыватель открывает заметку
 // с начала (хит мог быть по имени, а не по тексту)
 bool jumpToFirstMatch(const QStringList &words, bool firstWordOnly=false);

 // Абсолютный или относительный путь (т.е. директория),
 // куда будет сохраняться текст. Без завершающего слеша
 bool setWorkDirectory(QString dirName);
 QString getWorkDirectory(void);
 
 // Имя файла, куда должен сохраняться текст
 // Без пути, только имя
 void setFileName(QString fileName);
 QString getFileName(void);

 void saveTextarea();
 bool saveTextareaText();
 bool saveTextareaImages(int mode);
 bool loadTextarea();

 // Методы установки нестандартных процедур чтения и сохранения текста
 void setSaveCallback(void (*func)(QObject *editor, QString saveString));
 void setLoadCallback(void (*func)(QObject *editor, QString &loadString));

 // Метод установки функции переключения на предыдущее окно (для мобильного интерфейса)
 void setBackCallback(void (*func)(void));

 // Метод установки функции нажатия на кнопку Attach
 void setAttachCallback(void (*func)(void));

 // Методы установки и чтения произвольных нестандартных данных 
 // которые может хранить объект редактора
 void setMiscField(QString name, QString value);
 QString getMiscField(QString name);
 void clearAllMiscField(void);

 void setDirFileEmptyReaction(int mode);
 int  getDirFileEmptyReaction(void);

 int  getCursorPosition(void);
 void setCursorPosition(int n);

 int  getScrollBarPosition(void);
 void setScrollBarPosition(int n);

 QString smartFontFamily(QString fontName); // Умное преобразование имени шрифта
 int smartFontSize(int fontSize); // Умное преобразование размера шрифта

 void switchAttachIconExists(bool isExists);

 enum
 {
  SAVE_IMAGES_SIMPLE=0,       // Простое сохранение картинок, встречающихся в тексте
  SAVE_IMAGES_REMOVE_UNUSED=1 // Сохранение картинок, встречающихся в тексте, с удалением из каталога записи тех, которых в тексте нет
 };

 enum
  {
   DIRFILEEMPTY_REACTION_SHOW_ERROR,
   DIRFILEEMPTY_REACTION_SUPPRESS_ERROR
  };

 enum
 {
  WYEDIT_DESKTOP_MODE=0,
  WYEDIT_MOBILE_MODE=1
 };


signals:

 void send_expand_edit_area(bool flag);

 void wyeditFindInBaseClicked();

 // Уход из полоски в глобальный поиск с текстом запроса
 void wyeditFindInBaseWithText(const QString &text);

 void updateIndentsliderToActualFormat();
 void updateIndentSliderGeometry();

 void updateAlignButtonHiglight(bool);

 void changeFontselectOnDisplay(QString fontName);
 void changeFontsizeOnDisplay(int n);

private slots:

 void onShowhtmlClicked(void);
 void onFindtextClicked(void);
 void onFindNextHotkey(void);
 void onFindPrevHotkey(void);
 void onScrollUpHotkey(void);
 void onScrollDownHotkey(void);
 void onSettingsClicked(void);
 void onShowformattingClicked(bool);

 void onExpandEditAreaClicked(void);
 virtual void onStructureToggled(bool checked);
 void onSaveClicked(void);
 void onBackClicked(void);
 void onFindInBaseClicked(void);
 void onShowTextClicked(void);
 void onToAttachClicked(void);

 void onCursorPositionChanged(void); // Слот, контролирущий перемещение курсора
 void onSelectionChanged(void);
 void onUndo(void);
 void onRedo(void);
 void onCut(void);
 void onCopy(void);
 void onPaste(void);
 void onPasteAsPlainText(void);
 void onSelectAll(void);

 void onFindtextSignalDetect(const QString &text, QTextDocument::FindFlags flags);
 void onFindPrevSignalDetect(const QString &text, QTextDocument::FindFlags flags);
 void onReplacetextSignalDetect(const QString &text, const QString &replaceText, QTextDocument::FindFlags flags);
 void onReplaceAllSignalDetect(const QString &text, const QString &replaceText, QTextDocument::FindFlags flags);

 // Совпадает ли текущее выделение с искомой строкой при заданных флагах
 bool isSelectionMatch(const QString &text, QTextDocument::FindFlags flags) const;

 // Слоты полоски поиска: тот же движок, свое поведение на краях
 // (без окон сообщений, счетчик вместо них, зацикливание всегда)
 void onFindBarFind(const QString &text, QTextDocument::FindFlags flags);
 void onFindBarPrevious(void);
 void onFindBarNext(void);
 void onFindBarHighlight(const QString &text, QTextDocument::FindFlags flags);
 void onFindBarHidden(void);
 void onFindBarReplaceOne(const QString &text, const QString &replaceText, QTextDocument::FindFlags flags);
 void onFindBarReplaceAll(const QString &text, const QString &replaceText, QTextDocument::FindFlags flags);
 void onFindBarInBase(const QString &text);

 // Переключение окна и полоски с переносом запроса, режим хранится в конфиге
 void onAttachToBar(void);
 void onDetachToWindow(void);

 // Движок поиска в заметке, общий для окна и полоски.
 // Совпадения собираются вперед от начала документа, навигация ходит
 // по списку, подсветка красит все желтым и текущее оранжевым
 void collectFindMatches(const QString &text, QTextDocument::FindFlags flags);
 void paintFindMatches(void);
 bool goToFindMatch(bool backward, bool wrap);
 void clearFindMatches(void);
 void updateFindCounter(void);
 void showFindNotFound(const QString &text);
 void onFindDocumentChanged(void);
 void rehighlightFindMatches(void);
 int indexFromCursor(bool backward) const;

 // Открытие контекстного меню
 void onCustomContextMenuRequested(const QPoint &pos);

 // void onModificationChanged(bool flag);

private:

 void setupSignals(void);
 void setupToolsSignals(void);
 void setupEditorToolBarAssistant(int mode, EditorTextArea *textArea, QStringList disableToolList);
 void setupIndentSliderAssistant(void);
 void setupEditorTextArea(void);
 void setupStructurePanel(void);
 void setupCursorPositionDetector(void);
 void setupFormatters(void);
 void assembly(void);

 // Устанавка размера табуляции для клавиши Tab
 void setTabSize();

 // Переопределение событий обработки клавиш
 // нужны для определения момента undo/redo
 virtual void keyPressEvent(QKeyEvent *event);
 virtual void keyReleaseEvent(QKeyEvent *event);

 // Перехват Enter в заголовке раньше штатного сплита:
 // в конце заголовка новый абзац сразу обычный, а не еще заголовок
 virtual bool eventFilter(QObject *object, QEvent *event);

 // Область редактирования текста
 EditorTextArea *textArea=nullptr;

 // Панель структуры: живой список H1-H3, видна по кнопке тулбара.
 // В диалогах едет в сплиттер рядом с текстом, в MetaEditor встает
 // третьей колонкой сетки
 EditorStructurePanel *structurePanel=nullptr;

 // Форматировщики текста
 TypefaceFormatter       *typefaceFormatter=nullptr;
 PlacementFormatter      *placementFormatter=nullptr;
 ListFormatter           *listFormatter=nullptr;
 TableFormatter          *tableFormatter=nullptr;
 ImageFormatter          *imageFormatter=nullptr;
 AudioFormatter          *audioFormatter=nullptr;
 MathExpressionFormatter *mathExpressionFormatter=nullptr;
 ReferenceFormatter      *referenceFormatter=nullptr;
 SecretFormatter         *secretFormatter=nullptr;

 bool isInit;

 bool        initDataEnableAssembly = true;
 QString     initDataConfigFileName;
 bool        initDataEnableRandomSeed = false;
 QStringList initDataDisableToolList;

 // Рабочая директория редактора и файл текста
 // Используется при сохранении текста на диск
 QString workDirectory;
 QString workFileName;

 int viewMode; // Режим отображения редактора - WYEDIT_DESKTOP_MODE или WYEDIT_MOBILE_MODE

 EditorFindDialog *findDialog; // Виджет поиска
 EditorFindBar *findBar; // Встраиваемая полоска поиска, тот же движок

 // Скрытые экшены перехода между совпадениями: кнопок на панели нет,
 // срабатывают только по хоткеям editor-findNext / editor-findPrevious
 QAction *findNextAction=nullptr;
 QAction *findPrevAction=nullptr;

 // Скрытые экшены прокрутки без курсора: только хоткеи
 // editor-scrollUp / editor-scrollDown
 QAction *scrollUpAction=nullptr;
 QAction *scrollDownAction=nullptr;

 // Состояние движка поиска: запрос, флаги, совпадения, текущий индекс.
 // findCurrentIndex=-1 означает отсутствие текущего (после набора текста)
 QString findQuery;
 QTextDocument::FindFlags findFlags;
 QList<QTextCursor> findMatches;
 int findCurrentIndex=-1;

 // Отложенный пересчет подсветки при правке документа: правка в процессе
 // setHtml видит недособранный документ, пересчет откладывается в очередь
 bool findRehighlightPending=false;

 bool expandEditAreaFlag; // Распахнуто ли на максимум окно редактора

 // Указатели на переопределенные функции записи и чтения редактируемого текста
 void (*saveCallbackFunc)(QObject *editor, QString saveString)=nullptr;
 void (*loadCallbackFunc)(QObject *editor, QString &loadString)=nullptr;

 // Указатель на функцию переключения на предыдущее окно (для мобильного интерфейса)
 void (*backCallbackFunc)(void)=nullptr;

 // Указатель на функцию открытия присоединенных файлов
 void (*attachCallbackFunc)(void)=nullptr;

 // Поля для хранения произвольных данных
 // Обычно используются для запоминания нестандартного набора данных
 // в объекте редактора, и считываются из функции обратного вызова
 QMap<QString, QString> miscFields;

 int dirFileEmptyReaction;
};

#endif /* MYTETRA_EDITOR */
