#ifndef _METAEDITOR_H_
#define _METAEDITOR_H_

#include "libraries/wyedit/Editor.h"


class QWidget;
class QHBoxLayout;
class QGridLayout;
class QScrollArea;
class QSplitter;
class QLabel;
class QShowEvent;
class QTimer;
class QDialog;
class QMenu;
class QAction;

class AttachTableScreen;
class HtmlSourceEdit;

class MetaEditor: public Editor
{
 Q_OBJECT

public slots:

 void setField(QString n, QString v);
 void clearAll(void);
 void onClickToTag(const QString &tagNum);
 void onClickToUrl(const QString &link);

private slots:

public:
 MetaEditor(QWidget *parent=nullptr);
 ~MetaEditor(void);

 void setTreePath(QString path);
 void setName    (QString name);
 void setAuthor  (QString author);
 void updateRecordHeaderVisibility(void);
 void setUrl     (QString url);
 void setTags    (QString tags);

 // Список меток открытой записи для обновления извне (панель меток)
 QStringList getTagsList(void) const;

 static void toAttachCallback(void);

 void switchToEditorLayout(void);
 void switchToAttachLayout(void);

 void setReadOnly(bool state);

 void setFocusToBaseWidget();

 // Живой исходник HTML под заметкой (режим редактор+превью).
 // Верх сплиттера - текст заметки, низ - HtmlSourceEdit в паре.
 // Размещение выбирается выпадашкой кнопки HTML на тулбаре:
 // внизу, слева, справа или в отдельном окне
 enum class HtmlPlacement { Hidden=0, Bottom=1, Left=2, Right=3, Window=4 };
 void setHtmlSourcePlacement(HtmlPlacement placement);
 HtmlPlacement htmlSourcePlacement(void) const;
 void toggleHtmlSource(void);
 void setHtmlSourceVisible(bool visible);
 bool isHtmlSourceVisible(void) const;

public:

 // Цвет фона поля заметки: настройка поверх цвета темы.
 // Пустая строка effectiveNotePaperColor означает цвет темы
 static QString effectiveNotePaperColor(const QString &themeName,
                                       const QString &setting);
 // Цвет шрифта поля заметки: настройка поверх цвета темы.
 // Пустая строка effectiveNoteFontColor означает цвет темы
 static QString effectiveNoteFontColor(const QString &themeName,
                                      const QString &setting);
 // Общий stylesheet поля заметки из обеих настроек (бумага и шрифт).
 // Свой stylesheet виджета бьет тему, поэтому в него попадает только
 // явный цвет из настроек; пустые настройки полностью отдают поле теме
 static QString noteTextAreaStyleSheet(void);

 // Сами CSS-объявления бумаги и шрифта без селектора: общий движок
 // для основного редактора и открепляемых окон
 static QStringList notePaperDeclarations(void);
 void applyNoteColors(void);

 // Позиция курсора в статусбаре: строка и колонка с единицы.
 // Пересчитывается по сигналу курсора и при смене языка
 void updateCursorLabel(void);

 // Прямые подписи Url и Tags. Вызывается из MainWindow::retranslateUi
 void retranslateUi(void);

private:
 void setupLabels(void);
 void setupUI(void);
 void metaAssembly(void);
 void setupSignals(void);

 void setMininizeGridRow(QWidget *widget);
 void setNormalGridRow(QWidget *widget);
 int getGridRowNumber(QWidget *widget);

 QLabel *treePath; // Надпись Path (только для мобильного интерфейса)

 QLabel *recordName; // Надпись Title
 QLabel *recordAuthor; // Надпись Author(s)

 QLabel *labelUrl; // Надпись "Url"
 QLabel *recordUrl;

 QLabel *labelTags; // Надпись "Tags"
 QWidget *recordTagsContainer;
 QHBoxLayout *recordTagsLayout;
 QScrollArea *recordTagsScrollArea;
 QString recordTagsText;
 QStringList recordTagsTextList;
 QList<QLabel*> recordTagsLabels;

 // QSplitter *editorAndFileTableSplitter;

 // Виджет слоя прикрепляемых файлов
 AttachTableScreen *m_attachTableScreen;

 // Виджет слоя редактирования текста
 QWidget *m_extendEditorScreen;

 // Группировалка виджетов всех слоев (слоя редактирования и слоя прикрепляемых файлов)
 QVBoxLayout *m_metaEditorJoinLayer;

 // Живой исходник HTML: сплиттер в ячейке текста, низ - панель.
 // Синхронизация двусторонняя с дебаунсом, sourceSyncing гасит эхо
 // программных правок. Состояние на сессию, в конфиг не пишется
 QSplitter *sourceSplitter=nullptr;
 HtmlSourceEdit *htmlSourcePane=nullptr;
 QTimer *noteToSourceTimer=nullptr;
 QTimer *sourceToNoteTimer=nullptr;
 bool sourceSyncing=false;

 // Зеркало курсора редактор<->исходник: связь через текстовые
 // сниппеты (шапка toHtml не дает прямого блок<->строка).
 // Фокус не трогается, только позиция. cursorSyncing гасит эхо
 // программных сдвигов
 bool cursorSyncing=false;

 // Размещение исходника на сессию, в конфиг не пишется.
 // lastInlinePlacement - последнее неоконное место для
 // закрытия окна и переключения видимости
 HtmlPlacement htmlPlacement=HtmlPlacement::Hidden;
 HtmlPlacement lastInlinePlacement=HtmlPlacement::Bottom;

 // Отдельное окно исходника: панель переезжает туда целиком,
 // сигналы и зеркала живут дальше (объект тот же). Закрытие
 // окна возвращает панель на последнее встроенное место
 QDialog *htmlSourceWindow=nullptr;
 QWidget *htmlWindowContainer=nullptr;

 // Выпадашка кнопки HTML на тулбаре: скрыть, внизу, слева,
 // справа, в окне. Пункты чекаются по текущему размещению
 QMenu *htmlSourceMenu=nullptr;
 QAction *htmlHideAction=nullptr;
 QAction *htmlBottomAction=nullptr;
 QAction *htmlLeftAction=nullptr;
 QAction *htmlRightAction=nullptr;
 QAction *htmlWindowAction=nullptr;

 void refreshSourcePane(void);
 void onNoteTextChanged(void);
 void onSourceTextChanged(void);
 void syncNoteToSource(void);
 void syncSourceToNote(void);
 void mirrorNoteCursorToSource(void);
 void mirrorSourceCursorToNote(void);
 void mirrorNotePosToSource(int notePos);
 void mirrorSourcePosToNote(int sourcePos);

 // Встроить панель в сплиттер с ориентацией, местом и показом
 void embedHtmlSourcePane(Qt::Orientation orientation, int paneIndex, bool show);

 // Треть ширины/высоты исходнику, остальное тексту
 void applyHtmlSourceSizes(void);

 // Панель целиком в отдельное окно и возврат из него
 void takePaneToWindow(void);
 void onHtmlSourceWindowClosed(void);

 // Выпадашка кнопки HTML на тулбаре и чеки по размещению
 void setupHtmlSourceMenu(void);
 void syncHtmlSourceMenuChecks(void);

};

#endif /* _METAEDITOR_H_ */
