#ifndef _PREVIEWVIEW_H_
#define _PREVIEWVIEW_H_

#include <QAbstractScrollArea>
#include <QListWidget>


class QTextDocument;
class QListWidgetItem;

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
    int pageOffset(int page) const;
    int pageAtOffset(int y) const;

public slots:
    void zoomIn();
    void zoomOut();

protected:
    virtual void paintEvent(QPaintEvent *e);
    virtual void resizeEvent(QResizeEvent *);
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
