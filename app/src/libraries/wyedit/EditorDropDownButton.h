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

    // Ограничить видимую высоту меню заданным числом пунктов.
    // Меню при этом становится прокручиваемым (стрелки сверху/снизу).
    // Нужно для длинных меню вроде выбора языка кода
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

private:
    void setupUi();

    void setDividingMargin();

    QToolButton *m_button;
    QMenu *m_menu;

    int m_actionCount=0;
    int m_maxVisibleItems=-1; // Без ограничения, пока не вызовут setMaxVisibleItems
};

#endif // EDITORDROPDOWNBUTTON_H
