#ifndef EDITORSTRUCTUREPANEL_H
#define EDITORSTRUCTUREPANEL_H

#include <QListWidget>

class EditorTextArea;
class QTimer;

// Панель «Структура»: живой список заголовков H1-H3 текущей заметки.
// Клик по пункту ставит курсор в начало раздела. Сама панель текст
// не меняет и undo-стек не трогает
class EditorStructurePanel : public QListWidget
{
    Q_OBJECT

public:

    explicit EditorStructurePanel(QWidget *parent=nullptr);

    // Подключение к области редактирования: первичная перестройка
    // и подписка на изменения документа и движение курсора
    void setTextArea(EditorTextArea *area);

    // Перечитать заголовки из документа
    void rebuild(void);

private slots:

    void onDocumentChanged(void);
    void onJump(QListWidgetItem *item);
    void onCursorMoved(void);
    void onRebuildTimeout(void);

private:

    // Строка последнего заголовка не ниже позиции курсора, -1 если нет
    int currentHeadingRow(int cursorPos) const;

    EditorTextArea *m_area=nullptr;
    QTimer *m_timer=nullptr;
    bool m_sync=false;
};

#endif // EDITORSTRUCTUREPANEL_H
