#include <QListWidget>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
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
#include <QDebug>

#include <QDesktopServices>
#include <QUrl>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPlainTextEdit>

#include "ContentGallery.h"

#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"
#include "models/recordTable/Record.h"
#include "models/attachTable/AttachTableData.h"
#include "models/appConfig/AppConfig.h"
#include "views/tree/KnowTreeView.h"
#include "views/mainWindow/MainWindow.h"
#include "views/mainWindow/MainWindow.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/GlobalParameters.h"

extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;


// Пункт списка с числовой сортировкой: размер и дата сортируются
// по значению из UserRole, а не по отображаемому тексту
class GallerySortItem : public QTableWidgetItem
{
public:

    explicit GallerySortItem(const QString &text, qlonglong sortValue)
        : QTableWidgetItem(text)
    {
        setData(Qt::UserRole, sortValue);
    }

    bool operator<(const QTableWidgetItem &other) const override
    {
        return data(Qt::UserRole).toLongLong() < other.data(Qt::UserRole).toLongLong();
    }
};


ContentGallery::ContentGallery(GalleryMode mode, QWidget *parent) : QDialog(parent),
    galleryMode(mode)
{
    if(galleryMode==GalleryMode::Images)
        setWindowTitle(tr("Images gallery"));
    else
        setWindowTitle(tr("Attached files"));

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

    // Вид: плитка или список. Список умеет сортировку по всем колонкам
    viewCombo=new QComboBox(this);
    viewCombo->addItem(tr("Tiles"));
    viewCombo->addItem(tr("List"));

    QHBoxLayout *topLayout=new QHBoxLayout();
    topLayout->addWidget(tileCountLabel);
    topLayout->addWidget(tileSlider, 1);
    topLayout->addWidget(viewCombo);
    topLayout->addWidget(galleryCountLabel);

    imageGrid=new QListWidget(this);
    imageGrid->setViewMode(QListWidget::IconMode);
    imageGrid->setResizeMode(QListWidget::Adjust);
    imageGrid->setMovement(QListWidget::Static);
    imageGrid->setSelectionMode(QAbstractItemView::SingleSelection);

    // Список: имя, размер, тип, дата. Сортировка кликом по заголовку
    filesList=new QTableWidget(this);
    filesList->setColumnCount(4);
    filesList->setHorizontalHeaderLabels(QStringList() << tr("Name") << tr("Size") << tr("Type") << tr("Modified"));
    filesList->verticalHeader()->setVisible(false);
    filesList->setSelectionBehavior(QAbstractItemView::SelectRows);
    filesList->setSelectionMode(QAbstractItemView::SingleSelection);
    filesList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    filesList->setSortingEnabled(true);
    filesList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    filesList->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    filesList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    filesList->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    viewStack=new QStackedWidget(this);
    viewStack->addWidget(imageGrid);
    viewStack->addWidget(filesList);

    QVBoxLayout *centralLayout=new QVBoxLayout(this);
    centralLayout->addLayout(topLayout);
    centralLayout->addWidget(viewStack, 1);

    setLayout(centralLayout);

    connect(viewCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this,      &ContentGallery::onViewModeChanged);

    connect(tileSlider, &QSlider::valueChanged,
            this,       &ContentGallery::onTileColumnsChanged);

    connect(imageGrid->verticalScrollBar(), &QScrollBar::valueChanged,
            this,                           &ContentGallery::onScrollChanged);

    connect(imageGrid, &QListWidget::itemClicked,
            this,      &ContentGallery::onItemClicked);

    connect(imageGrid, &QListWidget::itemDoubleClicked,
            this,      &ContentGallery::onItemDoubleClicked);

    connect(filesList, &QTableWidget::cellClicked,
            this,      &ContentGallery::onListCellClicked);

    connect(filesList, &QTableWidget::cellDoubleClicked,
            this,      &ContentGallery::onListCellDoubleClicked);
}


ContentGallery::~ContentGallery(void)
{

}


// Число плиток в строке по позиции ползунка
int ContentGallery::tileColumns(int sliderPos)
{
    static const int columns[]={4, 6, 8, 12, 16};

    if(sliderPos<0)
        sliderPos=0;

    if(sliderPos>4)
        sliderPos=4;

    return columns[sliderPos];
}


// Размер плитки по ширине вьюпорта и числу колонок
int ContentGallery::tileSize(void) const
{
    int viewportWidth=imageGrid->viewport()->width();

    if(viewportWidth<=0)
        viewportWidth=imageGrid->width();

    int tile=viewportWidth/tileColumns(tileSlider->value());

    if(tile<32)
        tile=32;

    return tile;
}


void ContentGallery::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    refreshGallery();
}


void ContentGallery::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);

    layoutGrid();
}


void ContentGallery::onTileColumnsChanged(int sliderPos)
{
    tileCountLabel->setText(QString::number(tileColumns(sliderPos)));

    layoutGrid();
}


// Переключение плитки и списка. Слайдер работает только в плитке
void ContentGallery::onViewModeChanged(int comboIndex)
{
    viewStack->setCurrentIndex(comboIndex);
    tileSlider->setEnabled(comboIndex==0);
    tileCountLabel->setEnabled(comboIndex==0);
}


void ContentGallery::onScrollChanged(void)
{
    updateVisibleWindow();
}


// Перестроить сетку и подгрузить видимое окно
void ContentGallery::layoutGrid(void)
{
    const int tile=tileSize();

    imageGrid->setIconSize(QSize(tile, tile));
    imageGrid->setGridSize(QSize(tile+8, tile+8+24));

    updateVisibleWindow();
}


// Собрать галерею заново проходом по базе
void ContentGallery::refreshGallery(void)
{
    galleryImages.clear();
    imageGrid->clear();

    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

    if(treeView!=nullptr)
    {
        KnowTreeModel *treeModel=static_cast<KnowTreeModel *>(treeView->model());
        const TreeItem *rootItem=treeModel->getRootItem();

        if(rootItem!=nullptr)
        {
            if(galleryMode==GalleryMode::Images)
                collectGalleryImages(rootItem, QStringList(), galleryImages);
            else
                collectAttachedFiles(rootItem, QStringList(), galleryImages);
        }
    }

    for(int i=0; i<galleryImages.size(); ++i)
    {
        const GalleryImage &image=galleryImages.at(i);

        // В режиме файлов подпись имя файла, в режиме картинок имя заметки
        const QString itemText=(galleryMode==GalleryMode::Images)
                               ? image.noteName : image.fileName;

        QListWidgetItem *item=new QListWidgetItem(itemText, imageGrid);
        item->setData(Qt::UserRole, i);
        item->setToolTip(image.noteName+"\n"+image.imagePath);
    }

    if(galleryMode==GalleryMode::Images)
        galleryCountLabel->setText(tr("%1 images").arg(galleryImages.size()));
    else
        galleryCountLabel->setText(tr("%1 files").arg(galleryImages.size()));

    fillFilesList();

    layoutGrid();
}


// Собрать картинки базы проходом по дереву в порядке веток.
// Зашифрованные ветки без пароля пропускаются как в поиске
void ContentGallery::collectGalleryImages(const TreeItem *curritem,
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
                image.fileName=imageFiles.at(f);
                images.append(image);
            }
        }
    }

    for(int i=0; i<curritem->childCount(); i++)
        collectGalleryImages(curritem->child(i), itemPath, images);
}


// Собрать прикрепленные файлы базы тем же проходом.
// Только файлы на диске: ссылки без локального файла пропускаются.
// Таблица берется указателем: копирование AttachTableData
// по значению оставляет висячие ссылки
void ContentGallery::collectAttachedFiles(const TreeItem *curritem,
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
        // Константность снимается: файлы только читаются
        RecordTableData *recordTable=const_cast<TreeItem *>(curritem)->recordtableGetTableData();

        for(int i=0; i<static_cast<int>(recordTable->size()); i++)
        {
            Record *record=recordTable->getRecord(i);

            if(record==nullptr)
                continue;

            AttachTableData *attachTable=record->getAttachTablePointer();

            for(int a=0; a<attachTable->size(); ++a)
            {
                const QString diskPath=attachTable->getAbsoluteInnerFileName(a);

                if(!QFileInfo(diskPath).isFile())
                    continue;

                GalleryImage image;
                image.imagePath=diskPath;
                image.recordId=recordTable->getField("id", i);
                image.branchPath=itemPath;
                image.noteName=recordTable->getField("name", i);
                image.fileName=attachTable->getFileName(a);
                images.append(image);
            }
        }
    }

    for(int i=0; i<curritem->childCount(); i++)
        collectAttachedFiles(curritem->child(i), itemPath, images);
}


// Графические файлы каталога записи, кроме текста заметки.
// Имена файлов на диске, а не отображаемые: у вставленных картинок
// имена технические, их и показываем как есть
QStringList ContentGallery::recordImageFiles(const QString &recordDir)
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
void ContentGallery::updateVisibleWindow(void)
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
void ContentGallery::loadItemIcon(QListWidgetItem *item, int tile)
{
    if(item==nullptr)
        return;

    const int index=item->data(Qt::UserRole).toInt();

    if(index<0 || index>=galleryImages.size())
        return;

    const QString imagePath=galleryImages.at(index).imagePath;

    // Нераскрываемый формат: иконка типа вместо тумбы.
    // В режиме файлов так рисуются документы, в режиме картинок
    // битые файлы
    QImageReader probeReader(imagePath);

    if(!probeReader.canRead())
    {
        static QIcon fileIcon(QStringLiteral(":/resource/pic/attach_is_file.svg"));
        item->setIcon(fileIcon);
        return;
    }

    QImageReader reader(imagePath);

    if(!reader.canRead())
    {
        static QIcon brokenIcon(QStringLiteral(":/resource/pic/attach_is_file.svg"));
        item->setIcon(brokenIcon);
        return;
    }

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
    {
        static QIcon brokenReadIcon(QStringLiteral(":/resource/pic/attach_is_file.svg"));
        item->setIcon(brokenReadIcon);
        return;
    }

    item->setIcon(QIcon(QPixmap::fromImage(image)));
}


// Клик прыгает в заметку с картинкой
void ContentGallery::onItemClicked(QListWidgetItem *item)
{
    if(item==nullptr)
        return;

    const int index=item->data(Qt::UserRole).toInt();

    if(index<0 || index>=galleryImages.size())
        return;

    jumpToImage(galleryImages.at(index));
}


// Прыгнуть в заметку по данным пункта
void ContentGallery::jumpToImage(const GalleryImage &image) const
{
    MainWindow *mainWindow=find_object<MainWindow>("mainwindow");

    if(mainWindow==nullptr)
        return;

    mainWindow->setTreeAndRecordtablePositions(image.branchPath, image.recordId);
}


// Клик в списке прыгает в заметку
void ContentGallery::onListCellClicked(int row, int column)
{
    Q_UNUSED(column);

    QTableWidgetItem *item=filesList->item(row, 0);

    if(item==nullptr)
        return;

    jumpToImage(galleryImageFromItem(item));
}


// Двойной клик в списке открывает по режиму
void ContentGallery::onListCellDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    QTableWidgetItem *item=filesList->item(row, 0);

    if(item==nullptr)
        return;

    openGalleryImage(galleryImageFromItem(item));
}


// Собрать данные пункта списка обратно в структуру.
// Индексы строк после сортировки не годятся, поэтому путь,
// запись и файл хранятся прямо в пункте
GalleryImage ContentGallery::galleryImageFromItem(QTableWidgetItem *item) const
{
    GalleryImage image;

    if(item==nullptr)
        return image;

    image.branchPath=item->data(Qt::UserRole).toStringList();
    image.recordId=item->data(Qt::UserRole+1).toString();
    image.imagePath=item->data(Qt::UserRole+2).toString();
    image.noteName=item->text();

    return image;
}


// Двойной клик: картинку крупно, файл открыть
void ContentGallery::onItemDoubleClicked(QListWidgetItem *item)
{
    if(item==nullptr)
        return;

    const int index=item->data(Qt::UserRole).toInt();

    if(index<0 || index>=galleryImages.size())
        return;

    openGalleryImage(galleryImages.at(index));
}


// Открыть по режиму: картинку в просмотрщик, файл наружу.
// Текстовый файл показывается встроенным просмотрщиком текста
void ContentGallery::openGalleryImage(const GalleryImage &image) const
{
    if(galleryMode==GalleryMode::Images)
    {
        openImageViewer(image);
        return;
    }

    QMimeDatabase mimeDatabase;
    QMimeType mimeType=mimeDatabase.mimeTypeForFile(image.imagePath);

    if(mimeType.inherits(QStringLiteral("text/plain")))
        openTextViewer(image);
    else
        openAttachedFile(image);
}


// Открыть прикрепленный файл системным обработчиком.
// Путь абсолютный: относительный file-URL файловый менеджер не открывает
void ContentGallery::openAttachedFile(const GalleryImage &image) const
{
    if(!QFileInfo(image.imagePath).isFile())
    {
        QMessageBox::information(const_cast<ContentGallery *>(this),
                                 tr("Attached files"),
                                 tr("Can not open file: %1").arg(image.imagePath));
        return;
    }

    if(!QDesktopServices::openUrl(QUrl::fromLocalFile(image.imagePath)))
    {
        QMessageBox::information(const_cast<ContentGallery *>(this),
                                 tr("Attached files"),
                                 tr("Can not open file: %1").arg(image.imagePath));
    }
}


// Показать текстовый файл встроенным просмотрщиком.
// Читаются первые 200КБ как UTF-8: бинарные форматы (pdf, doc)
// так не открыть, для них нет зависимостей
void ContentGallery::openTextViewer(const GalleryImage &image) const
{
    QFile file(image.imagePath);

    if(!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::information(const_cast<ContentGallery *>(this),
                                 tr("Attached files"),
                                 tr("Can not open file: %1").arg(image.imagePath));
        return;
    }

    const QString textContent=QString::fromUtf8(file.read(200*1024));
    file.close();

    QDialog *viewer=new QDialog(const_cast<ContentGallery *>(this));
    viewer->setAttribute(Qt::WA_DeleteOnClose);
    viewer->setWindowTitle(image.fileName.isEmpty() ? image.noteName : image.fileName);
    viewer->resize(640, 480);

    QPlainTextEdit *textView=new QPlainTextEdit(viewer);
    textView->setPlainText(textContent);
    textView->setReadOnly(true);

    QPushButton *closeButton=new QPushButton(tr("Close"), viewer);
    connect(closeButton, &QPushButton::clicked, viewer, &QDialog::accept);

    QVBoxLayout *viewerLayout=new QVBoxLayout(viewer);
    viewerLayout->addWidget(textView, 1);
    viewerLayout->addWidget(closeButton);

    viewer->setLayout(viewerLayout);
    viewer->exec();
}


// Открыть картинку крупно в модальном просмотрщике с прокруткой
void ContentGallery::openImageViewer(const GalleryImage &image) const
{
    QPixmap pixmap(image.imagePath);

    if(pixmap.isNull())
    {
        QMessageBox::information(const_cast<ContentGallery *>(this),
                                 tr("Images gallery"),
                                 tr("Can not open image: %1").arg(image.imagePath));
        return;
    }

    QDialog *viewer=new QDialog(const_cast<ContentGallery *>(this));
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


// Заполнить список файлов: имя, размер, тип, дата. Сортировка
// кликом по заголовку, числа и даты сортируются как числа.
// Данные для прыжка и открытия хранятся в пунктах: индексы строк
// после сортировки не годятся
void ContentGallery::fillFilesList(void)
{
    filesList->setSortingEnabled(false);
    filesList->setRowCount(0);
    filesList->setRowCount(galleryImages.size());

    static QIcon fileIcon(QStringLiteral(":/resource/pic/attach_is_file.svg"));

    for(int i=0; i<galleryImages.size(); ++i)
    {
        const GalleryImage &image=galleryImages.at(i);
        const QFileInfo fileInfo(image.imagePath);

        QTableWidgetItem *nameItem=new QTableWidgetItem(fileIcon, image.fileName.isEmpty() ? image.noteName : image.fileName);
        nameItem->setData(Qt::UserRole, image.branchPath);
        nameItem->setData(Qt::UserRole+1, image.recordId);
        nameItem->setData(Qt::UserRole+2, image.imagePath);
        nameItem->setToolTip(image.noteName+"\n"+image.imagePath);
        filesList->setItem(i, 0, nameItem);

        const qint64 fileSize=fileInfo.exists() ? fileInfo.size() : 0;
        GallerySortItem *sizeItem=new GallerySortItem(formatFileSize(fileSize), fileSize);
        filesList->setItem(i, 1, sizeItem);

        const QString fileType=fileInfo.suffix().toLower();
        QTableWidgetItem *typeItem=new QTableWidgetItem(fileType);
        filesList->setItem(i, 2, typeItem);

        const qint64 modifiedMsecs=fileInfo.exists()
            ? fileInfo.lastModified().toMSecsSinceEpoch() : 0;
        GallerySortItem *dateItem=new GallerySortItem(
            fileInfo.exists()
            ? fileInfo.lastModified().toString(QStringLiteral("yyyy-MM-dd hh:mm"))
            : QString(),
            modifiedMsecs);
        filesList->setItem(i, 3, dateItem);
    }

    filesList->resizeColumnsToContents();
    filesList->setSortingEnabled(true);
}


// Человекочитаемый размер файла
QString ContentGallery::formatFileSize(qint64 bytes)
{
    if(bytes<1024)
        return QString::number(bytes);

    if(bytes<1024*1024)
        return QString::number(bytes/1024)+QStringLiteral(" KB");

    return QString::number(bytes/(1024*1024))+QStringLiteral(" MB");
}
