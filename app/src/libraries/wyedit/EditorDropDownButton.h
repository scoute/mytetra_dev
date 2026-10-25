#ifndef EDITORDROPDOWNBUTTON_H
#define EDITORDROPDOWNBUTTON_H

#include <QObject>
#include <QWidget>
#include <QColor>
#include <QAction>

#include "EditorToolPseudoButton.h"


class QToolButton;
class QMenu;


class EditorDropDownButton : public QWidget
{
    Q_OBJECT

public:
    explicit EditorDropDownButton(QWidget *parent = nullptr);

    void setText(const QString &text);
    void addAction(QAction *action);
    void setIcon(const QIcon &icon);

    // Переключение кнопки в режим "нажал - открылось меню".
    // Нужно для кнопки выбора языка кода: и клик по иконке, и хоткей
    // должны сразу показывать меню, а не применять действие по умолчанию
    void setInstantPopup(bool enable=true);

    // Режим списка: вместо QMenu показывается всплывающий QListWidget
    // со штатной полосой прокрутки. Нужен для длинных меню (выбор языка),
    // так как QMenu умеет прокручиваться только у краев экрана, а
    // maximumHeight его просто обрезает без стрелок
    void setListMode(bool enable=true);

    // Видимая высота всплывающего списка в пунктах (только list-режим).
    // Остальные пункты доступны прокруткой
    void setMaxVisibleItems(int count);

    EditorToolPseudoButton toolPseudoButton;

    void showMenu();

signals:
    void menuItemClicked(int num);

    // Клик по самой кнопке (не по стрелке меню). В режиме
    // MenuButtonPopup стрелка открывает меню и clicked не дергает
    void buttonClicked();

private slots:
    void onMenuItemClick(int num);

protected:
    // В list-режиме клик мыши и Space/Enter по кнопке должны показывать
    // список, а не внутреннее QMenu кнопки (QToolButton с InstantPopup
    // открывает меню приватным путем, мимо showMenu())
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void showListMenu(void); // Всплывающий список для list-режима

    void setDividingMargin();

    QToolButton *m_button;
    QMenu *m_menu;

    int m_actionCount=0;

    bool m_listMode=false; // Показывать QListWidget вместо QMenu
    int m_maxVisibleItems=-1; // Видимых пунктов в list-режиме
};

#endif // EDITORDROPDOWNBUTTON_H
