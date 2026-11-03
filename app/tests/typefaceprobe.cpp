// TypefaceFormatter стенд: маркеры кода, сборка кода под маркером,
// снятие подчеркивания copy-ссылок, сбор оглавления. Без Editor/EditorConfig:
// isCodeBlock идет по фону карточки (константа), остальное - чистые функции
// документа. Падение здесь = регрессия P3.2 или юнитов 400-404.
#include <QApplication>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QDebug>

#include "libraries/wyedit/EditorTextArea.h"
#include "libraries/wyedit/formatters/TypefaceFormatter.h"

static int fails=0;
static void check(bool ok, const char *name)
{
    qDebug() << (ok ? "PASS" : "FAIL") << name;
    if(!ok)
        ++fails;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    EditorTextArea area;
    TypefaceFormatter fmt;
    fmt.setTextArea(&area);

    // Маркеры: генерация и разбор
    check(TypefaceFormatter::codeMarkerLine(TypefaceFormatter::CodePython).contains("{lang = Python}"),
          "MARKER-LINE-PY");
    check(fmt.codeMarkerLanguage("# {lang = Python} [copy]")==TypefaceFormatter::CodePython,
          "MARKER-PARSE-PY");
    check(fmt.codeMarkerLanguage("<!-- {lang = HTML} -->")==TypefaceFormatter::CodeHtml,
          "MARKER-PARSE-HTML");
    check(fmt.codeMarkerLanguage("just text")==-1, "MARKER-PLAIN");
    check(fmt.codeMarkerLanguage("# {lang = nosuchlang}")==-1, "MARKER-UNKNOWN");

    // Код под маркером: фон карточки делает блоки кодовыми без editorConfig
    QTextDocument *doc=area.document();
    doc->clear();
    {
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
    }
    check(fmt.isCodeBlock(doc->findBlockByNumber(0)), "CODEBLOCK-0");
    check(fmt.isCodeBlock(doc->findBlockByNumber(2)), "CODEBLOCK-2");
    check(fmt.collectCodeBelowMarker(0)==QStringLiteral("x = 1\nprint(x)"),
          "COLLECT-EXACT");

    // Висячие пустые строки в конце в буфер не идут
    {
        QTextCursor c(doc);
        c.movePosition(QTextCursor::End);
        c.insertBlock();
        c.insertBlock();
        QTextBlockFormat card;
        card.setBackground(TypefaceFormatter::codeCardColor());
        c.select(QTextCursor::Document);
        c.mergeBlockFormat(card);
    }
    check(fmt.collectCodeBelowMarker(0)==QStringLiteral("x = 1\nprint(x)"),
          "COLLECT-TRIM-TAIL");

    // Снятие подчеркивания copy-ссылок, чужие ссылки не трогаем
    doc->clear();
    {
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
    }
    fmt.normalizeCopyLinkUnderline();
    {
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
        check(!copyUnderlined, "NOLINK-COPY-CLEAN");
        check(webUnderlined, "NOLINK-WEB-KEPT");
    }

    // Оглавление: уровни из формата блоков
    doc->clear();
    {
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
    }
    {
        QList<TypefaceFormatter::TocEntry> entries=fmt.collectTocEntries();
        check(entries.size()==3, "TOC-COUNT");
        check(entries.size()==3 && entries.at(0).level==1 &&
              entries.at(0).text==QStringLiteral("Chapter"), "TOC-H1");
        check(entries.size()==3 && entries.at(1).level==2 &&
              entries.at(1).text==QStringLiteral("Section"), "TOC-H2");
        check(entries.size()==3 && entries.at(2).level==4 &&
              entries.at(2).text==QStringLiteral("Deep"), "TOC-H4");
    }
    check(!TypefaceFormatter::tocGoPrefix().isEmpty(), "TOC-GOPREFIX");
    check(!TypefaceFormatter::tocAnchorHref().isEmpty(), "TOC-ANCHOR");

    // Выпадашка заголовков: индексы 3/4/5 -> H4/H5/H6 с эталонными
    // отступами, индекс 6 -> обычный текст. Связка менюACTION-порядок
    // (addAction) и onHeadingClicked рвется молча, ловим здесь
    doc->clear();
    {
        QTextCursor c(doc);
        c.insertText(QStringLiteral("Deep"));
        QTextCursor sel(doc);
        sel.select(QTextCursor::Document);
        area.setTextCursor(sel);

        fmt.onHeadingClicked(3);
        QTextBlockFormat f4=doc->firstBlock().blockFormat();
        check(f4.headingLevel()==4, "HEADING-H4-LEVEL");
        check(f4.topMargin()==12 && f4.bottomMargin()==12, "HEADING-H4-MARGINS");
        const double size4=doc->firstBlock().begin().fragment().charFormat().fontPointSize();

        fmt.onHeadingClicked(4);
        QTextBlockFormat f5=doc->firstBlock().blockFormat();
        check(f5.headingLevel()==5, "HEADING-H5-LEVEL");
        check(f5.topMargin()==12 && f5.bottomMargin()==4, "HEADING-H5-MARGINS");
        const double size5=doc->firstBlock().begin().fragment().charFormat().fontPointSize();

        fmt.onHeadingClicked(5);
        QTextBlockFormat f6=doc->firstBlock().blockFormat();
        check(f6.headingLevel()==6, "HEADING-H6-LEVEL");
        check(f6.topMargin()==0 && f6.bottomMargin()==0, "HEADING-H6-MARGINS");
        const double size6=doc->firstBlock().begin().fragment().charFormat().fontPointSize();

        // Кегль строго убывает с уровнем: лесенка 48/32/24/20/16/12
        check(size4>size5 && size5>size6, "HEADING-SIZE-MONOTONIC");

        fmt.onHeadingClicked(6);
        check(doc->firstBlock().blockFormat().headingLevel()==0, "HEADING-NORMAL-OFF");

        // Переключение вкл/выкл/вкл не растит кегль: второе применение
        // дает тот же размер, а не умноженный
        {
            QTextCursor sel(doc);
            sel.select(QTextCursor::Document);
            area.setTextCursor(sel);

            fmt.onHeadingClicked(0);
            const double firstOn=doc->firstBlock().begin().fragment().charFormat().fontPointSize();
            fmt.onHeadingClicked(0);
            fmt.onHeadingClicked(0);
            const double secondOn=doc->firstBlock().begin().fragment().charFormat().fontPointSize();
            check(firstOn>0.0 && firstOn==secondOn, "HEADING-RETOGGLE-STABLE");
        }

        // Наследие старого билда: ступень + явный размер
        {
            QTextCursor c(doc);
            c.select(QTextCursor::Document);
            c.insertText(QStringLiteral("Legacy"));
            c.setBlockFormat(QTextBlockFormat()); // ровно обычный абзац
            QTextCursor sel(doc);
            sel.select(QTextCursor::Document);
            area.setTextCursor(sel);

            QTextCharFormat legacy;
            legacy.setFontPointSize(11);
            legacy.setProperty(QTextFormat::FontSizeAdjustment, -1);
            QTextCursor merge(doc);
            merge.select(QTextCursor::Document);
            merge.mergeCharFormat(legacy);

            fmt.onHeadingClicked(0); // H1 поверх ступени
            QTextCharFormat applied=doc->firstBlock().begin().fragment().charFormat();
            check(!applied.hasProperty(QTextFormat::FontSizeAdjustment),
                  "LEGACY-ADJ-STRIPPED");
            const double legacyOn=applied.fontPointSize();

            fmt.onHeadingClicked(0); // снять
            QTextCharFormat removed=doc->firstBlock().begin().fragment().charFormat();
            check(!removed.hasProperty(QTextFormat::FontSizeAdjustment),
                  "LEGACY-ADJ-STRIPPED-OFF");

            fmt.onHeadingClicked(0); // заново: тот же кегль, не мелкий
            QTextCharFormat reapplied=doc->firstBlock().begin().fragment().charFormat();
            check(reapplied.fontPointSize()==legacyOn &&
                  !reapplied.hasProperty(QTextFormat::FontSizeAdjustment),
                  "LEGACY-REAPPLY-STABLE");
        }

        // Загруженный документ (stash нет - userData не сохраняется):
        // снятие нормализует кегль к базовому, повторное применение
        // не умножает след прошлого заголовка
        {
            QTextCursor c(doc);
            c.select(QTextCursor::Document);
            c.insertText(QStringLiteral("Loaded"));
            c.setBlockFormat(QTextBlockFormat()); // ровно обычный абзац
            QTextCursor sel(doc);
            sel.select(QTextCursor::Document);
            area.setTextCursor(sel);

            fmt.onHeadingClicked(0);
            const double freshOn=doc->firstBlock().begin().fragment().charFormat().fontPointSize();
            doc->firstBlock().setUserData(nullptr); // как после save/load
            fmt.onHeadingClicked(0); // снять без stash
            const double afterOff=doc->firstBlock().begin().fragment().charFormat().fontPointSize();
            fmt.onHeadingClicked(0); // применить заново
            const double reloadOn=doc->firstBlock().begin().fragment().charFormat().fontPointSize();
            check(afterOff<=freshOn && reloadOn==freshOn, "HEADING-RELOAD-STABLE");
        }
    }

    qDebug() << (fails==0 ? "TYPEFACEPROBE-OK" : "TYPEFACEPROBE-BAD");
    return fails;
}
