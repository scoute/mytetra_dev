// Лента миниатюр предпросмотра печати на QTest (см. 556).
// Чистые виджеты без базы: документ с несколькими страницами,
// сверка числа миниатюр, математики прокрутки и сигнала выбора.
#include <QtTest>
#include <QApplication>
#include <QTextDocument>
#include <QSignalSpy>
#include <QScrollBar>

#include "views/printPreview/PreviewView.h"

class TestPrintThumb : public QObject
{
    Q_OBJECT

private slots:

    void thumbsAndNav(void);
};

void TestPrintThumb::thumbsAndNav(void)
{
    // Документ на несколько страниц: маленькая высота страницы
    QTextDocument doc;
    doc.setPageSize(QSizeF(400, 120));

    QString html;
    for(int i=0; i<10; ++i)
        html+=QStringLiteral("<p>Line %1</p>").arg(i);
    doc.setHtml(html);

    const int pages=doc.pageCount();
    QVERIFY2(pages>=3, "document must span several pages");

    PreviewView view(&doc);
    view.resize(600, 200);
    view.show();
    QApplication::processEvents();

    // Первый показ сам вписывает страницу (автофит, см. 585):
    // дальше тест пляшет от гарантированного автофита
    QVERIFY2(!qFuzzyCompare(view.scale, 1.0), "first show must auto-fit");
    view.zoomReset();
    QApplication::processEvents();

    // Сброс — настоящий физический 100% (DPI экрана / 72),
    // а не единица: лист должен совпадать с бумагой по миллиметрам
    qreal dpi = view.viewport()->physicalDpiX();

    if(dpi <= 0)
        dpi = view.viewport()->logicalDpiX();

    QVERIFY2(qFuzzyCompare(view.scale, dpi/72.0), "reset must restore true 100%");

    // Математика страниц туда-обратно
    QCOMPARE(view.pageAtOffset(view.pageOffset(2)), 2);
    QCOMPARE(view.pageAtOffset(view.pageOffset(0)), 0);
    QVERIFY2(view.verticalScrollBar()->maximum()>0,
             "tall document must be scrollable");

    // Прыжок на страницу двигает скроллбар ровно на ее начало
    view.scrollToPage(1);
    QCOMPARE(view.verticalScrollBar()->value(), view.pageOffset(1));
    QCOMPARE(view.pageAtOffset(view.verticalScrollBar()->value()), 1);

    // Зум туда-обратно: сброс возвращает масштаб 1.0
    const int baseOffset=view.pageOffset(1);
    view.zoomIn();
    QVERIFY2(view.pageOffset(1)!=baseOffset, "zoom in must change geometry");
    view.zoomReset();
    QCOMPARE(view.pageOffset(1), baseOffset);
    view.scrollToPage(0);
    QCOMPARE(view.verticalScrollBar()->value(), 0);

    // Лента: по миниатюре на страницу, иконки не пустые
    PageThumbnailBar bar;
    bar.rebuild(&doc);
    QCOMPARE(bar.count(), pages);
    QVERIFY2(!bar.item(0)->icon().isNull(), "thumbnail icon must be rendered");

    // Выбор строки шлет номер страницы
    QSignalSpy spy(&bar, &PageThumbnailBar::pageSelected);
    bar.setCurrentRow(1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 1);

    // Тихая подсветка не спамит сигналами
    bar.setCurrentPageSilent(2);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(bar.currentRow(), 2);

    // Фиксированный размер миниатюр: ресайз ленты их не перерисовывает,
    // широкая лента раскладывает их в несколько колонок, выбор цел
    bar.show();
    QApplication::processEvents();
    bar.resize(500, 400);
    QApplication::processEvents();
    QCOMPARE(bar.count(), pages);
    QCOMPARE(bar.currentRow(), 2);
    QCOMPARE(bar.iconSize(), QSize(120, 120));
    QVERIFY2(bar.visualItemRect(bar.item(0)).top()==bar.visualItemRect(bar.item(1)).top(),
             "wide bar must lay thumbnails out in several columns");

    // Полоса прокрутки всегда видна: иначе появление/исчезновение
    // давало зацикленное "дрожание" с перерисовкой
    QCOMPARE(bar.verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOn);
}

QTEST_MAIN(TestPrintThumb)

#include "test_printthumb.moc"
