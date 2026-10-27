#ifndef _EDITORTEXTAREA_H_
#define _EDITORTEXTAREA_H_

#include <QTextEdit>


class QPaintEvent;
class QMimeData;
class QTapAndHoldGesture;
class QEvent;
class QGestureEvent;


class EditorTextArea : public QTextEdit
{
    Q_OBJECT

public:

    EditorTextArea(QWidget *parent=nullptr);
    ~EditorTextArea(void);

    //! Константы, используемые для определения формата данных в буфере обмена
    enum MimeDataFormat{ MimeDataText=1,
                         MimeDataHtml=2,
                         MimeDataImage=3 };

    virtual void paintEvent(QPaintEvent *event);
    virtual void resizeEvent(QResizeEvent *event);

    virtual bool canInsertFromMimeData(const QMimeData *source) const;
    virtual void insertFromMimeData(const QMimeData *source);
    virtual QMimeData *createMimeDataFromSelection(void) const;
    MimeDataFormat detectMimeDataFormat(const QMimeData *source);

    //! Получение флага, указывающего рисовать ли символы форматирования
    bool getShowFormatting(void);

    //! Установка флага, указывающего рисовать ли символы форматирования
    void setShowFormatting(bool i);

    int getIndentStartedLeft(void);
    int getIndentStartedRight(void);

    void softRedraw();

signals:
    void tapAndHoldGestureFinished(const QPoint &);
    void updateIndentlineGeometry(void);
    void clickedOnReference(QString href);

    // Одинарный клик по баблу голосовой заметки: play/pause на месте
    void clickedOnVoiceBubble(QString href, QPointF docPoint);

    // Клик по ссылке-кнопке [copy] в строке маркера блока кода.
    // Передается позиция документа: обработчик сам находит строку
    // маркера и забирает код ниже нее
    void clickedOnCopyCode(int docPosition);

    // Клик по пункту оглавления: передается адрес ссылки-перехода,
    // обработчик сам находит раздел и ставит туда курсор
    void clickedOnTocGo(QString href);

    void downloadImages(const QString href);

    void doubleClickOnImage();

    // Одинарный Ctrl+клик по картинке: открыть во внешней программе
    void clickOnImage();

public slots:
    void showIndentEdge(bool i);
    void setIndentEdgePos(int i);

    void onGlobalPressKey(int key);
    void onGlobalReleaseKey(int key);

    void onDownloadImagesSuccessfull(const QString html,
                                     const QMap<QString, QByteArray> referencesAndMemoryFiles,
                                     const QMap<QString, QString> referencesAndInternalNames);

private:

    bool event(QEvent *event);
    bool gestureEvent(QGestureEvent *event);
    void tapAndHoldGestureTriggered(QTapAndHoldGesture *gesture);

    virtual bool eventFilter(QObject *o, QEvent *e);

    // Переопределение событий обработки клавиш
    // Нужны для отслеживания нажатия клавиши Ctrl чтобы при ней запускался переход по ссылке при клике на ссылку
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);

    // Shift+Tab снимает один уровень отступа в начале строки
    void unindentCurrentLine(void);

    // Tab/Shift+Tab с многострочным выделением: отступ всего блока.
    // Возвращает true если обработка выполнена (вызывающий не зовет
    // штатный путь), false если выделение однострочное или его нет
    bool indentSelectedLines(bool indent);

    // Переопределение событий обработки мышки
    // Нужны для обработки кликов по ссылкам в тексте записи
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);

    void switchReferenceClickMode(bool flag);


    bool m_flagShowFormatting; //< Рисовать ли символы форматирования

    bool m_flagShowIndentEdge; //< Рисовать ли линию отступа
    int  m_posIndentEdge; //< По какой координате рисовать линию отступа

    bool m_mouseCursorOverriden;

    QPoint m_currentMousePosition;

    // Имя картинки под точкой: пусто если картинки нет.
    // Нужно для ховер-подсказки и Ctrl+клика по изображению
    QString imageAt(const QPoint &point);

    // Очередь голосовых MIME вставок: HTML вставляется асинхронно
    // через downloadImages, пары href-путь ждут своей вставки здесь
    QList<QByteArray> pendingVoiceMimes;

    // Голосовые ссылки и баблы в диапазоне документа без дублей
    QStringList voiceHrefsInRange(int from, int to) const;

    // Простановка цветов якорям по типу ссылки после HTML-вставки.
    // Служебные href (голос, copy-кнопка, toc-метка) не трогаются:
    // их формат выставляется своим кодом отдельно
    void recolorAnchorsInRange(int startPos, int endPos);

    // Рабочий каталог заметки через родителя-редактор
    QString voiceWorkDirectory(void) const;

    // Свободное имя voice-*.ext в каталоге
    static QString uniqueVoiceFileName(const QString &dir, const QString &baseName);

    // Докопировать голосовые файлы вставки в каталог заметки
    // (перенос между заметками), починить ссылки при коллизии
    // имен, материализовать баблы. Вызывается после вставки HTML
    void finishVoicePaste(const QByteArray &voiceMime);
};

#endif // _EDITORTEXTAREA_H_
