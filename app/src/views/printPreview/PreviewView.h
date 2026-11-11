#ifndef _PREVIEWVIEW_H_
#define _PREVIEWVIEW_H_

#include <QAbstractScrollArea>
#include <QFont>
#include <QListWidget>


class QTextDocument;
class QListWidgetItem;
class QShowEvent;

class PreviewView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    PreviewView(QTextDocument *document);

    inline void updateLayout() { resizeEvent(nullptr); viewport()->update(); }

    // Прокрутка на начало страницы и геометрия страниц.
    // Та же раскладка что в paintEvent: страница i начинается
    // на interPageSpacing + i*(interPageSpacing + H*scale),
    // значению скроллбара соответствует контент без начального отступа
    void scrollToPage(int page);

    // Номер страницы в подвале: общая отрисовка для превью и печати
    // (PrintPreview::printDocument зовет напрямую)
    static void drawFooterNumber(QPainter *painter, const QSizeF &pageSize, int page, Qt::Alignment alignment);

    // Шрифт номера: пиксельный размер, общий для превью и печати
    // (пункты на предмасштабированном живописце печати удваивались)
    static QFont footerFont(const QFont &base);
    int pageOffset(int page) const;
    int pageAtOffset(int y) const;

public slots:
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void zoomFit();

    // Подвал с номером страницы: вкл/выкл и положение
    // (слева, по центру, справа). Действует и на превью,
    // и на печать — номер рисуется одним кодом
    void setFooterEnabled(bool enable);
    void setFooterAlignment(Qt::Alignment alignment);

protected:
    virtual void paintEvent(QPaintEvent *e);
    virtual void resizeEvent(QResizeEvent *);
    virtual void showEvent(QShowEvent *e);
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);

private:
    void paintPage(QPainter *painter, int page);

    QTextDocument *doc;
    qreal scale;
    int interPageSpacing;
    QPoint mousePressPos;
    QPoint scrollBarValuesOnMousePress;

    bool m_footerEnabled=true;
    Qt::Alignment m_footerAlign=Qt::AlignHCenter;
    bool firstShow=true;
};


// Лента миниатюр страниц слева от предпросмотра, как в PDF-смотрелках.
// Клик/стрелки выбирают страницу и прыгают в основном виде.
// Миниатюры рисуются тем же движком что основной вид, поэтому
// совпадают попиксельно (масштаб + обрезка по странице)
class PageThumbnailBar : public QListWidget
{
    Q_OBJECT

public:
    explicit PageThumbnailBar(QWidget *parent=nullptr);

    // Перестроить миниатюры по документу
    void rebuild(QTextDocument *doc);

    // Подсветить страницу без сигналов (для следования за прокруткой)
    void setCurrentPageSilent(int page);

signals:
    void pageSelected(int page);

private slots:
    void onCurrentItemChanged(QListWidgetItem *current);

private:
    static QPixmap renderPage(QTextDocument *doc, int page, int thumbWidth);

    int m_thumbWidth=120;
    bool m_silent=false;
};

#endif // _PREVIEWVIEW_H_
