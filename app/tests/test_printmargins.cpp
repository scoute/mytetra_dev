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
#include <QMarginsF>
#include <QSettings>
#include <QTemporaryDir>
#include <QTemporaryDir>

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

    const QSize pageRectPx=preview.printer.pageLayout().paintRectPixels(preview.printer.resolution()).size();
    const qreal kx=qreal(preview.view->logicalDpiX())/preview.printer.logicalDpiX();
    const qreal ky=qreal(preview.view->logicalDpiY())/preview.printer.logicalDpiY();
    const QSizeF pageSize=preview.doc->pageSize();

    QVERIFY2(qFuzzyCompare(pageSize.width(), pageRectPx.width()*kx),
             "page width must follow printer page rect");
    QVERIFY2(qFuzzyCompare(pageSize.height(), pageRectPx.height()*ky),
             "page height must follow printer page rect");

    // Глухого отступа рамки больше нет: поля задает сам размер страницы
    QCOMPARE(preview.doc->rootFrame()->frameFormat().margin(), 0.0);

    // Смена полей пересчитывает раскладку в ту же сторону
    preview.printer.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);
    preview.setupPrintDoc();

    QVERIFY2(preview.doc->pageSize().width() > pageSize.width(),
             "smaller margins must widen page");
    QVERIFY2(preview.doc->pageSize().height() > pageSize.height(),
             "smaller margins must heighten page");

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
    printer.setFullPage(true);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    PrintPreview::prepareDocument(printer, &doc,
                                   printer.logicalDpiX(), printer.logicalDpiY());

    QVERIFY2(!printer.fullPage(), "helper must respect margins");

    const QSizeF printable=printer.pageLayout().paintRectPixels(printer.resolution()).size();
    QVERIFY2(qFuzzyCompare(doc.pageSize().width(), printable.width()),
             "unified page width must follow printer");
    QVERIFY2(qFuzzyCompare(doc.pageSize().height(), printable.height()),
             "unified page height must follow printer");
    QCOMPARE(doc.rootFrame()->frameFormat().margin(), 0.0);
}

QTEST_MAIN(TestPrintMargins)

#include "test_printmargins.moc"
