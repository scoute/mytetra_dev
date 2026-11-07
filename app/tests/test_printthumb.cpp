// Лента миниатюр предпросмотра печати на QTest, линия v2 (см. 556).
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

    // Математика страниц туда-обратно
    QCOMPARE(view.pageAtOffset(view.pageOffset(2)), 2);
    QCOMPARE(view.pageAtOffset(view.pageOffset(0)), 0);
    QVERIFY2(view.verticalScrollBar()->maximum()>0,
             "tall document must be scrollable");

    // Прыжок на страницу двигает скроллбар ровно на ее начало
    view.scrollToPage(1);
    QCOMPARE(view.verticalScrollBar()->value(), view.pageOffset(1));
    QCOMPARE(view.pageAtOffset(view.verticalScrollBar()->value()), 1);

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
}

QTEST_MAIN(TestPrintThumb)

#include "test_printthumb.moc"
