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

    // В list-режиме клики и клавиатура перехватываются фильтром,
    // чтобы показать список вместо внутреннего QMenu кнопки
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

    int maxWidth=m_button->width();
    for(QAction *action : m_menu->actions())
    {
        QListWidgetItem *item=new QListWidgetItem(action->icon(), action->text(), list);
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
    list->setCurrentRow(0);

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
            this, [focusGuard](){
                if(!focusGuard.isNull() && QApplication::focusWidget()==nullptr)
                    focusGuard->setFocus();
            });
    list->setAttribute(Qt::WA_DeleteOnClose);

    list->show();
    list->setFocus();
}


// В list-режиме клик мыши и Space/Enter по кнопке показывают список,
// а не внутреннее QMenu кнопки (QToolButton с InstantPopup открывает
// меню приватным путем, мимо showMenu())
bool EditorDropDownButton::eventFilter(QObject *watched, QEvent *event)
{
    if(m_listMode && watched==m_button)
    {
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





