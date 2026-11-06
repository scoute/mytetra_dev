// TypefaceFormatter на QTest (см. 495, линия v2).
// Порт typefaceprobe: маркеры кода, сборка кода под маркером,
// снятие подчеркивания copy-ссылок, сбор оглавления.
// Без Editor/EditorConfig: isCodeBlock идет по фону карточки
// (константа), остальное - чистые функции документа.
#include <QtTest>
#include <QApplication>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QTextCharFormat>

#include "libraries/wyedit/EditorTextArea.h"
#include "libraries/wyedit/formatters/TypefaceFormatter.h"

class TestTypeface : public QObject
{
    Q_OBJECT

private slots:

    void markers(void);
    void codeBlocks(void);
    void copyLinks(void);
    void toc(void);
    void headings(void);
    void htmlRoundtrip(void);
};

void TestTypeface::markers(void)
{
    EditorTextArea area;
    TypefaceFormatter fmt;
    fmt.setTextArea(&area);

    // Маркеры: генерация и разбор
    QVERIFY2(TypefaceFormatter::codeMarkerLine(TypefaceFormatter::CodePython)
             .contains("{lang = Python}"), "python marker must render");
    QCOMPARE(fmt.codeMarkerLanguage("# {lang = Python} [copy]"),
             TypefaceFormatter::CodePython);
    QCOMPARE(fmt.codeMarkerLanguage("<!-- {lang = HTML} -->"),
             TypefaceFormatter::CodeHtml);
    QCOMPARE(fmt.codeMarkerLanguage("just text"), -1);
    QCOMPARE(fmt.codeMarkerLanguage("# {lang = nosuchlang}"), -1);
}

void TestTypeface::codeBlocks(void)
{
    EditorTextArea area;
    TypefaceFormatter fmt;
    fmt.setTextArea(&area);
    QTextDocument *doc=area.document();

    // Код под маркером: фон карточки делает блоки кодовыми без editorConfig
    QTextCursor c(doc);
    QTextBlockFormat card;
    card.setBackground(TypefaceFormatter::codeCardColor());
    c.insertText("# {lang = Python}");
    c.insertBlock();
    c.insertText("x = 1");
    c.insertBlock();
    c.insertText("print(x)");
    c.select(QTextCursor::Document);
    c.mergeBlockFormat(card);

    QVERIFY2(fmt.isCodeBlock(doc->findBlockByNumber(0)), "first block must be code");
    QVERIFY2(fmt.isCodeBlock(doc->findBlockByNumber(2)), "last block must be code");
    QCOMPARE(fmt.collectCodeBelowMarker(0), QStringLiteral("x = 1\nprint(x)"));

    // Висячие пустые строки в конце в буфер не идут
    c.movePosition(QTextCursor::End);
    c.insertBlock();
    c.insertBlock();
    c.select(QTextCursor::Document);
    c.mergeBlockFormat(card);
    QCOMPARE(fmt.collectCodeBelowMarker(0), QStringLiteral("x = 1\nprint(x)"));
}

void TestTypeface::copyLinks(void)
{
    EditorTextArea area;
    TypefaceFormatter fmt;
    fmt.setTextArea(&area);
    QTextDocument *doc=area.document();

    // Снятие подчеркивания copy-ссылок, чужие ссылки не трогаем
    QTextCursor c(doc);
    QTextCharFormat copyLink;
    copyLink.setAnchor(true);
    copyLink.setAnchorHref(TypefaceFormatter::copyCodeHref());
    copyLink.setFontUnderline(true);
    c.insertText("# {lang = Python} [copy]", copyLink);
    c.insertBlock();
    QTextCharFormat webLink;
    webLink.setAnchor(true);
    webLink.setAnchorHref(QStringLiteral("https://example.com"));
    webLink.setFontUnderline(true);
    c.insertText("example", webLink);

    fmt.normalizeCopyLinkUnderline();

    bool copyUnderlined=false;
    bool webUnderlined=false;
    for(QTextBlock b=doc->begin(); b.isValid(); b=b.next())
        for(QTextBlock::iterator it=b.begin(); !it.atEnd(); ++it)
        {
            const QTextCharFormat f=it.fragment().charFormat();
            if(f.anchorHref()==TypefaceFormatter::copyCodeHref() && f.fontUnderline())
                copyUnderlined=true;
            if(f.anchorHref()==QStringLiteral("https://example.com") && f.fontUnderline())
                webUnderlined=true;
        }
    QVERIFY2(!copyUnderlined, "copy link must lose underline");
    QVERIFY2(webUnderlined, "web link must keep underline");
}

void TestTypeface::toc(void)
{
    EditorTextArea area;
    TypefaceFormatter fmt;
    fmt.setTextArea(&area);
    QTextDocument *doc=area.document();

    // Оглавление: уровни из формата блоков
    QTextCursor c(doc);
    QTextBlockFormat h1;
    h1.setHeadingLevel(1);
    c.insertText("Chapter");
    c.mergeBlockFormat(h1);
    c.insertBlock();
    QTextBlockFormat h2;
    h2.setHeadingLevel(2);
    c.insertText("Section");
    c.mergeBlockFormat(h2);
    c.insertBlock();
    QTextBlockFormat h4;
    h4.setHeadingLevel(4);
    c.insertText("Deep");
    c.mergeBlockFormat(h4);
    c.insertBlock();
    c.setBlockFormat(QTextBlockFormat());
    c.insertText("plain");

    QList<TypefaceFormatter::TocEntry> entries=fmt.collectTocEntries();
    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries.at(0).level, 1);
    QCOMPARE(entries.at(0).text, QStringLiteral("Chapter"));
    QCOMPARE(entries.at(1).level, 2);
    QCOMPARE(entries.at(1).text, QStringLiteral("Section"));
    QCOMPARE(entries.at(2).level, 4);
    QCOMPARE(entries.at(2).text, QStringLiteral("Deep"));

    QVERIFY2(!TypefaceFormatter::tocGoPrefix().isEmpty(), "go prefix must exist");
    QVERIFY2(!TypefaceFormatter::tocAnchorHref().isEmpty(), "anchor href must exist");
}

void TestTypeface::headings(void)
{
    EditorTextArea area;
    TypefaceFormatter fmt;
    fmt.setTextArea(&area);
    QTextDocument *doc=area.document();

    // Выпадашка заголовков: индексы 3/4/5 -> H4/H5/H6 с эталонными
    // отступами, индекс 6 -> обычный текст
    QTextCursor c(doc);
    c.insertText(QStringLiteral("Deep"));
    QTextCursor sel(doc);
    sel.select(QTextCursor::Document);
    area.setTextCursor(sel);

    fmt.onHeadingClicked(3);
    QTextBlockFormat f4=doc->firstBlock().blockFormat();
    QCOMPARE(f4.headingLevel(), 4);
    QVERIFY2(f4.topMargin()==12 && f4.bottomMargin()==12, "H4 margins must be 12/12");
    const double size4=doc->firstBlock().begin().fragment().charFormat().fontPointSize();

    fmt.onHeadingClicked(4);
    QTextBlockFormat f5=doc->firstBlock().blockFormat();
    QCOMPARE(f5.headingLevel(), 5);
    QVERIFY2(f5.topMargin()==12 && f5.bottomMargin()==4, "H5 margins must be 12/4");
    const double size5=doc->firstBlock().begin().fragment().charFormat().fontPointSize();

    fmt.onHeadingClicked(5);
    QTextBlockFormat f6=doc->firstBlock().blockFormat();
    QCOMPARE(f6.headingLevel(), 6);
    QVERIFY2(f6.topMargin()==0 && f6.bottomMargin()==0, "H6 margins must be 0/0");
    const double size6=doc->firstBlock().begin().fragment().charFormat().fontPointSize();

    // Кегль строго убывает с уровнем: лесенка 48/32/24/20/16/12
    QVERIFY2(size4>size5 && size5>size6, "sizes must decrease H4>H5>H6");

    fmt.onHeadingClicked(6);
    QCOMPARE(doc->firstBlock().blockFormat().headingLevel(), 0);

    // Переключение вкл/выкл/вкл не растит кегль
    {
        QTextCursor sel2(doc);
        sel2.select(QTextCursor::Document);
        area.setTextCursor(sel2);

        fmt.onHeadingClicked(0);
        const double firstOn=doc->firstBlock().begin().fragment().charFormat().fontPointSize();
        fmt.onHeadingClicked(0);
        fmt.onHeadingClicked(0);
        const double secondOn=doc->firstBlock().begin().fragment().charFormat().fontPointSize();
        QVERIFY2(firstOn>0.0 && firstOn==secondOn, "retoggle must be stable");
    }

    // Наследие старого билда: ступень + явный размер
    {
        QTextCursor c2(doc);
        c2.select(QTextCursor::Document);
        c2.insertText(QStringLiteral("Legacy"));
        c2.setBlockFormat(QTextBlockFormat());
        QTextCursor sel2(doc);
        sel2.select(QTextCursor::Document);
        area.setTextCursor(sel2);

        QTextCharFormat legacy;
        legacy.setFontPointSize(11);
        legacy.setProperty(QTextFormat::FontSizeAdjustment, -1);
        QTextCursor merge(doc);
        merge.select(QTextCursor::Document);
        merge.mergeCharFormat(legacy);

        fmt.onHeadingClicked(0);
        QTextCharFormat applied=doc->firstBlock().begin().fragment().charFormat();
        QVERIFY2(!applied.hasProperty(QTextFormat::FontSizeAdjustment),
                 "adjustment must be stripped on apply");
        const double legacyOn=applied.fontPointSize();

        fmt.onHeadingClicked(0);
        QTextCharFormat removed=doc->firstBlock().begin().fragment().charFormat();
        QVERIFY2(!removed.hasProperty(QTextFormat::FontSizeAdjustment),
                 "adjustment must stay stripped on remove");

        fmt.onHeadingClicked(0);
        QTextCharFormat reapplied=doc->firstBlock().begin().fragment().charFormat();
        QVERIFY2(reapplied.fontPointSize()==legacyOn &&
                 !reapplied.hasProperty(QTextFormat::FontSizeAdjustment),
                 "reapply must be stable");
    }

    // Загруженный документ: снятие нормализует кегль, повтор не умножает
    {
        QTextCursor c2(doc);
        c2.select(QTextCursor::Document);
        c2.insertText(QStringLiteral("Loaded"));
        c2.setBlockFormat(QTextBlockFormat());
        QTextCursor sel2(doc);
        sel2.select(QTextCursor::Document);
        area.setTextCursor(sel2);

        fmt.onHeadingClicked(0);
        const double freshOn=doc->firstBlock().begin().fragment().charFormat().fontPointSize();
        doc->firstBlock().setUserData(nullptr); // как после save/load
        fmt.onHeadingClicked(0);
        const double afterOff=doc->firstBlock().begin().fragment().charFormat().fontPointSize();
        fmt.onHeadingClicked(0);
        const double reloadOn=doc->firstBlock().begin().fragment().charFormat().fontPointSize();
        QVERIFY2(afterOff<=freshOn && reloadOn==freshOn, "reload must be stable");
    }
}

void TestTypeface::htmlRoundtrip(void)
{
    EditorTextArea area;
    TypefaceFormatter fmt;
    fmt.setTextArea(&area);
    QTextDocument *doc=area.document();

    // Кругооборот через HTML (сценарий рестарта): сохраненный <h1>
    // с явным font-size обязан отрисоваться тем же кеглем после setHtml
    const QString stored=QStringLiteral(
        "<html><head><meta name=\"qrichtext\" content=\"1\" />"
        "<style type=\"text/css\">p, li { white-space: pre-wrap; }</style></head>"
        "<body style=\" font-family:'DejaVu Sans'; font-size:11pt; font-weight:400; font-style:normal;\">"
        "<h1><span style=\" font-size:44pt; font-weight:600;\">H1</span></h1>"
        "<h2><span style=\" font-weight:600;\">H2</span></h2>"
        "<h6><span style=\" font-size:11pt; font-weight:600;\">H6</span></h6>"
        "</body></html>");

    doc->setDefaultFont(QFont(QStringLiteral("DejaVu Sans"), 11));
    doc->setHtml(stored);
    fmt.normalizeHeadingFontSizes(); // как при загрузке заметки

    const QTextCharFormat f1=doc->firstBlock().begin().fragment().charFormat();
    const QTextCharFormat f2=doc->firstBlock().next().begin().fragment().charFormat();
    const QTextCharFormat f6=doc->lastBlock().begin().fragment().charFormat();
    QCOMPARE(f1.fontPointSize(), 44.0);
    QVERIFY2(!f1.hasProperty(QTextFormat::FontSizeAdjustment), "H1 must lose adjustment");
    // Заголовок без явного кегля: лесенка 11*(8/3)=29.333, не мелкий
    QVERIFY2(f2.fontPointSize()>29.0 && f2.fontPointSize()<29.7,
             "H2 must follow the ladder");
    QVERIFY2(!f2.hasProperty(QTextFormat::FontSizeAdjustment), "H2 must lose adjustment");
    QCOMPARE(f6.fontPointSize(), 11.0);
    QVERIFY2(!f6.hasProperty(QTextFormat::FontSizeAdjustment), "H6 must lose adjustment");
    QVERIFY2(f1.fontPointSize()>f2.fontPointSize() &&
             f2.fontPointSize()>f6.fontPointSize(),
             "sizes must decrease H1>H2>H6");
}

QTEST_MAIN(TestTypeface)

#include "test_typeface.moc"
