#include <QAbstractScrollArea>
#include <QPrintDialog>
#include <QPrinter>
#include <QToolBar>
#include <QAction>
#include <QTextFormat>
#include <QMouseEvent>
#include <QTextFrame>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPalette>
#include <QPainter>
#include <QDebug>
#include <QPageSetupDialog>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QScrollBar>
#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QFrame>
#include <QFileDialog>
#include <QFileInfo>
#include <QScopedPointer>
#include <QStyledItemDelegate>
#include "libraries/MtComboBox.h"

#include "PreviewView.h"
#include "PrintPreview.h"
#include "libraries/ShortcutManager.h"
#include "libraries/AppContext.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppConfigUi.h"
#include "libraries/wyedit/formatters/TypefaceFormatter.h"


// Стоковые Qt-диалоги печати/настройки страницы используют голый
// QComboBox: под темной темой в попапе пустые квадраты вместо пунктов
// (тот же грабель, что лечит MtComboBox). QStyledItemDelegate чинит
// отрисовку на месте, пересобирать диалоги не нужно
void PrintPreview::fixStockDialogCombos(QDialog *dialog)
{
    const QList<QComboBox*> combos=dialog->findChildren<QComboBox*>();

    foreach(QComboBox *combo, combos)
        combo->setItemDelegate(new QStyledItemDelegate(combo));
}


PrintPreview::PrintPreview(const QTextDocument *document, QWidget *parent)
    : QWidget(nullptr), printer(QPrinter::HighResolution)
{
    Q_UNUSED(parent);

    setWindowTitle(tr("MyTetra - Print Preview"));

    // Отдельное окно верхнего уровня: transient-диалог с родителем
    // оконный менеджер лишает кнопок свернуть/развернуть
    // (см. комментарий в PrintPreview.h). Модальность выставляется
    // вызывающим кодом через setWindowModality
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                   Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint |
                   Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);

    doc = document->clone();

    // Предпросмотр как на бумаге: маркеры разрывов чистятся,
    // сами разрывы остаются и рвут страницы в preview
    TypefaceFormatter::stripPageBreaksForPrint(doc);

    view = new PreviewView(doc);
    doc->setUseDesignMetrics(true);
    doc->documentLayout()->setPaintDevice(view->viewport());

    // Стандартные поля до раскладки: Page Setup правит их уже
    // на живом принтере, prepareDocument только читает
    applyDefaultMargins(printer);

    setupPrintDoc();
    setupUI();
    setupShortcuts();
    setupSignals();
    assembly();
}

PrintPreview::~PrintPreview()
{
    delete doc; // todo: Проверить на утечку памяти, добавить объекты для удаления если необходимо
}

void PrintPreview::setupPrintDoc()
{
    prepareDocument(printer, doc);
}


// Стандартные поля MyTetra: слева 20 мм (подшивка), остальные по 5 мм.
// Миллиметры как в диалоге Page Setup, порядок QMarginsF —
// слева, сверху, справа, снизу
void PrintPreview::applyDefaultMargins(QPrinter &printer)
{
    printer.setPageMargins(QMarginsF(20.0, 5.0, 5.0, 5.0), QPageLayout::Millimeter);
}


// Вернуть fullPage и наши поля после диалога печати (см. шапку).
// Порядок важен: fullPage первым, поля поверх — иначе pageRect
// посчитается от сброшенных значений
void PrintPreview::reassertFullPage(QPrinter &printer)
{
    printer.setFullPage(true);
    applyDefaultMargins(printer);
}


// Общий конвейер подготовки документа для превью, прямой печати
// и экспорта: один размер страницы, один шрифт, без номеров страниц
void PrintPreview::prepareDocument(QPrinter &printer, QTextDocument *doc)
{
    printer.setFullPage(true);

    // Бумага целиком в миллиметрах, перевод в пункты: печать идет
    // в пунктах, device-px дают микроскопию (доказано пробами, см. 586)
    const QSizeF paperMm=printer.pageLayout().fullRect(QPageLayout::Millimeter).size();
    const qreal mmToPt=72.0/25.4;

    doc->setPageSize(QSizeF(paperMm.width()*mmToPt, paperMm.height()*mmToPt));

    // Поля пользователя рамкой текста: отдельно по сторонам,
    // с обрезкой чтобы не вылезти за полстраницы
    qreal marginLeft, marginTop, marginRight, marginBottom;
    printer.getPageMargins(&marginLeft, &marginTop, &marginRight, &marginBottom,
                           QPrinter::Millimeter);

    const qreal maxH=paperMm.width()/2.0;
    const qreal maxV=paperMm.height()/2.0;

    marginLeft=qBound(0.0, marginLeft, maxH);
    marginRight=qBound(0.0, marginRight, maxH);
    marginTop=qBound(0.0, marginTop, maxV);
    marginBottom=qBound(0.0, marginBottom, maxV);

    QTextFrameFormat fmt=doc->rootFrame()->frameFormat();
    fmt.setMargin(0);
    fmt.setLeftMargin(marginLeft*mmToPt);
    fmt.setTopMargin(marginTop*mmToPt);
    fmt.setRightMargin(marginRight*mmToPt);
    fmt.setBottomMargin(marginBottom*mmToPt);
    doc->rootFrame()->setFrameFormat(fmt);

    QFont f(doc->defaultFont());
    f.setPointSize(10);
    doc->setDefaultFont(f);
}

// Печать вручную постранично: QTextDocument::print() без явного
// pageSize пагинирует сам (плюс номера в подвале), а с явным —
// масштабирует непредсказуемо. Здесь страницы рисуются тем же кодом
// что превью: раскладка из prepareDocument, номера — drawFooterNumber
void PrintPreview::printDocument(QPrinter &printer, QTextDocument *doc)
{
    const bool numbersOn=AppContext::instance().appConfig()->uiSection()->get_printFooterEnabled();

    int mode=AppContext::instance().appConfig()->uiSection()->get_printFooterAlign();
    Qt::Alignment footerAlign=Qt::AlignHCenter;

    if(mode==0)
        footerAlign=Qt::AlignLeft;
    else if(mode==2)
        footerAlign=Qt::AlignRight;

    QPainter painter(&printer);

    if(!painter.isActive())
        return;

    // Документ разложен в пунктах, принтер рисует в device-пикселях
    const qreal k=printer.resolution()/72.0;

    // Сдвиг origin на бумажный угол. Нативный драйвер Windows стартует
    // живописца с угла printable-области даже при fullPage (замер VM:
    // сдвиг текста ровно на поля, слева 40мм вместо 20). Компенсация
    // к углу листа; где origin честный (Qt PDF-движок, Linux)
    // pageRect нулевой и это no-op
    const QPoint pageOrigin=printer.pageRect().topLeft();

    const QSizeF pgSize=doc->pageSize();
    const int pages=doc->pageCount();

    int from=printer.fromPage()-1;
    int to=printer.toPage()-1;

    if(printer.fromPage()<=0)
        from=0;

    if(printer.toPage()<=0 || to>=pages)
        to=pages-1;

    for(int i=qMax(0, from); i<=to && i<pages; ++i)
    {
        if(i>qMax(0, from))
            printer.newPage();

        painter.save();
        painter.scale(k, k);

        // Компенсация сюда, в масштабированную систему (единицы — пункты):
        // до scale() translate считал бы в device-пикселях и сдвига почти
        // не давал. Внутри save/restore — действует на каждую страницу
        if(!pageOrigin.isNull())
            painter.translate(-pageOrigin.x()/k, -pageOrigin.y()/k);

        QRectF docRect(QPointF(0, i*pgSize.height()), pgSize);
        QAbstractTextDocumentLayout::PaintContext ctx;
        ctx.clip=docRect;
        ctx.palette.setColor(QPalette::Text, Qt::black);

        painter.translate(0, -i*pgSize.height());
        painter.setClipRect(docRect);
        doc->documentLayout()->draw(&painter, ctx);

        if(numbersOn)
            PreviewView::drawFooterNumber(&painter, pgSize, i, footerAlign);

        painter.restore();
    }
}

void PrintPreview::setupUI()
{
    // Кнопки на панели инструментов
    buttonPrint=new QToolButton(this);
    buttonPrint->setText(tr("Print..."));
    buttonPrint->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    buttonPageSetup=new QToolButton(this);
    buttonPageSetup->setText(tr("Page Setup..."));
    buttonPageSetup->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    // Выгрузка в PDF прямо из превью: для ОС без PDF-принтера
    // (Win7) и чтобы не ходить в меню Файл. Настройки бумаги
    // берутся из текущего принтера превью (см. exportToPdf)
    buttonExportPdf=new QToolButton(this);
    buttonExportPdf->setText(tr("Export PDF..."));
    buttonExportPdf->setToolTip(tr("Export to PDF file with current page settings"));
    buttonExportPdf->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    buttonZoomIn=new QToolButton(this);
    buttonZoomIn->setText(tr("Zoom In"));
    buttonZoomIn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    buttonZoomOut=new QToolButton(this);
    buttonZoomOut->setText(tr("Zoom Out"));
    buttonZoomOut->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    // Обычный размер между увеличением и уменьшением:
    // возврат к масштабу 1.0 после игры зумом
    buttonZoomReset=new QToolButton(this);
    buttonZoomReset->setText(tr("100%"));
    buttonZoomReset->setToolTip(tr("Normal size"));
    buttonZoomReset->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    // Вписать страницу в окно: на высоком листе масштаб 1.0
    // не влезает по высоте и другого способа уместить нет
    buttonZoomFit=new QToolButton(this);
    buttonZoomFit->setText(tr("Fit"));
    buttonZoomFit->setToolTip(tr("Fit page to window size"));
    buttonZoomFit->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    // Положение номеров страниц в подвале: слева, по центру, справа.
    // Действует и на превью, и на печать (один и тот же drawFooterNumber)
    alignCombo=new MtComboBox(this);
    alignCombo->addItem(tr("Left"), 0);
    alignCombo->addItem(tr("Center"), 1);
    alignCombo->addItem(tr("Right"), 2);
    alignCombo->setToolTip(tr("Page number position"));

    const int savedAlign=AppContext::instance().appConfig()->uiSection()->get_printFooterAlign();
    const int alignIndex=savedAlign>=0 ? alignCombo->findData(savedAlign) : -1;
    alignCombo->setCurrentIndex(alignIndex>=0 ? alignIndex : 2);

    // Печатать ли номера вообще (и в превью, и на бумаге)
    footersCheckBox=new QCheckBox(this);
    footersCheckBox->setText(tr("Page numbers"));
    footersCheckBox->setToolTip(tr("Show page numbers in footer, in preview and in print"));
    footersCheckBox->setChecked(AppContext::instance().appConfig()->uiSection()->get_printFooterEnabled());

    buttonClose=new QToolButton(this);
    buttonClose->setText(tr("Close"));
    buttonClose->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    // Лента миниатюр страниц слева от основного вида.
    // Создается здесь, а не в assembly: сигналы подключаются раньше
    thumbBar=new PageThumbnailBar(this);

    // Стартовые состояние подвала — сохраненные: положение номеров
    // и сам факт их показа
    onAlignChanged(alignCombo->currentIndex());
    onFootersToggled(footersCheckBox->isChecked());
}

void PrintPreview::setupShortcuts()
{
    qDebug() << "Setup shortcut for" << staticMetaObject.className();

    buttonPrint->setShortcut( AppContext::instance().shortcutManager()->getKeySequence("misc-print") ); // Устанавливается шорткат
    buttonPrint->setToolTip( AppContext::instance().shortcutManager()->getKeySequenceAsText("misc-print") ); // ToolTip зависит от шортката
}

void PrintPreview::setupSignals()
{
    connect(buttonPrint, &QToolButton::clicked, this, &PrintPreview::print);
    connect(buttonPageSetup, &QToolButton::clicked, this, &PrintPreview::pageSetup);
    connect(buttonExportPdf, &QToolButton::clicked, this, &PrintPreview::exportToPdf);
    connect(buttonZoomIn, &QToolButton::clicked, view, &PreviewView::zoomIn);
    connect(buttonZoomReset, &QToolButton::clicked, view, &PreviewView::zoomReset);
    connect(buttonZoomFit, &QToolButton::clicked, view, &PreviewView::zoomFit);
    connect(alignCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PrintPreview::onAlignChanged);
    connect(footersCheckBox, &QCheckBox::toggled,
            this, &PrintPreview::onFootersToggled);
    connect(buttonZoomOut, &QToolButton::clicked, view, &PreviewView::zoomOut);
    connect(buttonClose, &QToolButton::clicked, this, &PrintPreview::close);

    // Лента миниатюр <-> основной вид: клик прыгает на страницу,
    // прокрутка подсвечивает текущую миниатюру
    connect(thumbBar, &PageThumbnailBar::pageSelected, view, &PreviewView::scrollToPage);
    connect(view->verticalScrollBar(), &QScrollBar::valueChanged, this, &PrintPreview::onMainScrollChanged);

    // Обновление горячих клавиш, если они были изменены
    connect(AppContext::instance().shortcutManager(), &ShortcutManager::updateWidgetShortcut, this, &PrintPreview::setupShortcuts);
}

void PrintPreview::onMainScrollChanged(int value)
{
    thumbBar->setCurrentPageSilent(view->pageAtOffset(value));
}

void PrintPreview::assembly()
{
    // Панель инструментов
    QHBoxLayout *toolsbox=new QHBoxLayout();
    toolsbox->addWidget(buttonPrint);
    toolsbox->addWidget(buttonPageSetup);
    toolsbox->addWidget(buttonExportPdf);
    toolsbox->addWidget(buttonZoomIn);
    toolsbox->addWidget(buttonZoomReset);
    toolsbox->addWidget(buttonZoomFit);
    toolsbox->addWidget(buttonZoomOut);
    toolsbox->addWidget(footersCheckBox);
    toolsbox->addWidget(alignCombo);

    // Отбивка закрывающей кнопки: визуальное отделение
    // безопасного выхода от рабочих инструментов
    QFrame *closeSeparator=new QFrame(this);
    closeSeparator->setFrameShape(QFrame::VLine);
    closeSeparator->setFrameShadow(QFrame::Sunken);
    toolsbox->addWidget(closeSeparator);

    toolsbox->addWidget(buttonClose);
    toolsbox->addStretch();

    // Лента миниатюр слева, основной вид справа. Сплиттер вместо
    // жесткого HBox: ширину ленты меняет пользователь, миниатюры
    // сами вписываются в новую ширину (resizeEvent ленты)
    QSplitter *contentSplitter=new QSplitter(this);
    contentSplitter->addWidget(thumbBar);
    contentSplitter->addWidget(view);
    contentSplitter->setStretchFactor(0, 0);
    contentSplitter->setStretchFactor(1, 1);
    contentSplitter->setSizes(QList<int>() << 180 << 780);

    // Сборка содержимого окна
    centralLayout=new QVBoxLayout();
    centralLayout->addLayout(toolsbox);
    centralLayout->addWidget(contentSplitter, 1);
    centralLayout->setSpacing(1);
    centralLayout->setContentsMargins(1,1,1,1);

    setLayout(centralLayout);
    resize(960, 600);

    // Миниатюры после раскладки документа
    thumbBar->rebuild(doc);
}

void PrintPreview::print()
{
    QPrintDialog *dlg = new QPrintDialog(&printer, this);
    fixStockDialogCombos(dlg);
    if (dlg->exec() == QDialog::Accepted) {
        // Диалог мог перестроить layout из DEVMODE: возвращаем
        // fullPage и наши поля до раскладки и печати
        reassertFullPage(printer);
        printDocument(printer, doc);
    }
    delete dlg;
}

void PrintPreview::pageSetup()
{
    QPageSetupDialog dlg(&printer, this);
    fixStockDialogCombos(&dlg);
    if (dlg.exec() == QDialog::Accepted) {
        setupPrintDoc(); // setup();
        view->updateLayout();
        thumbBar->rebuild(doc);
    }
}


// Настройка принтера PDF-экспорта из текущего принтера превью:
// формат, файл, раскладка целиком (бумага, поля, fullPage).
// Именно копирование layout, а не дефолты: пользователь уже
// настроил страницу в Page Setup и видит ее в превью.
// reassertFullPage здесь не нужен: диалога печати не было,
// настройки никто не сбрасывал
void PrintPreview::setupPdfExportPrinter(QPrinter &pdfPrinter,
                                        const QPrinter &srcPrinter,
                                        const QString &fileName)
{
    pdfPrinter.setOutputFormat(QPrinter::PdfFormat);
    pdfPrinter.setOutputFileName(fileName);
    pdfPrinter.setPageLayout(srcPrinter.pageLayout());
    pdfPrinter.setFullPage(srcPrinter.fullPage());
}


// Выгрузка в PDF прямо из превью. Печать по копии документа
// (как прямая печать): маркеры разрывов чистятся, живой документ
// не трогается. Принтер превью не портится: пишется во временный
void PrintPreview::exportToPdf()
{
    QString fileName=QFileDialog::getSaveFileName(this,
                                                  tr("Export to PDF"),
                                                  QString(),
                                                  tr("PDF files (*.pdf)"));

    if(fileName.isEmpty())
        return;

    if(QFileInfo(fileName).suffix().isEmpty())
        fileName.append(QStringLiteral(".pdf"));

    QPrinter pdfPrinter(QPrinter::HighResolution);
    setupPdfExportPrinter(pdfPrinter, printer, fileName);

    QScopedPointer<QTextDocument> printDoc(doc->clone());
    TypefaceFormatter::stripPageBreaksForPrint(printDoc.data());

    PrintPreview::prepareDocument(pdfPrinter, printDoc.data());
    PrintPreview::printDocument(pdfPrinter, printDoc.data());
}


void PrintPreview::onAlignChanged(int index)
{
    const int mode=alignCombo->itemData(index).toInt();

    Qt::Alignment footerAlign=Qt::AlignHCenter;

    if(mode==0)
        footerAlign=Qt::AlignLeft;
    else if(mode==2)
        footerAlign=Qt::AlignRight;

    view->setFooterAlignment(footerAlign);
    AppContext::instance().appConfig()->uiSection()->set_printFooterAlign(mode);
}


void PrintPreview::onFootersToggled(bool checked)
{
    view->setFooterEnabled(checked);
    AppContext::instance().appConfig()->uiSection()->set_printFooterEnabled(checked);
}

// #include "PrintPreview.moc"
