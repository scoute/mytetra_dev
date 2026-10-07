#ifndef _FAVORITESPANEL_H_
#define _FAVORITESPANEL_H_

#include <QWidget>

// Панель избранных заметок над деревом: ручной короткий список для прыжков.
// Данные - флаг favorite в XML записей. Одинарный клик
// прыгает (это лаунчер, а не таблица), правый клик убирает из списка.
// Удаленные заметки тихо пропадают при пересборке

class QListWidget;
class QListWidgetItem;
class QLabel;
class QShowEvent;

class FavoritesPanel : public QWidget
{
    Q_OBJECT

public:

    FavoritesPanel(QWidget *parent=nullptr);
    virtual ~FavoritesPanel(void);

public slots:

    // Пересборка списка. Публичный для обновления извне
    void refreshFavorites(void);

protected:

    // Список пересобирается при каждом показе: могли star/unstar
    void showEvent(QShowEvent *event) override;

private slots:

    void onNoteClicked(QListWidgetItem *item);
    void onFavoritesContextMenu(const QPoint &pos);
    void onRemoveFavorite(void);
    void onTreeMetadataSaved(void);

    // Прыжок к заметке через готовый механизм позиционирования.
    // Путь резолвится вживую: ветку могли переместить после starring
    void goToNote(const QString &id);

private:

    void setupUi(void);
    void assembly(void);
    void setupSignals(void);

    QListWidget *favoritesList=nullptr;

    // Шапка чтобы панель не висела одиноко: звездочка и слово
    QLabel *headerIcon=nullptr;
    QLabel *headerLabel=nullptr;

    // Подписки делаются один раз и лениво: в конструкторе
    // treeScreen может еще не существовать
    bool treeMetadataConnected;

    // Строка контекстного меню для пункта "убрать"
    QString contextNoteId;
};

#endif /* _FAVORITESPANEL_H_ */
