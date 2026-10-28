#ifndef _EDITORFLOATTOC_H_
#define	_EDITORFLOATTOC_H_

#include <QWidget>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QToolButton;
class QTimer;
class QShowEvent;
class QResizeEvent;

class EditorTextArea;


// // Плавающее полупрозрачное содержание заметки поверх текста,
// справа вверху вьюпорта. Видно всегда, когда в заметке есть
// заголовки H1-H3, иначе скрыто. Клик по треугольнику сворачивает
// список (заголовок остается), клик по пункту прыгает к разделу.
// Замена боковой панели структуры и ее кнопки: вставка оглавления
// в текст живет отдельно и не трогается
class EditorFloatToc : public QWidget
{
 Q_OBJECT

public:
 EditorFloatToc(QWidget *parent=nullptr);
 virtual ~EditorFloatToc();

 // Область текста: подписывается на правки (перестройка с дебаунсом),
// на курсор (подсветка текущего раздела) и на ресайз вьюпорта
 void setTextArea(EditorTextArea *area);

 void rebuild(void);

 int count(void) const;
 QListWidgetItem *item(int row) const;
 int currentRow(void) const;

protected:
 // Следит за размером вьюпорта: оверлей прижат к правому верху
 bool eventFilter(QObject *watched, QEvent *event) override;

 // Подложка вручную: полупрозрачный скругленный прямоугольник
 void paintEvent(QPaintEvent *event) override;

private slots:
 void onDocumentChanged(void);
 void onRebuildTimeout(void);
 void onBarsRangeChanged(int minimum, int maximum);
 void onJump(QListWidgetItem *item);
 void onCursorMoved(void);
 void onToggleCollapsed(void);

private:
 // Перестройка списка по документу. Пустые заголовки подписываются,
// длинные обрезаются. Пусто — виджет прячется сам (показ только
// при наличии заголовков), свернутость переживает перестройку
 void place(void);
 int currentHeadingRow(int cursorPos) const;

 EditorTextArea *m_area=nullptr;
 QListWidget *m_list=nullptr;
 QToolButton *m_headerButton=nullptr;
 QTimer *m_timer=nullptr;

 bool m_sync=false;
 bool m_collapsed=false;
};


#endif	/* _EDITORFLOATTOC_H_ */

