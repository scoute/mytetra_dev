#ifndef _IMAGESGALLERY_H_
#define _IMAGESGALLERY_H_

#include <QDialog>
#include <QStringList>

// Окно галереи картинок базы: плитка уменьшенных изображений,
// клик прыгает в заметку, двойной клик открывает картинку крупно.
// В памяти только видимое окно плюс по 20 картинок вперед-назад,
// остальное выгружается. Отдельным большим окном, а не доком

class QListWidget;
class QListWidgetItem;
class QSlider;
class QLabel;
class TreeItem;

struct GalleryImage
{
    QString imagePath;
    QString recordId;
    QStringList branchPath;
    QString noteName;
};

class ImagesGallery : public QDialog
{
    Q_OBJECT

public:

    ImagesGallery(QWidget *parent=nullptr);
    virtual ~ImagesGallery(void);

    // Собрать картинки базы проходом по дереву в порядке веток.
    // Зашифрованные ветки без пароля пропускаются как в поиске
    static void collectGalleryImages(const TreeItem *curritem,
                                     const QStringList &branchPath,
                                     QList<GalleryImage> &images);

    // Графические файлы каталога записи, кроме текста заметки
    static QStringList recordImageFiles(const QString &recordDir);

private slots:

    void onTileColumnsChanged(int sliderPos);
    void onScrollChanged(void);
    void onItemClicked(QListWidgetItem *item);
    void onItemDoubleClicked(QListWidgetItem *item);
    void refreshGallery(void);

protected:

    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:

    // Число плиток в строке по позиции ползунка
    static int tileColumns(int sliderPos);

    // Размер плитки по ширине вьюпорта и числу колонок
    int tileSize(void) const;

    // Перестроить сетку и подгрузить видимое окно
    void layoutGrid(void);

    // Подгрузить [первый-20, последний+20], остальное выгрузить
    void updateVisibleWindow(void);

    // Загрузить уменьшенную картинку в пункт
    void loadItemIcon(QListWidgetItem *item, int tile);

    // Открыть картинку крупно в модальном просмотрщике
    void openImageViewer(const GalleryImage &image) const;

    // Строка плитки, заголовок и слайдер
    QListWidget *imageGrid;
    QSlider *tileSlider;
    QLabel *tileCountLabel;
    QLabel *galleryCountLabel;

    QList<GalleryImage> galleryImages;

    // Сколько картинок держать вокруг видимых с каждой стороны
    static const int prefetchImages=20;
};

#endif /* _IMAGESGALLERY_H_ */
