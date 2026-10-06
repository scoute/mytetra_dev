#include <QListWidget>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QShowEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QImageReader>
#include <QPixmap>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QScrollArea>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QDebug>

#include "ImagesGallery.h"

#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"
#include "models/appConfig/AppConfig.h"
#include "views/tree/KnowTreeView.h"
#include "views/mainWindow/MainWindow.h"
#include "views/mainWindow/MainWindow.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/GlobalParameters.h"

extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;


ImagesGallery::ImagesGallery(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Images gallery"));
    resize(800, 600);

    // Число плиток в строке: 4-6-8-12-16, по умолчанию 6
    tileSlider=new QSlider(Qt::Horizontal, this);
    tileSlider->setMinimum(0);
    tileSlider->setMaximum(4);
    tileSlider->setValue(1);
    tileSlider->setTickPosition(QSlider::TicksBelow);
    tileSlider->setToolTip(tr("Tiles per row"));

    tileCountLabel=new QLabel(QString::number(tileColumns(1)), this);
    galleryCountLabel=new QLabel(this);

    QHBoxLayout *topLayout=new QHBoxLayout();
    topLayout->addWidget(tileCountLabel);
    topLayout->addWidget(tileSlider, 1);
    topLayout->addWidget(galleryCountLabel);

    imageGrid=new QListWidget(this);
    imageGrid->setViewMode(QListWidget::IconMode);
    imageGrid->setResizeMode(QListWidget::Adjust);
    imageGrid->setMovement(QListWidget::Static);
    imageGrid->setSelectionMode(QAbstractItemView::SingleSelection);

    QVBoxLayout *centralLayout=new QVBoxLayout(this);
    centralLayout->addLayout(topLayout);
    centralLayout->addWidget(imageGrid, 1);

    setLayout(centralLayout);

    connect(tileSlider, &QSlider::valueChanged,
            this,       &ImagesGallery::onTileColumnsChanged);

    connect(imageGrid->verticalScrollBar(), &QScrollBar::valueChanged,
            this,                           &ImagesGallery::onScrollChanged);

    connect(imageGrid, &QListWidget::itemClicked,
            this,      &ImagesGallery::onItemClicked);

    connect(imageGrid, &QListWidget::itemDoubleClicked,
            this,      &ImagesGallery::onItemDoubleClicked);
}


ImagesGallery::~ImagesGallery(void)
{

}


// Число плиток в строке по позиции ползунка
int ImagesGallery::tileColumns(int sliderPos)
{
    static const int columns[]={4, 6, 8, 12, 16};

    if(sliderPos<0)
        sliderPos=0;

    if(sliderPos>4)
        sliderPos=4;

    return columns[sliderPos];
}


// Размер плитки по ширине вьюпорта и числу колонок
int ImagesGallery::tileSize(void) const
{
    int viewportWidth=imageGrid->viewport()->width();

    if(viewportWidth<=0)
        viewportWidth=imageGrid->width();

    int tile=viewportWidth/tileColumns(tileSlider->value());

    if(tile<32)
        tile=32;

    return tile;
}


void ImagesGallery::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    refreshGallery();
}


void ImagesGallery::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);

    layoutGrid();
}


void ImagesGallery::onTileColumnsChanged(int sliderPos)
{
    tileCountLabel->setText(QString::number(tileColumns(sliderPos)));

    layoutGrid();
}


void ImagesGallery::onScrollChanged(void)
{
    updateVisibleWindow();
}


// Перестроить сетку и подгрузить видимое окно
void ImagesGallery::layoutGrid(void)
{
    const int tile=tileSize();

    imageGrid->setIconSize(QSize(tile, tile));
    imageGrid->setGridSize(QSize(tile+8, tile+8+24));

    updateVisibleWindow();
}


// Собрать галерею заново проходом по базе
void ImagesGallery::refreshGallery(void)
{
    galleryImages.clear();
    imageGrid->clear();

    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

    if(treeView!=nullptr)
    {
        KnowTreeModel *treeModel=static_cast<KnowTreeModel *>(treeView->model());
        const TreeItem *rootItem=treeModel->getRootItem();

        if(rootItem!=nullptr)
            collectGalleryImages(rootItem, QStringList(), galleryImages);
    }

    for(int i=0; i<galleryImages.size(); ++i)
    {
        const GalleryImage &image=galleryImages.at(i);

        QListWidgetItem *item=new QListWidgetItem(image.noteName, imageGrid);
        item->setData(Qt::UserRole, i);
        item->setToolTip(image.noteName+"\n"+image.imagePath);
    }

    galleryCountLabel->setText(tr("%1 images").arg(galleryImages.size()));

    layoutGrid();
}


// Собрать картинки базы проходом по дереву в порядке веток.
// Зашифрованные ветки без пароля пропускаются как в поиске
void ImagesGallery::collectGalleryImages(const TreeItem *curritem,
                                         const QStringList &branchPath,
                                         QList<GalleryImage> &images)
{
    if(curritem==nullptr)
        return;

    // Зашифрованная ветка без введенного пароля недоступна
    if(curritem->getField("crypt")=="1" &&
       globalParameters.getCryptKey().length()==0)
        return;

    QStringList itemPath=branchPath;
    const QString itemId=curritem->getField("id");

    if(!itemId.isEmpty())
        itemPath << itemId;

    if(curritem->recordtableGetRowCount() > 0)
    {
        const RecordTableData *recordTable=curritem->recordtableGetTableData();

        for(int i=0; i<static_cast<int>(recordTable->size()); i++)
        {
            const QString recordDir=mytetraConfig.get_tetradir()
                                    +"/base/"
                                    +recordTable->getField("dir", i);

            const QStringList imageFiles=recordImageFiles(recordDir);

            for(int f=0; f<imageFiles.size(); ++f)
            {
                GalleryImage image;
                image.imagePath=recordDir+"/"+imageFiles.at(f);
                image.recordId=recordTable->getField("id", i);
                image.branchPath=itemPath;
                image.noteName=recordTable->getField("name", i);
                images.append(image);
            }
        }
    }

    for(int i=0; i<curritem->childCount(); i++)
        collectGalleryImages(curritem->child(i), itemPath, images);
}


// Графические файлы каталога записи, кроме текста заметки.
// Имена файлов на диске, а не отображаемые: у вставленных картинок
// имена технические, их и показываем как есть
QStringList ImagesGallery::recordImageFiles(const QString &recordDir)
{
    QDir dir(recordDir);

    if(!dir.exists())
        return QStringList();

    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.gif" << "*.bmp";

    QStringList files=dir.entryList(filters, QDir::Files | QDir::Readable, QDir::Name);

    // Текст заметки картинкой не является
    files.removeAll(QStringLiteral("text.html"));

    return files;
}


// Подгрузить [первый-20, последний+20], остальное выгрузить.
// В памяти только видимое окно с запасом, старые выгружаются
void ImagesGallery::updateVisibleWindow(void)
{
    if(imageGrid->count()==0)
        return;

    const int tile=tileSize();

    // Границы видимого: первая и последняя строки через углы вьюпорта.
    // Угол может попасть в пустое место сетки: тогда оценка по геометрии
    int firstRow=imageGrid->row(imageGrid->itemAt(2, 2));
    int lastRow=imageGrid->row(imageGrid->itemAt(imageGrid->viewport()->width()-4,
                                                  imageGrid->viewport()->height()-4));

    if(firstRow<0)
        firstRow=0;

    if(lastRow<firstRow)
    {
        const int rowHeight=imageGrid->gridSize().height();
        const int columnWidth=imageGrid->gridSize().width();
        const int visibleRows=(rowHeight>0)
            ? imageGrid->viewport()->height()/rowHeight+2 : 4;
        const int columns=(columnWidth>0)
            ? qMax(1, imageGrid->viewport()->width()/columnWidth) : 4;
        lastRow=firstRow+visibleRows*columns;
    }

    const int firstVisible=firstRow;
    int lastVisible=lastRow;

    if(lastVisible>=imageGrid->count())
        lastVisible=imageGrid->count()-1;

    int loadFirst=firstVisible-prefetchImages;
    int loadLast=lastVisible+prefetchImages;

    if(loadFirst<0)
        loadFirst=0;

    if(loadLast>=imageGrid->count())
        loadLast=imageGrid->count()-1;

    for(int row=0; row<imageGrid->count(); ++row)
    {
        QListWidgetItem *item=imageGrid->item(row);

        if(item==nullptr)
            continue;

        if(row>=loadFirst && row<=loadLast)
        {
            // В окне: подгрузить если пусто
            if(item->icon().isNull())
                loadItemIcon(item, tile);
        }
        else
        {
            // Вне окна: выгрузить
            if(!item->icon().isNull())
                item->setIcon(QIcon());
        }
    }
}


// Загрузить уменьшенную картинку в пункт.
// Чтение идет через QImageReader сразу в размер плитки:
// полный кадр в память не поднимается
void ImagesGallery::loadItemIcon(QListWidgetItem *item, int tile)
{
    if(item==nullptr)
        return;

    const int index=item->data(Qt::UserRole).toInt();

    if(index<0 || index>=galleryImages.size())
        return;

    const QString imagePath=galleryImages.at(index).imagePath;

    QImageReader reader(imagePath);

    if(!reader.canRead())
        return;

    QSize sourceSize=reader.size();

    if(!sourceSize.isValid() || sourceSize.isEmpty())
        return;

    const qreal scale=qMin(static_cast<qreal>(tile)/static_cast<qreal>(sourceSize.width()),
                           static_cast<qreal>(tile)/static_cast<qreal>(sourceSize.height()));

    QSize scaledSize(static_cast<int>(sourceSize.width()*scale),
                     static_cast<int>(sourceSize.height()*scale));

    if(scaledSize.width()<1)
        scaledSize.setWidth(1);

    if(scaledSize.height()<1)
        scaledSize.setHeight(1);

    reader.setScaledSize(scaledSize);

    const QImage image=reader.read();

    if(image.isNull())
        return;

    item->setIcon(QIcon(QPixmap::fromImage(image)));
}


// Клик прыгает в заметку с картинкой
void ImagesGallery::onItemClicked(QListWidgetItem *item)
{
    if(item==nullptr)
        return;

    const int index=item->data(Qt::UserRole).toInt();

    if(index<0 || index>=galleryImages.size())
        return;

    const GalleryImage &image=galleryImages.at(index);

    MainWindow *mainWindow=find_object<MainWindow>("mainwindow");

    if(mainWindow==nullptr)
        return;

    mainWindow->setTreeAndRecordtablePositions(image.branchPath, image.recordId);
}


// Двойной клик открывает картинку крупно
void ImagesGallery::onItemDoubleClicked(QListWidgetItem *item)
{
    if(item==nullptr)
        return;

    const int index=item->data(Qt::UserRole).toInt();

    if(index<0 || index>=galleryImages.size())
        return;

    openImageViewer(galleryImages.at(index));
}


// Открыть картинку крупно в модальном просмотрщике с прокруткой
void ImagesGallery::openImageViewer(const GalleryImage &image) const
{
    QPixmap pixmap(image.imagePath);

    if(pixmap.isNull())
    {
        QMessageBox::information(const_cast<ImagesGallery *>(this),
                                 tr("Images gallery"),
                                 tr("Can not open image: %1").arg(image.imagePath));
        return;
    }

    QDialog *viewer=new QDialog(const_cast<ImagesGallery *>(this));
    viewer->setAttribute(Qt::WA_DeleteOnClose);
    viewer->setWindowTitle(image.noteName);
    viewer->resize(640, 480);

    QLabel *imageLabel=new QLabel(viewer);
    imageLabel->setPixmap(pixmap);
    imageLabel->resize(pixmap.size());

    QScrollArea *scrollArea=new QScrollArea(viewer);
    scrollArea->setWidget(imageLabel);
    scrollArea->setWidgetResizable(false);
    scrollArea->setAlignment(Qt::AlignCenter);

    QPushButton *closeButton=new QPushButton(tr("Close"), viewer);
    connect(closeButton, &QPushButton::clicked, viewer, &QDialog::accept);

    QVBoxLayout *viewerLayout=new QVBoxLayout(viewer);
    viewerLayout->addWidget(scrollArea, 1);
    viewerLayout->addWidget(closeButton);

    viewer->setLayout(viewerLayout);
    viewer->exec();
}
