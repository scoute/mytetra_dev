#include <QScrollBar>
#include <QTextDocument>
#include <QPalette>
#include <QPainter>
#include <QPaintEvent>
#include <QColor>
#include <QDebug>
#include <QAbstractTextDocumentLayout>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QListWidgetItem>
#include <QIcon>
#include <QPixmap>
#include <QImage>

#include "PreviewView.h"


PreviewView::PreviewView(QTextDocument *document)
{
    verticalScrollBar()->setSingleStep(20);
    horizontalScrollBar()->setSingleStep(20);

    viewport()->setBackgroundRole(QPalette::Dark);

    doc = document;
    scale = 1.0;
    interPageSpacing = 30;
}


void PreviewView::zoomIn()
{
    scale += 0.2;
    resizeEvent(0);
    viewport()->update();
}


void PreviewView::zoomOut()
{
    scale -= 0.2;
    resizeEvent(0);
    viewport()->update();
}


void PreviewView::zoomReset()
{
    // Обычный размер: масштаб 1.0, как при открытии предпросмотра
    scale = 1.0;
    resizeEvent(0);
    viewport()->update();
}


void PreviewView::paintEvent(QPaintEvent *)
{
    QPainter p(viewport());

    p.translate(-horizontalScrollBar()->value(), -verticalScrollBar()->value());
    p.translate(interPageSpacing, interPageSpacing);

    const int pages = doc->pageCount();
    for (int i = 0; i < pages; ++i) {
        p.save();
        p.scale(scale, scale);

        paintPage(&p, i);

        p.restore();
        p.translate(0, interPageSpacing + doc->pageSize().height() * scale);
    }
}


void PreviewView::paintPage(QPainter *painter, int page)
{
    const QSizeF pgSize = doc->pageSize();

    QColor col(Qt::black);

    painter->setPen(col);
    painter->setBrush(Qt::white);
    painter->drawRect(QRectF(QPointF(0, 0), pgSize));
    painter->setBrush(Qt::NoBrush);

    col = col.lighter();
    painter->drawLine(QLineF(pgSize.width(), 1, pgSize.width(), pgSize.height() - 1));

    col = col.lighter();
    painter->drawLine(QLineF(pgSize.width(), 2, pgSize.width(), pgSize.height() - 2));

    QRectF docRect(QPointF(0, page * pgSize.height()), pgSize);
    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.clip = docRect;

    // don't use the system palette text as default text color, on HP/UX
    // for example that's white, and white text on white paper doesn't
    // look that nice
    ctx.palette.setColor(QPalette::Text, Qt::black);

    painter->translate(0, - page * pgSize.height());
    painter->setClipRect(docRect);
    doc->documentLayout()->draw(painter, ctx);
}


void PreviewView::resizeEvent(QResizeEvent *)
{
    const QSize viewportSize = viewport()->size();

    QSize docSize;
    docSize.setWidth(qRound(doc->pageSize().width() * scale + 2 * interPageSpacing));
    const int pageCount = doc->pageCount();
    docSize.setHeight(qRound(pageCount * doc->pageSize().height() * scale + (pageCount + 1) * interPageSpacing));

    horizontalScrollBar()->setRange(0, docSize.width() - viewportSize.width());
    horizontalScrollBar()->setPageStep(viewportSize.width());

    verticalScrollBar()->setRange(0, docSize.height() - viewportSize.height());
    verticalScrollBar()->setPageStep(viewportSize.height());
}


void PreviewView::mousePressEvent(QMouseEvent *e)
{
    mousePressPos = e->pos();
    scrollBarValuesOnMousePress.rx() = horizontalScrollBar()->value();
    scrollBarValuesOnMousePress.ry() = verticalScrollBar()->value();
    e->accept();
}


void PreviewView::mouseMoveEvent(QMouseEvent *e)
{
    if (mousePressPos.isNull()) {
        e->ignore();
        return;
    }

    horizontalScrollBar()->setValue(scrollBarValuesOnMousePress.x() - e->pos().x() + mousePressPos.x());
    verticalScrollBar()->setValue(scrollBarValuesOnMousePress.y() - e->pos().y() + mousePressPos.y());
    horizontalScrollBar()->update();
    verticalScrollBar()->update();
    e->accept();
}


void PreviewView::mouseReleaseEvent(QMouseEvent *e)
{
    mousePressPos = QPoint();
    e->accept();
}


void PreviewView::scrollToPage(int page)
{
    const int pages=doc->pageCount();
    if(pages<=0)
        return;

    page=qBound(0, page, pages-1);
    verticalScrollBar()->setValue(pageOffset(page));
    horizontalScrollBar()->setValue(0);
}


int PreviewView::pageOffset(int page) const
{
    return qRound(page*(interPageSpacing+doc->pageSize().height()*scale));
}


int PreviewView::pageAtOffset(int value) const
{
    const qreal step=interPageSpacing+doc->pageSize().height()*scale;

    if(step<=0)
        return 0;

    // Страница i занимает [i*step, (i+1)*step): верх вьюпорта
    // на значении value показывает страницу с полом
    return qBound(0, int(value/step), qMax(0, doc->pageCount()-1));
}


PageThumbnailBar::PageThumbnailBar(QWidget *parent) : QListWidget(parent)
{
    // Вертикальная лента: иконка сверху, номер снизу, без правки.
    // Перенос включен: при узкой ленте миниатюры идут строго
    // друг под другом с вертикальной прокруткой
    setViewMode(QListView::IconMode);
    setMovement(QListView::Static);
    setResizeMode(QListView::Adjust);
    setWrapping(true);
    setSpacing(6);
    setIconSize(QSize(m_thumbWidth, m_thumbWidth));
    // Вертикальная полоса всегда видна: появление/исчезновение
    // меняло бы ширину вьюпорта и давало зацикленное "дрожание"
    // с перерисовкой миниатюр. Горизонтальной нет вообще
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMinimumWidth(96);

    connect(this, &QListWidget::currentItemChanged,
            this, &PageThumbnailBar::onCurrentItemChanged);
}


void PageThumbnailBar::rebuild(QTextDocument *document)
{
    m_doc=document;

    m_silent=true;
    clear();

    if(m_doc!=nullptr)
    {
        m_thumbWidth=thumbWidthForViewport();
        setIconSize(QSize(m_thumbWidth, m_thumbWidth));

        const int pages=m_doc->pageCount();

        for(int i=0; i<pages; ++i)
        {
            QListWidgetItem *item=new QListWidgetItem(
                QIcon(renderPage(m_doc, i, m_thumbWidth)),
                tr("Page %1").arg(i+1),
                this);
            item->setData(Qt::UserRole, i);
            item->setTextAlignment(Qt::AlignHCenter);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setSizeHint(QSize(m_thumbWidth+8, m_thumbWidth+28));
        }

        if(pages>0)
            setCurrentRow(0);
    }

    m_silent=false;
}


void PageThumbnailBar::setCurrentPageSilent(int page)
{
    if(page<0 || page>=count())
        return;

    m_silent=true;
    setCurrentRow(page);
    m_silent=false;
}


void PageThumbnailBar::onCurrentItemChanged(QListWidgetItem *current)
{
    if(m_silent || current==nullptr)
        return;

    emit pageSelected(current->data(Qt::UserRole).toInt());
}


void PageThumbnailBar::resizeEvent(QResizeEvent *e)
{
    QListWidget::resizeEvent(e);

    if(m_doc==nullptr)
        return;

    // Миниатюры вписываются в ширину ленты: перерисовка только
    // при реальном изменении ширины, иначе ресайз окна тормозил бы
    const int width=thumbWidthForViewport();

    if(width==m_thumbWidth)
        return;

    m_thumbWidth=width;
    setIconSize(QSize(m_thumbWidth, m_thumbWidth));
    renderAll();
}


int PageThumbnailBar::thumbWidthForViewport() const
{
    // Ширина вьюпорта минус поля и место под вертикальный скроллбар
    const int width=viewport()->width()-8-verticalScrollBar()->sizeHint().width();

    return qMax(48, width);
}


void PageThumbnailBar::renderAll()
{
    const int row=currentRow();

    m_silent=true;

    for(int i=0; i<count(); ++i)
    {
        QListWidgetItem *listItem=QListWidget::item(i);

        if(listItem==nullptr)
            continue;

        listItem->setIcon(QIcon(renderPage(m_doc,
                                           listItem->data(Qt::UserRole).toInt(),
                                           m_thumbWidth)));
        listItem->setSizeHint(QSize(m_thumbWidth+8, m_thumbWidth+28));
    }

    // Выделение и позиция прокрутки сохраняются
    setCurrentRow(row);

    m_silent=false;
}


QPixmap PageThumbnailBar::renderPage(QTextDocument *document, int page, int thumbWidth)
{
    const QSizeF pgSize=document->pageSize();
    const qreal k=thumbWidth/qMax(1.0, pgSize.width());
    const int thumbHeight=qMax(1, qRound(pgSize.height()*k));

    QImage image(thumbWidth, thumbHeight, QImage::Format_ARGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.scale(k, k);

    // Та же обрезка что в PreviewView::paintPage: только своя страница
    QRectF docRect(QPointF(0, page*pgSize.height()), pgSize);
    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.clip=docRect;
    ctx.palette.setColor(QPalette::Text, Qt::black);

    painter.translate(0, -page*pgSize.height());
    painter.setClipRect(docRect);
    document->documentLayout()->draw(&painter, ctx);

    painter.resetTransform();
    painter.setPen(QColor(160, 160, 160));
    painter.drawRect(0, 0, thumbWidth-1, thumbHeight-1);

    return QPixmap::fromImage(image);
}

