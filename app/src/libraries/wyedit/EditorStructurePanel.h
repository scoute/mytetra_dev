#ifndef EDITORSTRUCTUREPANEL_H
#define EDITORSTRUCTUREPANEL_H

#include <QWidget>

class EditorTextArea;
class QLabel;
class QListWidget;
class QListWidgetItem;

// Панель «Структура»: заголовок и живой список заголовков H1-H3
// текущей заметки. Клик по пункту ставит курсор в начало раздела.
// Сама панель текст не меняет и undo-стек не трогает
class EditorStructurePanel : public QWidget
{
    Q_OBJECT

public:

    explicit EditorStructurePanel(QWidget *parent=nullptr);

    // Подключение к области редактирования: первичная перестройка
    // и подписка на изменения документа и движение курсора
    void setTextArea(EditorTextArea *area);

    // Перечитать заголовки из документа
    void rebuild(void);

    // Подпись над списком. Видна всегда, кроме случая когда панель
    // живет в сетке MetaEditor: там подпись - отдельный виджет выше,
    // чтобы список начинался ровно на уровне текста заметки
    void setTitleVisible(bool visible);

    int count(void) const;
    QListWidgetItem *item(int row) const;
    int currentRow(void) const;

    // Смещение текста первой неиндентированной строки от левого края
    // панели: под него ровняется внешняя подпись в MetaEditor
    int firstItemTextX(void) const;

private slots:

    void onDocumentChanged(void);
    void onJump(QListWidgetItem *item);
    void onCursorMoved(void);
    void onRebuildTimeout(void);

private:

    // Подпись выравнивается по тексту пунктов: отступ вычисляется
    // по реальному положению первой строки без отступа-вложенности,
    // а не гадается под стиль (рамка списка и паддинги везде свои)
    void adjustTitleIndent(void);

    // Строка последнего заголовка не ниже позиции курсора, -1 если нет
    int currentHeadingRow(int cursorPos) const;

    EditorTextArea *m_area=nullptr;
    QTimer *m_timer=nullptr;
    QLabel *m_title=nullptr;
    QListWidget *m_list=nullptr;
    bool m_sync=false;

protected:
    void showEvent(QShowEvent *event) override;
};

#endif // EDITORSTRUCTUREPANEL_H
