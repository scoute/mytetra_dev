// Поля принтера в предпросмотре печати на QTest (см. 582).
// Page Setup задает поля на QPrinter, setupPrintDoc обязан
// пересчитывать размер страницы из pageRect, а не лить глухой
// отступ на полный лист. Чистый QPrinter без системного принтера:
// бумага и поля выставляются явно, результат не зависит от машины.
#include <QtTest>
#include <QApplication>
#include <QTextDocument>
#include <QTextFrame>
#include <QPrinter>
#include <QPageLayout>
#include <QPageSize>
#include <QColor>
#include <QPainter>
#include <QMarginsF>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QFileInfo>\n#include <QFontMetrics>

#include "views/printPreview/PrintPreview.h"
#include "views/printPreview/PreviewView.h"
#include "libraries/AppContext.h"
#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "libraries/TrashMonitoring.h"
#include "libraries/WalkHistory.h"
#include "libraries/VisitHistory.h"
#include "libraries/ActionLogger.h"
#include "libraries/ShortcutManager.h"
#include "libraries/PeriodicCheckBase.h"
#include "libraries/PeriodicSyncro.h"
#include "libraries/InternalClipboard.h"
#include "models/appConfig/AppFiles.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "models/appConfig/AppConfig.h"

extern ShortcutManager shortcutManager;

extern AppConfig mytetraConfig;

class TestPrintMargins : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void marginsAffectPageSize(void);

    void unifiedHelperSetsPageSize(void);

    void footerShowsPageNumber(void);

    void zoomFitFitsPage(void);

    void footerPositionFollowsSetting(void);

    void printDocumentSmokeTest(void);

    void footerFontStaysSmallOnPrinter(void);

private:

    QTemporaryDir m_tmp;
    QSettings *m_settings=nullptr;
};

void TestPrintMargins::initTestCase(void)
{
    static FixedParameters *fp=new FixedParameters;
    static GlobalParameters *gp=new GlobalParameters;
    static AppFiles *af=new AppFiles;
    static DataBaseConfig *dbc=new DataBaseConfig;
    static TrashMonitoring *tm=new TrashMonitoring;
    static WalkHistory *wh=new WalkHistory;
    static VisitHistory *vh=new VisitHistory;
    static ActionLogger *al=new ActionLogger;
    static PeriodicCheckBase *pcb=new PeriodicCheckBase;
    static PeriodicSyncro *ps=new PeriodicSyncro;
    static InternalClipboard *ic=new InternalClipboard;
    AppContext::instance().init(fp, gp, &mytetraConfig, af, dbc, tm,
                                wh, vh, al, &shortcutManager, pcb, ps,
                                ic, nullptr, &gp->cryptHolder());

    // PrintPreview тянет хоткеи через AppContext: конфиг нужен,
    // иначе setupShortcuts падает на нулевом хранилище
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");
    m_settings=new QSettings(m_tmp.filePath(QStringLiteral("test_printmargins.ini")),
                             QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;
}

void TestPrintMargins::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestPrintMargins::marginsAffectPageSize(void)
{
    QTextDocument src;
    src.setHtml(QStringLiteral("<p>Test print margins</p>"));

    PrintPreview preview(&src, nullptr);
    preview.show();
    QApplication::processEvents();

    // Фиксированная бумага и известные поля: результат не зависит
    // от системного принтера по умолчанию
    preview.printer.setPageSize(QPageSize(QPageSize::A4));
    preview.printer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);
    preview.setupPrintDoc();

    // Страница — полный лист в пунктах (пропорции бумаги),
    // перевод мм->pt единый для экрана и печати
    const QSizeF paperMm=preview.printer.pageLayout().fullRect(QPageLayout::Millimeter).size();
    const qreal mmToPt=72.0/25.4;
    const QSizeF pageSize=preview.doc->pageSize();

    QVERIFY2(qFuzzyCompare(pageSize.width(), paperMm.width()*mmToPt),
             "page width must follow full paper");
    QVERIFY2(qFuzzyCompare(pageSize.height(), paperMm.height()*mmToPt),
             "page height must follow full paper");

    // Поля пользователя — рамкой текста, по 20мм с каждой стороны
    const QTextFrameFormat fmt=preview.doc->rootFrame()->frameFormat();
    QCOMPARE(fmt.margin(), 0.0);
    QVERIFY2(qFuzzyCompare(fmt.leftMargin(), 20.0*mmToPt),
             "left margin must follow page setup");
    QVERIFY2(qFuzzyCompare(fmt.topMargin(), 20.0*mmToPt),
             "top margin must follow page setup");

    // Смена полей не трогает размер страницы, только рамку
    preview.printer.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);
    preview.setupPrintDoc();

    QCOMPARE(preview.doc->pageSize(), pageSize);
    QVERIFY2(qFuzzyCompare(preview.doc->rootFrame()->frameFormat().leftMargin(), 10.0*mmToPt),
             "smaller margins must shrink frame");

    // Миниатюры пересобираются под новую раскладку без падений
    preview.thumbBar->rebuild(preview.doc);
}

void TestPrintMargins::unifiedHelperSetsPageSize(void)
{
    // Статик для прямой печати и экспорта: та же раскладка что превью.
    // Явный pageSize заодно гасит автонумерацию страниц Qt
    QTextDocument doc;
    doc.setHtml(QStringLiteral("<p>Unified pipeline</p>"));

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    PrintPreview::prepareDocument(printer, &doc);

    QVERIFY2(printer.fullPage(), "helper prints from paper origin");

    // Лист целиком 1:1, поля — рамкой 15мм
    const QSizeF paperMm=printer.pageLayout().fullRect(QPageLayout::Millimeter).size();
    const qreal mmToPt=72.0/25.4;
    QVERIFY2(qFuzzyCompare(doc.pageSize().width(), paperMm.width()*mmToPt),
             "unified page width must follow printer paper");
    QVERIFY2(qFuzzyCompare(doc.pageSize().height(), paperMm.height()*mmToPt),
             "unified page height must follow printer paper");

    const QTextFrameFormat fmt=doc.rootFrame()->frameFormat();
    QCOMPARE(fmt.margin(), 0.0);
    QVERIFY2(qFuzzyCompare(fmt.leftMargin(), 15.0*mmToPt),
             "unified left margin must follow page setup");
}

void TestPrintMargins::footerShowsPageNumber(void)
{
    // Номер в подвале рисуется поверх контента (серым по белому),
    // в документ и печать не попадает. Короткий текст: низ чист,
    // кроме самого номера
    QTextDocument doc;
    doc.setHtml(QStringLiteral("<p>Hi</p>"));
    doc.setPageSize(QSizeF(400, 500));

    PreviewView view(&doc);
    view.resize(500, 600);
    view.show();
    QApplication::processEvents();
    view.scrollToPage(0);
    QApplication::processEvents();

    const QImage shot=view.viewport()->grab().toImage().convertToFormat(QImage::Format_ARGB32);

    // Низ первой страницы: страница начинается с отступа 30,
    // подвал — последние ~24px страницы
    bool found=false;

    for(int y=490; y<560 && !found; ++y)
        for(int x=170; x<290 && !found; ++x)
        {
            const QColor c=shot.pixelColor(x, y);

            if(c.alpha() > 200 &&
               qAbs(c.red()-120) < 50 &&
               qAbs(c.green()-120) < 50 &&
               qAbs(c.blue()-120) < 50)
                found=true;
        }

    QVERIFY2(found, "footer must contain gray page number");
}

void TestPrintMargins::zoomFitFitsPage(void)
{
    // Высокая страница в низком окне: первый показ сам вписывает
    // (автофит), зум туда-сюда возвращает в то же состояние
    QTextDocument doc;
    doc.setHtml(QStringLiteral("<p>Hi</p>"));
    doc.setPageSize(QSizeF(400, 1100));

    PreviewView view(&doc);
    view.resize(500, 200);
    view.show();
    QApplication::processEvents();
    QApplication::processEvents();

    QVERIFY2(view.verticalScrollBar()->maximum()==0,
             "first show must fit page without scrolling");

    view.zoomIn();
    QApplication::processEvents();
    QVERIFY2(view.verticalScrollBar()->maximum() > 0,
             "zoom in must overflow again");

    view.zoomFit();
    QApplication::processEvents();
    QVERIFY2(view.verticalScrollBar()->maximum()==0,
             "manual fit must restore fitting");
}

void TestPrintMargins::footerPositionFollowsSetting(void)
{
    // Положение номера в подвале: слева/центр/справа.
    // Рисуется общей drawFooterNumber (она же на печати),
    // поэтому тест гоняет ее напрямую на картинке известного размера:
    // детерминировано, без вьюпорта и масштаба
    const QSizeF pgSize(400, 200);

    auto footerCenterX = [&](Qt::Alignment align) {
        QImage img(400, 200, QImage::Format_ARGB32);
        img.fill(Qt::white);

        QPainter painter(&img);
        PreviewView::drawFooterNumber(&painter, pgSize, 0, align);
        painter.end();

        long sumX = 0;
        long count = 0;

        // Полоса подвала: низ страницы минус 24..4
        for(int y = 176; y < 196; ++y)
            for(int x = 0; x < 400; ++x)
            {
                const QRgb px = img.pixel(x, y);
                const int g = qGray(px);

                if(g > 90 && g < 160 && qAlpha(px) > 200)
                {
                    sumX += x;
                    ++count;
                }
            }

        if(count == 0)
            return -1.0;

        return double(sumX) / double(count);
    };

    const double leftX = footerCenterX(Qt::AlignLeft);
    QVERIFY2(leftX >= 0, "left footer number must be rendered");
    QVERIFY2(leftX < 400.0 / 3.0,
             "left number must sit in left third");
    // Отступ от края бумаги минимум 1 см (=28.35 pt): цифра не должна
    // липнуть к обрезу. Толерантность 5px на глифные подшипники шрифта
    QVERIFY2(leftX >= 28.35 - 5.0,
             "left number must keep 1cm inset from paper edge");

    const double centerX = footerCenterX(Qt::AlignHCenter);
    QVERIFY2(centerX >= 0, "center footer number must be rendered");
    QVERIFY2(qAbs(centerX - 200.0) < 400.0 / 3.0,
             "center number must sit in middle third");

    const double rightX = footerCenterX(Qt::AlignRight);
    QVERIFY2(rightX >= 0, "right footer number must be rendered");
    QVERIFY2(rightX > 2.0 * 400.0 / 3.0,
             "right number must sit in right third");
    QVERIFY2(rightX <= 400.0 - 28.35 + 5.0,
             "right number must keep 1cm inset from paper edge");
}

void TestPrintMargins::printDocumentSmokeTest(void)
{
    // Сквозной прогон печати в PDF: раскладка из prepareDocument,
    // постраничная отрисовка с номерами. Проверяется что файл
    // создается непустым и без падений (содержимое — в превью-тестах)
    QTextDocument doc;

    QString html;
    for(int i = 0; i < 30; ++i)
        html += QStringLiteral("<p>Line %1 for print smoke test</p>").arg(i);
    doc.setHtml(html);

    const QString outPath = m_tmp.filePath(QStringLiteral("smoke.pdf"));

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(outPath);

    PrintPreview::prepareDocument(printer, &doc);
    QVERIFY2(doc.pageCount() > 1, "smoke doc must span several pages");

    PrintPreview::printDocument(printer, &doc);

    QVERIFY2(QFileInfo(outPath).isFile(), "smoke pdf must be created");
    QVERIFY2(QFileInfo(outPath).size() > 0, "smoke pdf must not be empty");
}

void TestPrintMargins::footerFontStaysSmallOnPrinter(void)
{
    // Регрессия: номер в PDF уходил кеглем 150 вместо ~12 —
    // пункты на предмасштабированном живописце печати удваивались
    // (device-DPI плюс масштаб k). Тот же хелпер что зовет печать,
    // на реальном устройстве QPrinter: высота шрифта обязана остаться
    // в пределах подвала (<=24px), а не в сотнях пикселей высокого DPI
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(m_tmp.filePath(QStringLiteral("metrics.pdf")));

    const QFont f = PreviewView::footerFont(QFont());
    const int h = QFontMetrics(f, &printer).height();

    QVERIFY2(h > 0, "footer font metrics must resolve on printer");
    QVERIFY2(h <= 24, "footer font must stay small on high-dpi printer device");
}

QTEST_MAIN(TestPrintMargins)

#include "test_printmargins.moc"
