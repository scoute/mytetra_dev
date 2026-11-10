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
#include <QShowEvent>
#include <QTimer>
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
    // Пол не дает уйти в ноль и минус: иначе страница схлопывается
    // и paint уходит в деление на крошечный масштаб
    if(scale > 0.05)
        scale = qMax(0.05, scale - 0.2);
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


void PreviewView::zoomFit()
{
    // Вписать страницу в вьюпорт целиком: масштаб по меньшей стороне
    // с тем же отступом что в paintEvent. Диапазон как у ручного зума
    const QSizeF pgSize = doc->pageSize();

    if(pgSize.width() <= 0 || pgSize.height() <= 0)
        return;

    const QSize viewportSize = viewport()->size();

    const qreal fitScale = qMin((viewportSize.width() - 2 * interPageSpacing) / pgSize.width(),
                                (viewportSize.height() - 2 * interPageSpacing) / pgSize.height());

    if(fitScale > 0)
        scale = qBound(0.05, fitScale, 8.0);

    resizeEvent(0);
    viewport()->update();
}


void PreviewView::setFooterAlignment(Qt::Alignment alignment)
{
    if(m_footerAlign != alignment)
    {
        m_footerAlign = alignment;
        viewport()->update();
    }
}


void PreviewView::setFooterEnabled(bool enable)
{
    if(m_footerEnabled != enable)
    {
        m_footerEnabled = enable;
        viewport()->update();
    }
}


void PreviewView::drawFooterNumber(QPainter *painter, const QSizeF &pageSize, int page, Qt::Alignment alignment)
{
    // Номер страницы в подвале: серым, слева/по центру/справа.
    // Общая для превью и печати, чтобы "что вижу" совпадало
    // с "что на бумаге". Вызывается при транслированном на начало
    // страницы живописце (минус высота страниц выше), поэтому
    // смещение пишется полным
    Qt::Alignment hAlign = Qt::AlignHCenter;

    if(alignment & Qt::AlignLeft)
        hAlign = Qt::AlignLeft;
    else if(alignment & Qt::AlignRight)
        hAlign = Qt::AlignRight;

    painter->setPen(QColor(120, 120, 120));

    QFont footerFont = painter->font();
    footerFont.setPointSize(9);
    painter->setFont(footerFont);

    painter->drawText(QRectF(0, page * pageSize.height() + pageSize.height() - 24,
                             pageSize.width(), 20),
                      hAlign | Qt::AlignVCenter,
                      QString::number(page + 1));
}


void PreviewView::showEvent(QShowEvent *event)
{
    QAbstractScrollArea::showEvent(event);

    // Первый показ — вписать страницу целиком: на FullHD лист A4
    // в масштабе 1.0 не влезает по высоте и способа уместить нет.
    // Отложенно, чтобы вьюпорт уже получил финальный размер:
    // синхронно в showEvent геометрия еще не устаканилась
    if(firstShow)
    {
        firstShow = false;
        QTimer::singleShot(0, this, &PreviewView::zoomFit);
    }
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

    // Номер в подвале — поверх контента, только если включен.
    // Координаты в транслированной системе (минус высота страниц
    // выше), поэтому смещение пишется полным
    if(m_footerEnabled)
        drawFooterNumber(painter, pgSize, page, m_footerAlign);
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
    // Перенос включен: при широкой ленте миниатюры сами встают
    // в несколько колонок (максимум ограничивает максимальная ширина
    // ленты), при узкой идут строго друг под другом.
    // Вертикальная полоса всегда видна: появление/исчезновение
    // меняло бы ширину вьюпорта и давало зацикленное "дрожание".
    // Горизонтальной нет вообще
    setViewMode(QListView::IconMode);
    setMovement(QListView::Static);
    setResizeMode(QListView::Adjust);
    setWrapping(true);
    setSpacing(6);
    setIconSize(QSize(m_thumbWidth, m_thumbWidth));

    // Фиксированная ячейка сетки: перенос считается от нее.
    // Ширина - под миниатюру с полями, высота - миниатюра плюс подпись
    setGridSize(QSize(m_thumbWidth+12, m_thumbWidth+34));
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Ширина ленты: минимум одна колонка, максимум четыре.
    // Лишнее место при растягивании забирает основной вид:
    // больше четырех колонок не бывает
    const int scrollWidth=verticalScrollBar()->sizeHint().width();
    setMinimumWidth(gridSize().width()+scrollWidth+8);
    setMaximumWidth(4*gridSize().width()+scrollWidth+8);

    connect(this, &QListWidget::currentItemChanged,
            this, &PageThumbnailBar::onCurrentItemChanged);
}


void PageThumbnailBar::rebuild(QTextDocument *document)
{
    m_silent=true;
    clear();

    if(document!=nullptr)
    {
        // Размер миниатюр фиксированный (m_thumbWidth): ширина ленты
        // на него не влияет, влияет только на число колонок 1-4
        const int pages=document->pageCount();

        for(int i=0; i<pages; ++i)
        {
            QListWidgetItem *item=new QListWidgetItem(
                QIcon(renderPage(document, i, m_thumbWidth)),
                tr("Page %1").arg(i+1),
                this);
            item->setData(Qt::UserRole, i);
            item->setTextAlignment(Qt::AlignHCenter);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
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

