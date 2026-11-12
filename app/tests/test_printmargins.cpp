// Поля принтера в предпросмотре печати на QTest (см. 582).
// Page Setup задает поля на QPrinter, setupPrintDoc обязан
// пересчитывать размер страницы из pageRect, а не лить глухой
// отступ на полный лист. Чистый QPrinter без системного принтера:
// бумага и поля выставляются явно, результат не зависит от машины.
#include <QtTest>
#include <QApplication>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextTable>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <functional>
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
#include "models/appConfig/AppConfigUi.h"

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
    void defaultMargins(void);
    void pdfExportPrinterSetup(void);
    void zoomResetTrueSize(void);
    void pageBorderStaysVisible(void);

    void fixedTableWidthClamped(void);

    void tablesAlignLeftAtRoot(void);

    void frameDrawnOverWideContent(void);

    void savedPageSetupApplies(void);

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

    // Компенсация origin в printDocument: здесь pageRect нулевой
    // (Qt PDF-движок, origin честный), сдвига быть не должно.
    // На нативном драйвере Windows pageRect ненулевой (замер VM)
    // и тот же код компенсирует сдвиг к бумажному углу
    QVERIFY2(printer.pageRect().topLeft().isNull(),
             "pageRect must start at paper corner on sane backend");

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

void TestPrintMargins::defaultMargins(void)
{
    // Стандартные поля MyTetra: слева 20 мм (подшивка),
    // остальные по 5 мм. Ставятся хелпером при создании принтера,
    // конвейер prepareDocument их только читает
    QPrinter printer(QPrinter::HighResolution);
    PrintPreview::applyDefaultMargins(printer);

    qreal left = 0;
    qreal top = 0;
    qreal right = 0;
    qreal bottom = 0;
    printer.getPageMargins(&left, &top, &right, &bottom,
                           QPrinter::Millimeter);

    QCOMPARE(left, 20.0);
    QCOMPARE(top, 5.0);
    QCOMPARE(right, 5.0);
    QCOMPARE(bottom, 5.0);
}

void TestPrintMargins::pdfExportPrinterSetup(void)
{
    // Кнопка экспорта из превью: принтер PDF наследует раскладку
    // текущего принтера (бумага, поля, fullPage), а не дефолты.
    // Хелпер без диалога, проверяется напрямую
    QPrinter srcPrinter(QPrinter::HighResolution);
    srcPrinter.setPageMargins(QMarginsF(10.0, 11.0, 12.0, 13.0), QPageLayout::Millimeter);
    srcPrinter.setFullPage(true);

    QPrinter pdfPrinter(QPrinter::HighResolution);
    PrintPreview::setupPdfExportPrinter(pdfPrinter, srcPrinter,
                                       QStringLiteral("/tmp/opencode/export.pdf"));

    QCOMPARE(pdfPrinter.outputFormat(), QPrinter::PdfFormat);
    QCOMPARE(pdfPrinter.outputFileName(), QStringLiteral("/tmp/opencode/export.pdf"));
    QCOMPARE(pdfPrinter.fullPage(), srcPrinter.fullPage());
    QCOMPARE(pdfPrinter.pageLayout().pageSize().id(), srcPrinter.pageLayout().pageSize().id());
    QCOMPARE(pdfPrinter.pageLayout().orientation(), srcPrinter.pageLayout().orientation());

    qreal sl = 0, st = 0, sr = 0, sb = 0;
    qreal dl = 0, dt = 0, dr = 0, db = 0;
    srcPrinter.getPageMargins(&sl, &st, &sr, &sb, QPrinter::Millimeter);
    pdfPrinter.getPageMargins(&dl, &dt, &dr, &db, QPrinter::Millimeter);
    QCOMPARE(dl, sl);
    QCOMPARE(dt, st);
    QCOMPARE(dr, sr);
    QCOMPARE(db, sb);
}

void TestPrintMargins::zoomResetTrueSize(void)
{
    // Кнопка 100%: физический размер листа, а не масштаб 1.0.
    // Масштаб равен DPI экрана / 72 (физический, запасной логический)
    QTextDocument doc;
    doc.setPageSize(QSizeF(400, 500));

    PreviewView view(&doc);
    view.show();
    QApplication::processEvents();
    QApplication::processEvents();

    view.zoomIn();
    view.zoomReset();

    qreal dpi = view.viewport()->physicalDpiX();

    if(dpi <= 0)
        dpi = view.viewport()->logicalDpiX();

    QVERIFY2(qAbs(view.scale - dpi/72.0) < 1e-9,
             "100% must be real-size scale, not 1.0");
}

void TestPrintMargins::pageBorderStaysVisible(void)
{
    // Рамка страницы — косметическое перо: видна целиком на любом
    // масштабе. Раньше перо 1px в единицах раскладки на дробном
    // масштабе давало субпиксельную линию, пропадавшую местами
    // в зависимости от смещения при прокрутке (см. 593)
    QTextDocument doc;
    doc.setHtml(QStringLiteral("<p>Hi</p>"));
    doc.setPageSize(QSizeF(400, 500));

    PreviewView view(&doc);
    view.resize(803, 317);
    view.show();
    QApplication::processEvents();
    QApplication::processEvents();
    view.zoomFit();

    // Мелкий масштаб: линия 1px в единицах раскладки растворяется
    // (старый код), косметическое перо держит 1 device-пиксель всегда
    while(view.scale > 0.25)
        view.zoomOut();

    QApplication::processEvents();

    QImage shot = view.viewport()->grab().toImage().convertToFormat(QImage::Format_ARGB32);

    // Строка заведомо внутри страницы по вертикали
    const int yMid = 30 + qRound(500.0 * view.scale / 2.0);

    // Левый край страницы: первый светлый пиксель в этой строке
    int left = -1;

    for(int x = 0; x < shot.width(); ++x)
        if(qGray(shot.pixel(x, yMid)) > 200)
        {
            left = x;
            break;
        }

    QVERIFY2(left >= 0, "page must be visible");

    // Правая кромка ±2px: темные пиксели рамки по всей высоте
    const int right = left + qRound(400.0 * view.scale);
    const int top = 30;
    const int bottom = qMin(shot.height() - 1, top + qRound(500.0 * view.scale));

    int darkRows = 0;
    int rows = 0;

    for(int y = top; y < bottom; y += 5)
    {
        ++rows;

        for(int x = qMax(0, right - 2); x <= qMin(shot.width() - 1, right + 2); ++x)
            if(qGray(shot.pixel(x, y)) < 128)
            {
                ++darkRows;
                break;
            }
    }

    QVERIFY2(rows > 0, "page must span rows");
    QVERIFY2(darkRows * 10 >= rows * 8,
             "page right border must be visible along height");
}

// Рекурсивный поиск первой таблицы в кадре (для fixedTableWidthClamped)
static QTextTable *findFirstTable(QTextFrame *frame)
{
    for(QTextFrame::iterator it=frame->begin(); !it.atEnd(); ++it)
    {
        QTextTable *table=qobject_cast<QTextTable*>(it.currentFrame());

        if(table!=nullptr)
            return table;

        if(it.currentFrame()!=nullptr)
        {
            QTextTable *nested=findFirstTable(it.currentFrame());

            if(nested!=nullptr)
                return nested;
        }
    }

    return nullptr;
}

void TestPrintMargins::fixedTableWidthClamped(void)
{
    // Вставленная из веба таблица с width="600": фиксированная ширина
    // шире текстового поля. Без нормализации Qt якорил бы ее слева
    // и растил вправо за границу (см. 596, проба tableprobe).
    // prepareDocument жмет такие таблицы до 100%
    QTextDocument wide;
    wide.setHtml(QStringLiteral("<p>Before</p>"
                                "<table width=\"600\" align=\"center\" border=\"1\">"
                                "<tr><td>AAA</td><td>BBB</td></tr>"
                                "</table><p>After</p>"));

    QPrinter printer(QPrinter::HighResolution);
    PrintPreview::applyDefaultMargins(printer);
    PrintPreview::prepareDocument(printer, &wide);

    QTextTable *wideTable=findFirstTable(wide.rootFrame());
    QVERIFY2(wideTable!=nullptr, "table must be found");

    QVERIFY2(wideTable->format().width().type()==QTextLength::PercentageLength,
             "overwide fixed table must become percentage");
    QCOMPARE(wideTable->format().width().rawValue(), 100.0);

    // Узкая фиксированная (200) в поле помещается — не трогается
    QTextDocument narrow;
    narrow.setHtml(QStringLiteral("<table width=\"200\" border=\"1\">"
                                  "<tr><td>AAA</td></tr>"
                                  "</table>"));

    PrintPreview::prepareDocument(printer, &narrow);

    QTextTable *narrowTable=findFirstTable(narrow.rootFrame());
    QVERIFY2(narrowTable!=nullptr, "narrow table must be found");

    QVERIFY2(narrowTable->format().width().type()==QTextLength::FixedLength,
             "fitting fixed table must stay fixed");
    QCOMPARE(narrowTable->format().width().rawValue(), 200.0);

    // Вложенная широкая таблица тоже жмется
    QTextDocument nested;
    nested.setHtml(QStringLiteral("<table width=\"100%\" border=\"1\">"
                                  "<tr><td><table width=\"600\" border=\"1\">"
                                  "<tr><td>Inner</td></tr>"
                                  "</table></td></tr>"
                                  "</table>"));

    PrintPreview::prepareDocument(printer, &nested);

    QTextTable *outerTable=findFirstTable(nested.rootFrame());
    QVERIFY2(outerTable!=nullptr, "outer table must be found");
    QVERIFY2(outerTable->format().width().type()==QTextLength::PercentageLength,
             "outer percentage table must stay percentage");

    QTextTable *innerTable=findFirstTable(outerTable);
    QVERIFY2(innerTable!=nullptr, "inner table must be found");
    QVERIFY2(innerTable->format().width().type()==QTextLength::PercentageLength,
             "overwide nested fixed table must become percentage");
}

void TestPrintMargins::tablesAlignLeftAtRoot(void)
{
    // Центрированная 100% и правая 50%: без нормализации Qt ставит
    // их как L + align*(pageW-R-W) — левое поле забыто в базе
    // выравнивания, таблицы уезжают вправо за границу (см. 596,
    // проба tableprobe: +33pt и +61pt). prepareDocument ставит
    // корневым процентным таблицам AlignLeft: позиция align=0
    // не течет, 100%-я выглядит так же, узкие влезают в поле.
    // Вложенная центрированная не трогается (в ячейке утечки нет)
    QTextDocument doc;
    doc.setHtml(QStringLiteral("<p>Before</p>"
                               "<table width=\"100%\" align=\"center\" border=\"1\">"
                               "<tr><td>A</td><td><table width=\"50%\" align=\"center\" border=\"1\">"
                               "<tr><td>Inner</td></tr>"
                               "</table></td></tr>"
                               "</table>"
                               "<table width=\"50%\" align=\"right\" border=\"1\">"
                               "<tr><td>B</td><td>C</td></tr>"
                               "</table><p>After</p>"));

    QPrinter printer(QPrinter::HighResolution);
    PrintPreview::applyDefaultMargins(printer);
    PrintPreview::prepareDocument(printer, &doc);

    // Текстовое поле в пунктах для сверки геометрии
    qreal ml, mt, mr, mb;
    printer.getPageMargins(&ml, &mt, &mr, &mb, QPrinter::Millimeter);
    const qreal mmToPt=72.0/25.4;
    const qreal textLeft=ml*mmToPt;
    const qreal textRight=doc.pageSize().width()-mr*mmToPt;

    QList<QTextTable*> rootTables;
    QTextTable *innerTable=nullptr;
    std::function<void(QTextFrame*, bool)> walk=[&](QTextFrame *f, bool isRoot)
    {
        for(QTextFrame::iterator it=f->begin(); !it.atEnd(); ++it)
        {
            QTextTable *t=qobject_cast<QTextTable*>(it.currentFrame());

            if(t!=nullptr)
            {
                if(isRoot)
                    rootTables.append(t);
                else if(innerTable==nullptr)
                    innerTable=t;

                walk(t, false);
                continue;
            }

            if(it.currentFrame()!=nullptr)
                walk(it.currentFrame(), false);
        }
    };
    walk(doc.rootFrame(), true);

    QCOMPARE(rootTables.size(), 2);
    QVERIFY2(innerTable!=nullptr, "nested table must be found");

    // Корневые — влево и в пределах поля с запасом 6pt
    // (остаточная константа Qt ~1-4pt). Вложенная — как была
    foreach(QTextTable *t, rootTables)
    {
        QVERIFY2(t->format().alignment() & Qt::AlignLeft,
                 "root percentage table must be left aligned for print");

        const QRectF fr=doc.documentLayout()->frameBoundingRect(t);
        QVERIFY2(fr.left() >= textLeft-1.0 && fr.left() <= textLeft+6.0,
                 "root table must start at text left");
        QVERIFY2(fr.right() <= textRight+6.0,
                 "root table must not run past text right");
    }

    QVERIFY2(!(innerTable->format().alignment() & Qt::AlignLeft),
             "nested table alignment must be preserved");
}

void TestPrintMargins::savedPageSetupApplies(void)
{
    // Галочка "Запоминать текущие поля": сохраненные поля применяются
    // к новым принтерам, reassertFullPage после диалога печати
    // их не затирает хардкодом. Без сохраненного — дефолты 20/5/5/5
    AppConfigUi *ui=AppContext::instance().appConfig()->uiSection();
    ui->set_printMargins(QMarginsF(10.0, 11.0, 12.0, 13.0));
    ui->set_printPageSizeId(QPageSize::Letter);
    ui->set_printPageOrientation(QPageLayout::Landscape);

    QPrinter printer(QPrinter::HighResolution);
    PrintPreview::applySavedPageSetup(printer);

    // Сохраненные поля применились
    qreal left, top, right, bottom;
    printer.getPageMargins(&left, &top, &right, &bottom, QPrinter::Millimeter);
    QCOMPARE(QMarginsF(left, top, right, bottom), QMarginsF(10.0, 11.0, 12.0, 13.0));

    // Сохраненные бумага и ориентация применились
    QVERIFY2(printer.pageLayout().pageSize().id()==QPageSize::Letter,
             "saved paper size must apply");
    QVERIFY2(printer.pageLayout().orientation()==QPageLayout::Landscape,
             "saved orientation must apply");

    // Диалог печати сбрасывает fullPage: reassert возвращает fullPage,
    // а поля берет из конфига, а не из хардкода 20/5/5/5.
    // Бумагу и ориентацию не трогает (их мог сменить диалог)
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setFullPage(false);
    PrintPreview::reassertFullPage(printer);
    QVERIFY2(printer.fullPage(), "fullPage must be restored");

    printer.getPageMargins(&left, &top, &right, &bottom, QPrinter::Millimeter);
    QCOMPARE(QMarginsF(left, top, right, bottom), QMarginsF(10.0, 11.0, 12.0, 13.0));
    QVERIFY2(printer.pageLayout().pageSize().id()==QPageSize::A4,
             "reassert must not touch paper size");

    // Чистка за собой: другие тесты ждут пустой конфиг
    m_settings->remove(QStringLiteral("printMarginLeft"));
    m_settings->remove(QStringLiteral("printMarginTop"));
    m_settings->remove(QStringLiteral("printMarginRight"));
    m_settings->remove(QStringLiteral("printMarginBottom"));
    m_settings->remove(QStringLiteral("printPageSizeId"));
    m_settings->remove(QStringLiteral("printPageOrientation"));

    // Без сохраненного — старое поведение: дефолтные поля
    QPrinter fresh(QPrinter::HighResolution);
    PrintPreview::applySavedPageSetup(fresh);

    fresh.getPageMargins(&left, &top, &right, &bottom, QPrinter::Millimeter);
    QCOMPARE(QMarginsF(left, top, right, bottom), QMarginsF(20.0, 5.0, 5.0, 5.0));
}

void TestPrintMargins::frameDrawnOverWideContent(void)
{
    // Широкий непрозрачный контент (вставленная из веба таблица шире
    // страницы с непрозрачным фоном) своим фоном затирал кромку:
    // рамка рисовалась первой, контент поверх. Разрыв стабильный,
    // от скролла и зума не зависит, только на страницах с таким
    // контентом (см. 597). Теперь фон первым, кромка поверх всего:
    // глубоко черные пиксели кромки обязаны быть по всей видимой
    // высоте. Порог глубокого черного: затертая кромка сереет
    // и детектором <128 не ловится, целая косметическая линия дает
    // полностью черные пиксели
    QTextDocument doc;
    QString html = QStringLiteral("<table width=\"600\" align=\"center\" border=\"1\" bgcolor=\"#ffffff\">");

    for(int row = 0; row < 20; ++row)
        html += QStringLiteral("<tr><td>AAA</td><td>BBB</td></tr>");

    html += QStringLiteral("</table>");
    doc.setHtml(html);
    doc.setPageSize(QSizeF(400, 500));

    PreviewView view(&doc);
    view.resize(803, 317);
    view.show();
    QApplication::processEvents();
    QApplication::processEvents();

    // Живой масштаб как на экране (а не авто-zoomFit): стирание кромки
    // наполовину пикселя видно только при реальном округлении,
    // при мелком fit-масштабе кромка сохраняется случайно
    view.zoomReset();
    QApplication::processEvents();

    QImage shot = view.viewport()->grab().toImage().convertToFormat(QImage::Format_ARGB32);

    int left = -1;

    for(int y = 10; y < 60 && left < 0; ++y)
        for(int x = 0; x < shot.width(); ++x)
            if(qGray(shot.pixel(x, y)) > 200)
            {
                left = x;
                break;
            }

    QVERIFY2(left >= 0, "page must be visible");

    const int right = left + qRound(400.0 * view.scale);
    int darkRows = 0;

    for(int y = 0; y < shot.height(); ++y)
    {
        for(int x = qMax(0, right - 2); x <= qMin(shot.width() - 1, right + 2); ++x)
            if(qGray(shot.pixel(x, y)) < 64)
            {
                ++darkRows;
                break;
            }
    }

    QVERIFY2(darkRows > 200,
             "page border must survive opaque wide content");
}

QTEST_MAIN(TestPrintMargins)

#include "test_printmargins.moc"
