#include <QAction>
#include <QToolButton>
#include <QListWidget>
#include <QHBoxLayout>
#include <QStyle>
#include <QMenu>
#include <QApplication>
#include <QPointer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDebug>

#include "EditorDropDownButton.h"


EditorDropDownButton::EditorDropDownButton(QWidget *parent)
    : QWidget{parent},
      toolPseudoButton(this)
{
    setupUi();
}


void EditorDropDownButton::setupUi()
{
    QHBoxLayout* layout = new QHBoxLayout(this);

    m_button = new QToolButton();
    m_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_button->setPopupMode(QToolButton::MenuButtonPopup);

    m_button->setArrowType(Qt::NoArrow); // Убирается стандартная стрелка
    m_button->setIcon(style()->standardIcon(QStyle::SP_TitleBarShadeButton));


    // Создается меню
    m_menu = new QMenu(this);
    m_button->setMenu(m_menu);

    // Собирается виджет
    layout->addWidget(m_button);

    m_button->installEventFilter(this);
    // Клик по кнопке (стрелка меню его не дает): applying last color
    connect(m_button, &QToolButton::clicked,
            this,     &EditorDropDownButton::buttonClicked);

    // Обнуляются отступы контента
    // layout->setContentsMargins(0, 0, 0, 0);
    this->setDividingMargin(); // Попробовать регулировать через стиль темы
}


// Кнопка отображает только иконку из-за режима Qt::ToolButtonIconOnly,
// текст не будет виден. Метод не используется
void EditorDropDownButton::setText(const QString &text)
{
    m_button->setText(text);
}


void EditorDropDownButton::addAction(QAction *action)
{
    action->setProperty("menuIndex", m_actionCount);
    m_menu->addAction(action);

    // Соединяется сигнал нового добавленного пункта меню и
    // вызов обработчика через лямбда-функцию с передачей номера пункта меню
    // Нумерация пунктов меню идет с нуля
    connect(action, &QAction::triggered,
            this, [this, action](){
                    int index = action->property("menuIndex").toInt();
                    this->onMenuItemClick(index);
                  });

    m_actionCount++;
}


void EditorDropDownButton::setIcon(const QIcon &icon)
{
    m_button->setIcon(icon);
}


// Переключение кнопки в режим "нажал - открылось меню"
void EditorDropDownButton::setInstantPopup(bool enable)
{
    if(enable)
        m_button->setPopupMode(QToolButton::InstantPopup);
    else
        m_button->setPopupMode(QToolButton::MenuButtonPopup);
}


// Режим списка: вместо QMenu показывается всплывающий QListWidget
// со штатной полосой прокрутки
void EditorDropDownButton::setListMode(bool enable)
{
    m_listMode=enable;
}


// Видимая высота всплывающего списка в пунктах (только list-режим).
// Остальные пункты доступны прокруткой
void EditorDropDownButton::setMaxVisibleItems(int count)
{
    m_maxVisibleItems=count;
}


void EditorDropDownButton::setPreselectRow(int row)
{
    m_preselectRow=row;
}


// Показать всплывающий список языков (list-режим). Пункты берутся
// из действий меню, индекс пункта совпадает с menuIndex действия,
// поэтому сигнал menuItemClicked работает как для QMenu
void EditorDropDownButton::showListMenu(void)
{
    // Запоминается фокус, чтобы вернуть его после закрытия списка.
    // QMenu фокус не трогает, список с Qt::Popup - забирает
    QWidget *previousFocusWidget=QApplication::focusWidget();
    QPointer<QWidget> focusGuard(previousFocusWidget);

    QListWidget *list=new QListWidget(m_button);
    list->setWindowFlags(Qt::Popup);
    list->setFocusPolicy(Qt::StrongFocus);
    list->setUniformItemSizes(true);

    // Подсветка следует за мышью, как в обычном QMenu:
    // без tracking вьюпорт не шлет hover-события и строка
    // подсвечивается только по клику
    list->setMouseTracking(true);
    list->viewport()->setMouseTracking(true);

    int maxWidth=m_button->width();
    for(QAction *action : m_menu->actions())
    {
        // В списке нет колонки шорткатов как в QMenu, поэтому шорткат
        // дописывается в текст строки (раньше он ехал в action->text()
        // для всех, что давало задвоение в обычных меню)
        QString itemText=action->text();
        if(!action->shortcut().isEmpty())
            itemText+=" ("+action->shortcut().toString()+")";
        QListWidgetItem *item=new QListWidgetItem(action->icon(), itemText, list);
        item->setData(Qt::UserRole, action->property("menuIndex").toInt());
        maxWidth=qMax(maxWidth, list->sizeHintForColumn(0));
    }

    int rowCount=list->count();
    int visibleCount=rowCount;
    if(m_maxVisibleItems>0)
        visibleCount=qMin(rowCount, m_maxVisibleItems);

    int rowHeight=list->sizeHintForRow(0);
    if(rowHeight<=0)
        rowHeight=list->fontMetrics().height()+8;

    list->setMinimumWidth(maxWidth+4);
    list->setFixedHeight(visibleCount*rowHeight+2*list->frameWidth());

    QPoint popupPos=m_button->mapToGlobal(QPoint(0, m_button->height()));
    list->move(popupPos);

    // Предвыбор строки: язык блока под курсором (кнопка кода),
    // иначе первая. Невалидный номер игнорируется
    if(m_preselectRow>=0 && m_preselectRow<rowCount)
        list->setCurrentRow(m_preselectRow);
    else
        list->setCurrentRow(0);

    // Текущий язык дополнительно помечается галкой: одного
    // current-подсвечивания в чужой теме может быть не видно,
    // а чекбокс рисуется стилем всегда. Без языка под курсором
    // галок нет, чекбокс ставится только на текущую строку
    if(m_preselectRow>=0 && m_preselectRow<rowCount)
    {
        QListWidgetItem *current=list->item(m_preselectRow);

        if(current!=nullptr)
        {
            current->setFlags(current->flags() | Qt::ItemIsUserCheckable);
            current->setCheckState(Qt::Checked);
        }
    }

    // Текущий открытый список запоминается, чтобы фильтр событий
    // мог двигать текущую строку за мышью. Прошлый список, если вдруг
    // еще жив, закрывается: открыт всегда только один
    if(m_list!=nullptr)
        m_list->close();

    m_list=list;
    list->viewport()->installEventFilter(this);
    list->installEventFilter(this); // Esc ловится самим списком, не вьюпортом

    connect(list, &QListWidget::itemClicked,
            this, [this, list, focusGuard](QListWidgetItem *item){
                int index=item->data(Qt::UserRole).toInt();
                list->close();
                if(!focusGuard.isNull())
                    focusGuard->setFocus();
                this->onMenuItemClick(index);
            });

    connect(list, &QListWidget::itemActivated,
            this, [this, list, focusGuard](QListWidgetItem *item){
                int index=item->data(Qt::UserRole).toInt();
                list->close();
                if(!focusGuard.isNull())
                    focusGuard->setFocus();
                this->onMenuItemClick(index);
            });

    // Список удаляется сам при закрытии
    connect(list, &QListWidget::destroyed,
            this, [this, list, focusGuard](){
                if(m_list==list)
                    m_list=nullptr;
                if(!focusGuard.isNull() && QApplication::focusWidget()==nullptr)
                    focusGuard->setFocus();
            });
    list->setAttribute(Qt::WA_DeleteOnClose);

    list->show();
    list->setFocus();

    // Явный захват мыши: голый QWidget с Qt::Popup сам граб не ставит
    // (в отличие от QMenu), и клики вне списка до него не доходят —
    // без граба закрыть кликом мимо невозможно. Клик вне прямоугольника
    // гасится в фильтре событий ниже, граб снимается скрытием
    list->grabMouse();
}


// В list-режиме клик мыши и Space/Enter по кнопке показывают список,
// а не внутреннее QMenu кнопки (QToolButton с InstantPopup открывает
// меню приватным путем, мимо showMenu())
bool EditorDropDownButton::eventFilter(QObject *watched, QEvent *event)
{
    if(m_listMode && watched==m_button)
    {
        // Повторный клик по кнопке закрывает открытый список (тоггл).
        // Без этого пресс сначала гасит Qt::Popup, а потом тут же
        // открывает список заново — закрыть кликом невозможно.
        // Проверка только на press: release при открытом списке
        // относится ему же, а не кнопке
        if(event->type()==QEvent::MouseButtonPress &&
           m_list!=nullptr && m_list->isVisible())
        {
            m_list->close();
            return true;
        }

        if(event->type()==QEvent::MouseButtonPress)
        {
            showListMenu();
            return true;
        }

        if(event->type()==QEvent::KeyPress)
        {
            int key=static_cast<QKeyEvent *>(event)->key();
            if(key==Qt::Key_Space || key==Qt::Key_Enter || key==Qt::Key_Return)
            {
                showListMenu();
                return true;
            }
        }
    }

    // Движение мыши над открытым списком: текущая строка едет за курсором.
    // Отслеживается напрямую по событию, а не через itemEntered,
    // чтобы поведение не зависело от доставки hover-событий
    if(m_list!=nullptr && watched==m_list->viewport() &&
            event->type()==QEvent::MouseMove)
    {
        QMouseEvent *mouseEvent=static_cast<QMouseEvent *>(event);
        QListWidgetItem *item=m_list->itemAt(mouseEvent->pos());

        if(item!=nullptr)
            m_list->setCurrentItem(item);
    }

    // Отмена списка по Esc: обычный QMenu закрывается сам,
    // самодельному списку закрытие делается вручную. Фокус
    // возвращается штатным путем через обработчик destroyed
    if(m_list!=nullptr && watched==m_list &&
            event->type()==QEvent::KeyPress)
    {
        int key=static_cast<QKeyEvent *>(event)->key();
        if(key==Qt::Key_Escape)
        {
            m_list->close();
            return true;
        }
    }

    // Клик мимо списка при активном грабе мыши: пресса приходит
    // в сам список с координатами вне его прямоугольника.
    // Закрыть и съесть, иначе клик провалится дальше
    if(m_list!=nullptr && watched==m_list &&
            event->type()==QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent=static_cast<QMouseEvent *>(event);

        if(!m_list->rect().contains(mouseEvent->pos()))
        {
            m_list->close();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}


void EditorDropDownButton::showMenu()
{
    if(m_listMode)
    {
        showListMenu();
        return;
    }

    m_button->showMenu();

    qDebug() << "Show menu for EditorDropDownButton";
}


void EditorDropDownButton::setDividingMargin()
{
    int layoutLeftMargin = this->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 3;
    int layoutTopMargin = 0; // this->style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    int layoutRightMargin = this->style()->pixelMetric(QStyle::PM_LayoutRightMargin) / 3;
    int layoutBottomMargin = 0; // this->style()->pixelMetric(QStyle::PM_LayoutBottomMargin);

    QMargins margins(layoutLeftMargin,
                     layoutTopMargin,
                     layoutRightMargin,
                     layoutBottomMargin);

    this->layout()->setContentsMargins(margins);
}


void EditorDropDownButton::onMenuItemClick(int num)
{
    qDebug() << "В EditorDropDownButton выбран пункт " << num;

    emit menuItemClicked(num);
}





