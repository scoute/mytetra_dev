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

private slots:

    void onNoteClicked(QListWidgetItem *item);
    void onFavoritesContextMenu(const QPoint &pos);
    void onRemoveFavorite(void);
    void onTreeMetadataSaved(void);
    void onFavoritesChanged(void);

    // Прыжок к заметке через готовый механизм позиционирования.
    // Путь резолвится вживую: ветку могли переместить после starring
    void goToNote(const QString &id);

private:

    void setupUi(void);
    void assembly(void);
    void setupSignals(void);

    // Роль пометки мертвой строки (записи нет в текущей базе)
    enum ItemRoles
    {
        DeadRole=Qt::UserRole+1
    };

    QListWidget *favoritesList=nullptr;

    QVBoxLayout *favoritesLayout=nullptr;

    // Ручной драг был: компактный режим выключен до перезапуска
    bool m_userResized=false;

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
