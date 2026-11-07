#ifndef _FAVORITESPANEL_H_
#define _FAVORITESPANEL_H_

#include <QWidget>
#include <QStringList>

// Панель избранных заметок над деревом: ручной короткий список для прыжков.
// Данные - хранилище Favorites (сайдкар или поля XML, выбирается
// в настройках). Одинарный клик прыгает (это лаунчер, а не таблица),
// правый клик убирает из списка. Записи без тела (удалены или база
// подменена) висят красными строками и не выкидываются: при возврате
// базы оживают сами

class QListWidget;
class QListWidgetItem;
class QLabel;
class QShowEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMimeData;
class QVBoxLayout;
class QEvent;
class TreeScreen;

class FavoritesPanel : public QWidget
{
    Q_OBJECT

public:

    FavoritesPanel(QWidget *parent=nullptr);
    virtual ~FavoritesPanel(void);

public slots:

    // Пересборка списка. Публичный для обновления извне
    void refreshFavorites(void);

    // Пол панели: шапка плюс две строки. Публичный для стенда
    void updateMinimumHeight(void);

    // Высота списка по строкам с потолком. Публичный для стенда
    void updateListHeight(void);

    // Пин панели под контент. Публичный для стенда
    void updatePanelHeight(void);

    // Посадка секции сплиттера под панель. Публичный для стенда
    void syncSplitterToContent(void);

    // Границу подвинули руками: список на всю высоту панели
    void setUserResized(void);

    // Обновить прямые тексты новым языком
    void retranslateUi(void);

protected:

    // Список пересобирается при каждом показе: могли star/unstar
    void showEvent(QShowEvent *event) override;

    // Бросок записи из таблицы записей: дроп добавляет в избранное
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    // Проверка ноши на формат записей таблицы
    bool canAcceptRecordsDrop(const QMimeData *mimeData) const;

    // Перехват дропов самого списка: внутренний дроп (перестановка)
    // обрабатывается здесь с вычислением строки, внешний (из таблицы
    // записей) - как обычное добавление. Дефолт QListWidget для чужих
    // данных не годится: вставил бы мусор строками
    bool eventFilter(QObject *watched, QEvent *event) override;

    // Порядок списка как есть (включая красные мертвые строки)
    QStringList currentListOrder(void) const;

    // Сдвинуть строку контекстного меню на delta / в край.
    // Работает только в сайдкар-режиме: в xml-режиме порядок -
    // номера favor в mytetra.xml, их не правим
    void moveContextNote(int delta);
    void moveContextNoteToEdge(bool toTop);

private slots:

    void onNoteClicked(QListWidgetItem *item);
    void onNoteDoubleClicked(QListWidgetItem *item);
    void onFavoritesContextMenu(const QPoint &pos);
    void onRemoveFavorite(void);
    void onMoveFavoriteUp(void);
    void onMoveFavoriteDown(void);
    void onMoveFavoriteToTop(void);
    void onMoveFavoriteToBottom(void);
    void onTreeMetadataSaved(void);
    void onFavoritesChanged(void);

    // Прыжок к заметке через готовый механизм позиционирования.
    // Путь резолвится вживую: ветку могли переместить после starring
    void goToNote(const QString &id);

private:

    void setupUi(void);
    void assembly(void);
    void setupSignals(void);

    // Высота строки списка с фолбэком на шрифт
    int favoritesRowHeight(void) const;

    // Роль пометки мертвой строки (записи нет в текущей базе)
    enum ItemRoles
    {
        DeadRole=Qt::UserRole+1
    };

    QListWidget *favoritesList=nullptr;

    QVBoxLayout *favoritesLayout=nullptr;

    // Ручной драг был: компактный режим выключен до перезапуска
    bool m_userResized=false;

    // Программная посадка секции в процессе: splitterMoved от нее
    // ручным драгом не считается
    bool m_syncingSplitter=false;

    // Шапка чтобы панель не висела одиноко: звездочка и слово
    QLabel *headerIcon=nullptr;
    QLabel *headerLabel=nullptr;

    // Подписки делаются один раз и лениво: в конструкторе
    // treeScreen может еще не существовать
    bool treeMetadataConnected;

    // TreeScreen захватывается в конструкторе: позже панель
    // переедет в сплиттер и parentWidget им уже не будет
    TreeScreen *treeScreenRef=nullptr;

    // Строка контекстного меню для пункта "убрать"
    QString contextNoteId;
};

#endif /* _FAVORITESPANEL_H_ */
