#include <QTextBlock>
#include <QTextBlockUserData>
#include <QTextDocument>
#include <QPainter>
#include <QTextLayout>
#include <QFont>
#include <QRegularExpression>
#include <QFile>
#include <QMap>
#include <QClipboard>
#include <QToolTip>
#include <QDebug>
#include <QColorDialog>
#include <QColor>
#include <QDomNode>
#include <QUrl>
#include <QTextTable>
#include <QApplication>

#include <QCXXHighlighter>
#include <QBashHighlighter>
#include <QCLikeHighlighter>
#include <QCssHighlighter>
#include <QGLSLHighlighter>
#include <QIniHighlighter>
#include <QPhpHighlighter>
#include <QSqlHighlighter>
#include <QJSONHighlighter>
#include <QLuaHighlighter>
#include <QPythonHighlighter>
#include <QStyleSyntaxHighlighter>
#include <QSyntaxStyle>
#include <QXMLHighlighter>

#include "TypefaceFormatter.h"

#include "../Editor.h"
#include "../EditorConfig.h"
#include "../EditorTextArea.h"
#include "../EditorToolBarAssistant.h"
#include "../EditorFontFamilyComboBox.h"
#include "../EditorSpecialCharDialog.h"
#include "../EditorCursorPositionDetector.h"
#include "../EditorFontSizeComboBox.h"
#include "../../TraceLogger.h"
#include "libraries/helpers/MessageHelper.h"



TypefaceFormatter::TypefaceFormatter()
{

}


// Форматирование Bold
void TypefaceFormatter::onBoldClicked(void)
{
    // TRACELOG

    smartFormat(Bold);
}


// Форматирование Italic
void TypefaceFormatter::onItalicClicked(void)
{
    // TRACELOG

    smartFormat(Italic);
}


// Форматирование подчеркивания
void TypefaceFormatter::onUnderlineClicked(void)
{
    // TRACELOG

    smartFormat(Underline);
}


// Форматирование Strike
void TypefaceFormatter::onStrikeOutClicked(void)
{
    // TRACELOG

    smartFormat(StrikeOut);
}


// Форматирование SuperScript
void TypefaceFormatter::onSuperScriptClicked(void)
{
    // TRACELOG

    smartFormat(SuperScript);
}


// Форматирование SubScript
void TypefaceFormatter::onSubScriptClicked(void)
{
    // TRACELOG

    smartFormat(SubScript);
}


void TypefaceFormatter::smartFormat(int formatType)
{
    // Если выделение есть
    if(textArea->textCursor().hasSelection())
    {
        // Переназначение позиций выделения, иначе снятие форматирования не срабатывает
        // в случае выделения справа-налево (снизу-вверх)
        QTextCursor cursor = textArea->textCursor();
        const int anchor = cursor.anchor();
        const int position = cursor.position();
        if (anchor > position)
        {
            cursor.setPosition(position, QTextCursor::MoveAnchor);
            cursor.setPosition(anchor, QTextCursor::KeepAnchor);
            textArea->setTextCursor(cursor);
        }

        if(formatType==Bold)
        {
            if(textArea->fontWeight() != QFont::Bold)
                textArea->setFontWeight(QFont::Bold); // Bold
            else
                textArea->setFontWeight(QFont::Normal); // Remove Bold
        }

        if(formatType==Italic)
        {
            if(!textArea->fontItalic())
                textArea->setFontItalic(true);
            else
                textArea->setFontItalic(false);
        }

        if(formatType==Underline)
        {
            if(!textArea->fontUnderline())
                textArea->setFontUnderline(true);
            else
                textArea->setFontUnderline(false);
        }

        if(formatType==StrikeOut)
        {
            QTextCharFormat format;
            if(!textArea->textCursor().charFormat().fontStrikeOut())
                format.setFontStrikeOut(true);
            else
                format.setFontStrikeOut(false);
            textArea->textCursor().mergeCharFormat(format);
        }

        if(formatType==SuperScript)
        {
            QTextCharFormat format;
            if(textArea->textCursor().charFormat().verticalAlignment() != QTextCharFormat::AlignSuperScript)
                format.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
            else
                format.setVerticalAlignment(QTextCharFormat::AlignNormal);
            textArea->textCursor().mergeCharFormat(format);
        }

        if(formatType==SubScript)
        {
            QTextCharFormat format;
            if(textArea->textCursor().charFormat().verticalAlignment() != QTextCharFormat::AlignSubScript)
                format.setVerticalAlignment(QTextCharFormat::AlignSubScript);
            else
                format.setVerticalAlignment(QTextCharFormat::AlignNormal);
            textArea->textCursor().mergeCharFormat(format);
        }
    }
    else
    {
        // Иначе надо выделить дополнительным курсором слово на котором стоит курсор
        QTextCursor cursor=textArea->textCursor();

        int cursorPosition=cursor.position();

        // Выделяется последний символ
        cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
        QString prevCharacterAsString=cursor.selectedText();
        QChar prevCharacter;
        if(prevCharacterAsString.length()==1)
            prevCharacter=prevCharacterAsString[0];

        // Если последний символ не является пробелом
        if( !prevCharacter.isSpace() )
        {
            // Выделяется все слово
            cursor.setPosition(cursorPosition); // Сначала сбрасывается позиция на ту, которая была в начале до выделения последнего символа
            cursor.select(QTextCursor::WordUnderCursor); // Учесть, что WordUnderCursor - это выделение слова и пробельных символов после него
        }

        // К выделению применяется/отменяется форматирование
        QTextCharFormat format;

        if(formatType==Bold)
        {
            if(cursor.charFormat().fontWeight() != QFont::Bold)
                format.setFontWeight(QFont::Bold);
            else
                format.setFontWeight(QFont::Normal); // Remove Bold
        }

        if(formatType==Italic)
        {
            if(!cursor.charFormat().fontItalic())
                format.setFontItalic(true);
            else
                format.setFontItalic(false);
        }

        if(formatType==Underline)
        {
            if(!cursor.charFormat().fontUnderline())
                format.setFontUnderline(true);
            else
                format.setFontUnderline(false);
        }

        if(formatType==StrikeOut)
        {
            if(!cursor.charFormat().fontStrikeOut())
                format.setFontStrikeOut(true);
            else
                format.setFontStrikeOut(false);
        }

        if(formatType==SuperScript)
        {
            if(cursor.charFormat().verticalAlignment() != QTextCharFormat::AlignSuperScript)
                format.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
            else
                format.setVerticalAlignment(QTextCharFormat::AlignNormal);
            textArea->mergeCurrentCharFormat(format);
        }

        if(formatType==SubScript)
        {
            if(cursor.charFormat().verticalAlignment() != QTextCharFormat::AlignSubScript)
                format.setVerticalAlignment(QTextCharFormat::AlignSubScript);
            else
                format.setVerticalAlignment(QTextCharFormat::AlignNormal);
            textArea->mergeCurrentCharFormat(format);
        }

        cursor.mergeCharFormat(format);
    }

    emit updateOutlineButtonHiglight();
}


// Форматирование моноширинным шрифтом
void TypefaceFormatter::onMonospaceClicked(void)
{
    // TRACELOG

    textArea->textCursor().beginEditBlock();

    // Устанавливается шрифт
    QFont font;
    font.fromString( editorConfig->get_monospace_font() );
    this->doChangeFontFamily( font.family() );

    // Новый установленный шрифт показывается в выпадающем списке шрифтов
    emit changeFontselectOnDisplay(font.family());

    // Если в настройках разрешена автоматическая установка нужного размера
    // моноширинного шрифта
    if(editorConfig->get_monospace_font_size_apply()==true)
    {
        // Устанавливается нужный размер
        this->doChangeFontPointSize( editorConfig->get_monospace_font_size() );

        // В выпадающем списке размеров выставляется установленный размер
        emit changeFontsizeOnDisplay( editorConfig->get_monospace_font_size() );
    }

    textArea->textCursor().endEditBlock();
}


// Выбор пункта меню кнопки кода: форматирование с подсветкой выбранного языка
void TypefaceFormatter::onCodeLanguageClicked(int language)
{
    if(language<CodePlain || language>=CodeCount)
        return;

    formatCodeWithLanguage(language);
}


// Форматирование в качестве кода
void TypefaceFormatter::formatCodeWithLanguage(int language)
{
    // TRACELOG

    // Если выделения нет
    if(!textArea->textCursor().hasSelection())
        return;

    textArea->textCursor().beginEditBlock();
    // Далее при любом выходе из метода нужно вызывать textArea->textCursor().endEditBlock()

    // Обработка мягкого переноса в выделенном тексте
    // Учитываются мягкие переносы до выделенного текста (1-й символ до выделения) и в выделенных абзацах
    workingSoftCarryInSelection();
    
    bool enableIndent;

    // Проверяется, выбран ли четко блок (блоки) текста,
    // это нужно знать для того, чтобы форматировать/не форматировать отступы
    // Блок - это текст между <p>...</p>
    if(!editor->cursorPositionDetector->isBlockSelect())
    {
        // Если выбран не четко блок (блоки)

        // Нужно определить, находится ли выделение в пределах одного блока
        // Если выделение вылазит за пределы одного блока, форматировать отступы нельзя

        // Выясняются позиции начала и конца блока
        QTextBlock currentBlock = textArea->textCursor().block();
        int blockStart=currentBlock.position();
        int blockStop=currentBlock.position()+currentBlock.length();

        // Выясняются позиции начала и конца выделения
        int selectStart=textArea->textCursor().selectionStart();
        int selectStop =textArea->textCursor().selectionEnd();

        qDebug() << "Code format action, block " << blockStart << blockStop << " selection " << selectStart << selectStop;

        if(blockStart<=selectStart && blockStop>=selectStop)
            enableIndent=false; // Выбран кусок текста в пределах блока
        else{
            qDebug() << "The selection is outside one block";
            //Хорошо бы писать в статусную строку сообщение, почему форматирование не выполнено
            textArea->textCursor().endEditBlock();
            return;
        }
    }
    else
        enableIndent=true; // Выбран четко блок (блоки) текста, нужно делать отступ


    // Символьное форматирование (жирность, курсив, подчеркивание) сохраняется,
    // унифицируются только семейство, размер и цвет шрифта ниже.
    // Раньше здесь вызывался onTextOnlyClicked(), который вырезал фрагмент
    // и вставлял его чистым текстом, поэтому форматирование в Code убивало
    // предыдущее форматирование (баг #94)

    // Маркер языка: заменить/вставить строку вида // {lang = C++}
    // в начале выделения (для Plain и JSON - наоборот убрать наш маркер)
    if(!normalizeCodeMarker(language))
    {
        textArea->textCursor().endEditBlock();
        return;
    }

    // Устанавливается шрифт
    QFont font;
    font.fromString(editorConfig->get_code_font());
    this->doChangeFontFamily( font.family() );

    // Новый установленный шрифт показывается в выпадающем списке шрифтов
    emit changeFontselectOnDisplay(font.family());

    // Если в настройках разрешена автоматическая установка нужного размера
    if(editorConfig->get_code_font_size_apply()==true)
    {
        // Устанавливается нужный размер
        this->doChangeFontPointSize( editorConfig->get_code_font_size() );

        // В выпадающем списке размеров выставляется установленный размер
        emit changeFontsizeOnDisplay(editorConfig->get_code_font_size());
    }

    // Устанавливается цвет текста
    QColor codeColor(editorConfig->get_code_font_color ());
    textArea->setTextColor(codeColor);

    // Если в настройках разрешена автоматическая установка нужного отступа
    // И выбран четко блок (блоки)
    if(editorConfig->get_code_indent_size_apply()==true &&
            enableIndent==true)
    {
        // Создание форматирования
        QTextBlockFormat indentFormatting;
        indentFormatting.setLeftMargin(editorConfig->get_code_indent_size());
        indentFormatting.setAlignment(Qt::AlignLeft); // Выравнивание по левому краю

        // Форматирование для добавления отступа
        textArea->textCursor().mergeBlockFormat(indentFormatting);

        editor->updateIndentsliderToActualFormat();
    }

    // Подсветка синтаксиса выбранного языка поверх базового оформления кода
    if(language!=CodePlain)
        applyCodeHighlight(language);

    // Фоновая заливка блока: код (вместе со строкой-маркером) выглядит
    // единой плашкой. Обычный код заливку наоборот снимает, чтобы Plain
    // оставался старым поведением без карточки
    if(language!=CodePlain)
        setCodeBlockBackground(QBrush(codeCardColor()));
    else
        setCodeBlockBackground(QBrush(Qt::NoBrush));

    textArea->textCursor().endEditBlock();
}


// Одноразовая подсветка синтаксиса в текущем выделении через хайлайтеры
// QCodeEditor на временном документе. Вызывается внутри edit-блокa
// formatCodeWithLanguage, свое begin/end не делает
void TypefaceFormatter::applyCodeHighlight(int language)
{
    QTextCursor cursor=textArea->textCursor();
    if(!cursor.hasSelection())
        return;

    int selectStart=cursor.selectionStart();
    int selectEnd=cursor.selectionEnd();

    QTextDocument *doc=textArea->document();

    // Собираются блоки документа, покрытые выделением, и смещения
    // выделенных подстрок внутри текста каждого блока
    QList<QTextBlock> realBlocks;
    QList<int> blockSelStart;
    QList<int> blockSelLength;

    for(QTextBlock block=doc->findBlock(selectStart);
        block.isValid() && block.position()<selectEnd;
        block=block.next())
    {
        int blockTextLen=block.length()-1; // Без символа конца абзаца
        if(blockTextLen<0)
            blockTextLen=0;

        int from=qMax(selectStart, block.position())-block.position();
        int to=qMin(selectEnd, block.position()+blockTextLen)-block.position();

        if(to>from)
        {
            realBlocks.append(block);
            blockSelStart.append(from);
            blockSelLength.append(to-from);
        }
    }

    if(realBlocks.isEmpty())
        return;

    // Выделенный текст переносится во временный документ построчно,
    // чтобы штатные хайлайтеры отработали многострочные конструкции
    // (комментарии /* */, строки ''' и т.п.) через состояния блоков
    QStringList lines;
    for(int i=0; i<realBlocks.size(); ++i)
        lines.append( realBlocks.at(i).text().mid(blockSelStart.at(i), blockSelLength.at(i)) );

    QTextDocument tempDoc;
    tempDoc.setPlainText( lines.join("\n") );

    // Защита от рассинхрона блоков временного и настоящего документов
    if(tempDoc.blockCount()!=realBlocks.size())
        return;

    QStyleSyntaxHighlighter *highlighter=nullptr;
    switch(language)
    {
    case CodeCxx:    highlighter=new QCXXHighlighter(&tempDoc); break;
    case CodeGlsl:   highlighter=new QGLSLHighlighter(&tempDoc); break;
    case CodeJson:   highlighter=new QJSONHighlighter(&tempDoc); break;
    case CodeLua:    highlighter=new QLuaHighlighter(&tempDoc); break;
    case CodePython: highlighter=new QPythonHighlighter(&tempDoc); break;
    case CodeAngular:
    case CodeHtml:   highlighter=new QXMLHighlighter(&tempDoc); break;
    case CodeBash:
    case CodeCMake:
    case CodeMake:
    case CodeYaml:   highlighter=new QBashHighlighter(&tempDoc); break;
    case CodeC:      highlighter=new QCLikeHighlighter(QStringLiteral(":/languages/c.xml"), &tempDoc); break;
    case CodeCSharp: highlighter=new QCLikeHighlighter(QStringLiteral(":/languages/csharp.xml"), &tempDoc); break;
    case CodeGo:     highlighter=new QCLikeHighlighter(QStringLiteral(":/languages/go.xml"), &tempDoc); break;
    case CodeJava:   highlighter=new QCLikeHighlighter(QStringLiteral(":/languages/java.xml"), &tempDoc); break;
    case CodeJs:
    case CodeReact:  highlighter=new QCLikeHighlighter(QStringLiteral(":/languages/js.xml"), &tempDoc); break;
    case CodeQml:    highlighter=new QCLikeHighlighter(QStringLiteral(":/languages/qml.xml"), &tempDoc); break;
    case CodeTypescript: highlighter=new QCLikeHighlighter(QStringLiteral(":/languages/typescript.xml"), &tempDoc); break;
    case CodeRust:   highlighter=new QCLikeHighlighter(QStringLiteral(":/languages/rust.xml"), &tempDoc); break;
    case CodeSql:    highlighter=new QSqlHighlighter(&tempDoc); break;
    case CodePhp:    highlighter=new QPhpHighlighter(&tempDoc); break;
    case CodeCss:    highlighter=new QCssHighlighter(&tempDoc); break;
    case CodeIni:    highlighter=new QIniHighlighter(&tempDoc); break;
    default:
        return;
    }

    highlighter->setSyntaxStyle(codeStyleForLanguage(language));
    highlighter->rehighlight();

    // Форматы из временного документа переносятся в выделение.
    // Из стиля берутся только цвет текста, жирность и курсив.
    // Фон осознанно не переносится: в стиле по умолчанию у части
    // форматов белый фон, который ломал бы темные темы
    for(int i=0; i<realBlocks.size(); ++i)
    {
        QTextBlock tempBlock=tempDoc.findBlockByNumber(i);
        if(!tempBlock.isValid() || tempBlock.layout()==nullptr)
            continue;

        int basePos=realBlocks.at(i).position()+blockSelStart.at(i);

        QVector<QTextLayout::FormatRange> ranges=tempBlock.layout()->formats();
        for(int r=0; r<ranges.size(); ++r)
        {
            QTextCharFormat source=ranges.at(r).format;

            // Пустой формат (стиль не знает такого имени, например Text
            // или Operator в теме по умолчанию) пропускается: кисть
            // по умолчанию черная и валидная, проверять надо стиль кисти
            if(source.foreground().style()==Qt::NoBrush)
                continue;

            QTextCharFormat target;
            target.setForeground(source.foreground());
            if(source.fontWeight()!=QFont::Normal)
                target.setFontWeight(source.fontWeight());
            if(source.fontItalic())
                target.setFontItalic(true);

            QTextCursor applyCursor=textArea->textCursor();
            applyCursor.setPosition(basePos+ranges.at(r).start);
            applyCursor.setPosition(basePos+ranges.at(r).start+ranges.at(r).length, QTextCursor::KeepAnchor);
            applyCursor.mergeCharFormat(target);
        }
    }

    delete highlighter;
}


// Каноническое имя языка для маркера блока кода. Без перевода:
// маркер в тексте заметки всегда на английском. Пусто для Plain и JSON
// (у JSON нет комментариев, маркер вставлять не во что)
QString TypefaceFormatter::codeMarkerName(int language)
{
    switch(language)
    {
    case CodeAngular: return QStringLiteral("Angular");
    case CodeBash:    return QStringLiteral("Bash");
    case CodeC:       return QStringLiteral("C");
    case CodeCMake:    return QStringLiteral("CMake");
    case CodeCss:      return QStringLiteral("CSS");
    case CodeCxx:     return QStringLiteral("C++");
    case CodeCSharp:  return QStringLiteral("C#");
    case CodeGlsl:    return QStringLiteral("GLSL");
    case CodeGo:      return QStringLiteral("Go");
    case CodeHtml:    return QStringLiteral("HTML");
    case CodeIni:     return QStringLiteral("INI");
    case CodeJava:    return QStringLiteral("Java");
    case CodeJs:      return QStringLiteral("JavaScript");
    case CodeLua:     return QStringLiteral("Lua");
    case CodeMake:    return QStringLiteral("Make");
    case CodePython:  return QStringLiteral("Python");
    case CodeQml:     return QStringLiteral("QML");
    case CodeReact:   return QStringLiteral("React");
    case CodeRust:    return QStringLiteral("Rust");
    case CodeSql:     return QStringLiteral("SQL");
    case CodeTypescript: return QStringLiteral("TypeScript");
    case CodeXml:     return QStringLiteral("XML");
    case CodeYaml:    return QStringLiteral("YAML");
    default:          return QString();
    }
}


// Строка-маркер языка для вставки в текст. Пусто - у языка нет маркера
QString TypefaceFormatter::codeMarkerLine(int language)
{
    QString name=codeMarkerName(language);
    if(name.isEmpty())
        return QString();

    switch(language)
    {
    case CodePython:
    case CodeBash:
    case CodeCMake:
    case CodeMake:
    case CodeYaml:
        return QStringLiteral("# {lang = ")+name+QStringLiteral("}");
    case CodeLua:
    case CodeSql:
        return QStringLiteral("-- {lang = ")+name+QStringLiteral("}");
    case CodeIni:
        return QStringLiteral("; {lang = ")+name+QStringLiteral("}");
    case CodeCss:
        return QStringLiteral("/* {lang = ")+name+QStringLiteral("} */");
    case CodeHtml:
    case CodeXml:
    case CodeAngular:
        return QStringLiteral("<!-- {lang = ")+name+QStringLiteral("} -->");
    default:
        return QStringLiteral("// {lang = ")+name+QStringLiteral("}");
    }
}


// Распознать наш маркер в строке, вернуть язык или -1.
// Адрес ссылки-кнопки [copy] в строке маркера
QString TypefaceFormatter::copyCodeHref(void)
{
    return QStringLiteral("mytetra://copy-code");
}


// Текст кнопки копирования, добавляемый к строке маркера через пробел
QString TypefaceFormatter::copyCodeSuffix(void)
{
    return QStringLiteral("[copy]");
}


// Цвет плашки блока кода
QColor TypefaceFormatter::codeCardColor(void)
{
    return QColor(QStringLiteral("#F2F4F7"));
}


// Распознать наш маркер в строке, вернуть язык или -1.
// Форматы: // {lang = Имя}, # {lang = Имя}, -- {lang = Имя},
// ; {lang = Имя}, /* {lang = Имя} */, <!-- {lang = Имя} -->.
// Совпадение строгое: вся строка - только маркер
int TypefaceFormatter::codeMarkerLanguage(const QString &line)
{
    QString trimmed=line.trimmed();

    static const QRegularExpression lineMarker(
        QStringLiteral("^(?://|#|--|;)\\s*\\{lang\\s*=\\s*(.+?)\\}\\s*(?:\\[copy\\])?\\s*$"));
    static const QRegularExpression blockMarker(
        QStringLiteral("^(?:<!--|/\\*)\\s*\\{lang\\s*=\\s*(.+?)\\}\\s*(?:-->|\\*/)\\s*(?:\\[copy\\])?\\s*$"));

    QString name;
    QRegularExpressionMatch match=lineMarker.match(trimmed);
    if(match.hasMatch())
        name=match.captured(1);
    else
    {
        match=blockMarker.match(trimmed);
        if(match.hasMatch())
            name=match.captured(1);
    }

    if(name.isEmpty())
        return -1;

    // JSON маркеров не имеет, его имя здесь не опознается
    for(int lang=CodeAngular; lang<CodeCount; ++lang)
    {
        if(lang==CodeJson)
            continue;
        if(codeMarkerName(lang)==name)
            return lang;
    }

    return -1;
}


// Обеспечить маркер языка в начале выделения: заменить наш маркер
// (в выделении или строкой выше) на новый, вставить новый сверху
// или убрать маркер для языка без маркера (Plain, JSON).
// Вызывается внутри edit-блока formatCodeWithLanguage.
// Возвращает false, если после обработки выделять нечего
bool TypefaceFormatter::normalizeCodeMarker(int language)
{
    QTextCursor cursor=textArea->textCursor();
    if(!cursor.hasSelection())
        return false;

    int selectStart=cursor.selectionStart();
    int selectEnd=cursor.selectionEnd();

    QTextDocument *doc=textArea->document();
    QTextBlock firstBlock=doc->findBlock(selectStart);

    // Наш маркер либо первая строка выделения, либо строка прямо над ним
    QTextBlock markerBlock;
    bool markerAbove=false;
    if(codeMarkerLanguage(firstBlock.text())!=-1)
        markerBlock=firstBlock;
    else
    {
        QTextBlock prevBlock=firstBlock.previous();
        if(prevBlock.isValid() &&
           codeMarkerLanguage(prevBlock.text())!=-1 &&
           prevBlock.position()+prevBlock.length()==firstBlock.position())
        {
            markerBlock=prevBlock;
            markerAbove=true;
        }
    }

    QString newMarker=codeMarkerLine(language);

    int newStart=selectStart;
    int newEnd=selectEnd;

    QTextCursor edit(doc);

    // Язык без маркера: просто убрать найденный маркер
    if(newMarker.isEmpty())
    {
        if(!markerBlock.isValid())
            return true;

        int markerPos=markerBlock.position();
        int markerLen=markerBlock.text().length();

        if(markerAbove)
        {
            // Маркер строкой выше: удалить его вместе с переводом строки
            edit.setPosition(markerPos);
            edit.setPosition(firstBlock.position(), QTextCursor::KeepAnchor);
            edit.removeSelectedText();

            newStart=selectStart-(markerLen+1);
            newEnd=selectEnd-(markerLen+1);
        }
        else
        {
            QTextBlock nextBlock=markerBlock.next();
            bool onlyMarker=(selectEnd<=markerPos+markerLen);

            if(onlyMarker && nextBlock.isValid())
            {
                // Выделен только маркер: удалить его вместе с переводом строки
                edit.setPosition(markerPos);
                edit.setPosition(nextBlock.position(), QTextCursor::KeepAnchor);
            }
            else if(onlyMarker)
            {
                // Выделен только маркер в последнем блоке: удалить его текст
                edit.setPosition(markerPos);
                edit.setPosition(markerPos+markerLen, QTextCursor::KeepAnchor);
            }
            else
            {
                // Маркер первый в выделении, дальше код: удалить текст маркера
                // и его перевод строки (следующий блок заведомо есть,
                // раз выделение уходит дальше маркера)
                edit.setPosition(markerPos);
                edit.setPosition(markerPos+markerLen+1, QTextCursor::KeepAnchor);
            }
            edit.removeSelectedText();

            newStart=markerPos;
            newEnd=onlyMarker ? markerPos : selectEnd-(markerLen+1);
        }

        if(newEnd<=newStart)
        {
            QTextCursor empty(doc);
            empty.setPosition(newStart);
            textArea->setTextCursor(empty);
            return false;
        }

        QTextCursor newCursor(doc);
        newCursor.setPosition(newStart);
        newCursor.setPosition(newEnd, QTextCursor::KeepAnchor);
        textArea->setTextCursor(newCursor);
        return true;
    }

    // Язык с маркером: заменить найденный или вставить новый сверху.
    // Ссылка-кнопка [copy] приписывается всегда
    const QString fullMarker=newMarker+QStringLiteral(" ")+copyCodeSuffix();

    if(markerBlock.isValid())
    {
        int markerPos=markerBlock.position();
        int markerLen=markerBlock.text().length();

        edit.setPosition(markerPos);
        edit.setPosition(markerPos+markerLen, QTextCursor::KeepAnchor);
        edit.insertText(fullMarker);

        applyCopyLinkFormat(markerPos, newMarker.length());

        int delta=fullMarker.length()-markerLen;
        if(markerAbove)
        {
            newStart=selectStart+delta;
            newEnd=selectEnd+delta;
        }
        else
        {
            newStart=markerPos;
            newEnd=selectEnd+delta;
        }
    }
    else
    {
        int insertPos=firstBlock.position();

        edit.setPosition(insertPos);
        edit.insertText(fullMarker+QStringLiteral("\n"));

        applyCopyLinkFormat(insertPos, newMarker.length());

        int inserted=fullMarker.length()+1;

        // Маркер включается в выделение, чтобы ниже получить
        // базовое оформление и подсветку как комментария
        newStart=insertPos;
        newEnd=selectEnd+inserted;
    }

    QTextCursor newCursor(doc);
    newCursor.setPosition(newStart);
    newCursor.setPosition(newEnd, QTextCursor::KeepAnchor);
    textArea->setTextCursor(newCursor);
    return newEnd>newStart;
}


// Стиль подсветки для языка: отдельная тема в :/styles/<имя>.xml
// (палитры в духе родных nano-схем, приглушенные под светлый фон),
// поэтому языки легко отличить с первого взгляда. Кешируется,
// fallback - стиль по умолчанию
QSyntaxStyle *TypefaceFormatter::codeStyleForLanguage(int language)
{
    static QMap<int, QSyntaxStyle*> cache;

    if(cache.contains(language))
        return cache.value(language);

    QString styleName;
    switch(language)
    {
    case CodeAngular:    styleName=QStringLiteral("angular"); break;
    case CodeBash:       styleName=QStringLiteral("bash"); break;
    case CodeC:          styleName=QStringLiteral("c"); break;
    case CodeCMake:      styleName=QStringLiteral("cmake"); break;
    case CodeCss:        styleName=QStringLiteral("css"); break;
    case CodeCxx:        styleName=QStringLiteral("cxx"); break;
    case CodeCSharp:     styleName=QStringLiteral("csharp"); break;
    case CodeGlsl:       styleName=QStringLiteral("glsl"); break;
    case CodeGo:         styleName=QStringLiteral("go"); break;
    case CodeHtml:       styleName=QStringLiteral("html"); break;
    case CodeIni:        styleName=QStringLiteral("ini"); break;
    case CodeJava:       styleName=QStringLiteral("java"); break;
    case CodeJs:         styleName=QStringLiteral("js"); break;
    case CodeJson:       styleName=QStringLiteral("json"); break;
    case CodeLua:        styleName=QStringLiteral("lua"); break;
    case CodeMake:       styleName=QStringLiteral("make"); break;
    case CodePhp:        styleName=QStringLiteral("php"); break;
    case CodePython:     styleName=QStringLiteral("python"); break;
    case CodeQml:        styleName=QStringLiteral("qml"); break;
    case CodeReact:      styleName=QStringLiteral("react"); break;
    case CodeRust:       styleName=QStringLiteral("rust"); break;
    case CodeSql:        styleName=QStringLiteral("sql"); break;
    case CodeTypescript: styleName=QStringLiteral("typescript"); break;
    case CodeXml:        styleName=QStringLiteral("xml"); break;
    case CodeYaml:       styleName=QStringLiteral("yaml"); break;
    default:
        break;
    }

    QSyntaxStyle *style=QSyntaxStyle::defaultStyle();
    if(!styleName.isEmpty())
    {
        QFile styleFile(QStringLiteral(":/styles/")+styleName+QStringLiteral(".xml"));
        if(styleFile.open(QIODevice::ReadOnly))
        {
            QSyntaxStyle *custom=new QSyntaxStyle();
            if(custom->load(styleFile.readAll()))
                style=custom;
            else
                delete custom;
        }
    }

    cache.insert(language, style);
    return style;
}


// Залить фоном абзацы текущего выделения (единая плашка блока кода).
// Вызывается внутри edit-блока formatCodeWithLanguage
void TypefaceFormatter::setCodeBlockBackground(const QBrush &brush)
{
    QTextBlockFormat backgroundFormatting;
    backgroundFormatting.setBackground(brush);

    textArea->textCursor().mergeBlockFormat(backgroundFormatting);
}


// Оформить суффикс-кнопку [copy] в строке маркера как ссылку.
// lineStart - начало строки, markerLen - длина текста маркера
// без суффикса. Вызывается внутри edit-блока
void TypefaceFormatter::applyCopyLinkFormat(int lineStart, int markerLen)
{
    const int linkStart=lineStart+markerLen+1; // После пробела
    const int linkLength=copyCodeSuffix().length();

    QTextCursor linkCursor(textArea->document());
    linkCursor.setPosition(linkStart);
    linkCursor.setPosition(linkStart+linkLength, QTextCursor::KeepAnchor);

    QTextCharFormat linkFormat;
    linkFormat.setAnchor(true);
    linkFormat.setAnchorHref(copyCodeHref());
    linkFormat.setFontUnderline(true);

    linkCursor.mergeCharFormat(linkFormat);
}


// Проверка, что абзац выглядит как строка блока кода: либо залит
// плашкой карточки, либо набран шрифтом кода (блоки, оформленные
// до появления карточек, опознаются по шрифту)
bool TypefaceFormatter::isCodeBlock(const QTextBlock &block) const
{
    if(!block.isValid())
        return false;

    if(block.blockFormat().background().style()!=Qt::NoBrush &&
       block.blockFormat().background().color()==codeCardColor())
        return true;

    // Шрифт кода из настроек
    QFont codeFont;
    codeFont.fromString(editorConfig->get_code_font());

    for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
    {
        QTextFragment fragment=it.fragment();

        if(!fragment.isValid() || fragment.length()==0)
            continue;

        // Пустые и пробельные фрагменты не показательны
        QString text=fragment.text();
        bool hasNonSpace=false;
        for(QChar ch : text)
        {
            if(!ch.isSpace())
            {
                hasNonSpace=true;
                break;
            }
        }

        if(!hasNonSpace)
            continue;

        return fragment.charFormat().fontFamily()==codeFont.family();
    }

    return false;
}


// Собрать текст кода под строкой маркера. Точка может быть как на
// самом маркере (клик по [copy]), так и ниже в теле кода (контекстное
// меню): во втором случае маркер ищется подъемом вверх по строкам кода.
// Пусто - маркера нет или кода под ним нет
QString TypefaceFormatter::collectCodeBelowMarker(int docPosition) const
{
    QTextDocument *doc=textArea->document();

    QTextBlock block=doc->findBlock(docPosition);

    if(block.isValid() && codeMarkerLanguage(block.text())<0)
    {
        // Подъем вверх, пока идут строки кода
        int guard=0;
        while(block.isValid() && isCodeBlock(block) && guard<100000)
        {
            QTextBlock prev=block.previous();

            if(!prev.isValid())
                return QString();

            if(codeMarkerLanguage(prev.text())>=0)
            {
                block=prev;
                break;
            }

            block=prev;
            ++guard;
        }

        if(!block.isValid() || codeMarkerLanguage(block.text())<0)
            return QString();
    }

    if(!block.isValid())
        return QString();

    QStringList lines;

    for(QTextBlock code=block.next(); code.isValid(); code=code.next())
    {
        if(!isCodeBlock(code))
            break;

        lines.append(code.text());
    }

    // Висячие пустые строки в конце в буфер не идут
    while(!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.takeLast();

    return lines.join(QStringLiteral("\n"));
}


// Курсор (или начало выделения) внутри блока кода с маркером
// и под маркером есть хоть одна непустая строка кода.
// Используется контекстным меню для показа пункта копирования
bool TypefaceFormatter::isCursorInCodeBlock(void)
{
    QTextCursor cursor=textArea->textCursor();

    const int pos=cursor.hasSelection() ? cursor.selectionStart() : cursor.position();

    QTextDocument *doc=textArea->document();

    QTextBlock block=doc->findBlock(pos);

    // Стоим прямо на маркере
    if(block.isValid() && codeMarkerLanguage(block.text())>=0)
    {
        for(QTextBlock code=block.next(); code.isValid(); code=code.next())
        {
            if(!isCodeBlock(code))
                break;

            if(!code.text().trimmed().isEmpty())
                return true;
        }

        return false;
    }

    // Поднимаемся по строкам кода до маркера
    int guard=0;
    while(block.isValid() && isCodeBlock(block) && guard<100000)
    {
        QTextBlock prev=block.previous();

        if(!prev.isValid())
            return false;

        if(codeMarkerLanguage(prev.text())>=0)
            return true;

        block=prev;
        ++guard;
    }

    return false;
}


// Тот же копипаст, но точка берется из текущего курсора
// (для контекстного меню: начало выделения или позиция)
void TypefaceFormatter::onCopyCodeFromCursor(void)
{
    QTextCursor cursor=textArea->textCursor();

    const int pos=cursor.hasSelection() ? cursor.selectionStart() : cursor.position();

    onCopyCodeClicked(pos);
}


// Клик по ссылке-кнопке [copy] в строке маркера: код блока ниже
// маркера уходит в буфер обмена
void TypefaceFormatter::onCopyCodeClicked(int docPosition)
{
    const QString code=collectCodeBelowMarker(docPosition);

    if(code.isEmpty())
        return;

    QApplication::clipboard()->setText(code);

    QToolTip::showText(QCursor::pos(), tr("Code copied to clipboard"));
}


// Запомненное оформление абзаца до первого превращения в заголовок:
// размер, жирность и отступы. Нужно для точного отката: снятие уровня
// возвращает свое, а не дефолт. Запоминается один раз: H1->H2 хранит
// исходное обычное оформление. Только стандартные свойства: все
// откатывается штатным undo и не оставляет мусора значений
struct HeadingStash : public QTextBlockUserData
{
    int stashedLevel=0;
    double pointSize=0.0;
    int weight=QFont::Normal;
    QVariant sizeAdjustment;
    double topMargin=0.0;
    double bottomMargin=0.0;
    bool hasData=false;
};


// Высота чернил строки: строки изображения с не-белыми пикселями.
// Нужна для замера масштабного коэффициента заголовков
static int inkedHeight(const QImage &image)
{
    int top=image.height();
    int bottom=-1;

    for(int y=0; y<image.height(); ++y)
        for(int x=0; x<image.width(); ++x)
            if(image.pixel(x, y)!=qRgb(255, 255, 255))
            {
                if(y<top)
                    top=y;
                if(y>bottom)
                    bottom=y;
            }

    if(bottom<top)
        return 0;

    return bottom-top+1;
}


// Масштабный коэффициент размера заголовка: во сколько раз h1/h2/h3
// крупнее обычного текста. Qt хранит крупность в свойстве
// FontSizeAdjustment, а оно не откатывается undo и плодит мусор,
// поэтому в работу берутся абсолютные размеры. Коэффициент замеряется
// один раз за сессию рендером эталона: допущений о внутренностях Qt нет
static double headingScaleFactor(int level)
{
    static double factors[4]={0.0, 0.0, 0.0, 0.0};
    static bool measured=false;

    if(!measured)
    {
        measured=true;
        factors[0]=1.0;

        auto renderTag=[](const QString &tag) -> int
        {
            QTextDocument probe;
            QFont baseFont;
            baseFont.setPointSize(100);
            probe.setDefaultFont(baseFont);
            probe.setHtml(tag);
            probe.setTextWidth(2000);

            QImage image(2000, 400, QImage::Format_RGB32);
            image.fill(Qt::white);
            QPainter painter(&image);
            probe.drawContents(&painter);
            painter.end();

            return inkedHeight(image);
        };

        const int plainHeight=renderTag(QStringLiteral("<p>Hg</p>"));
        if(plainHeight>0)
        {
            const int h1=renderTag(QStringLiteral("<h1>Hg</h1>"));
            const int h2=renderTag(QStringLiteral("<h2>Hg</h2>"));
            const int h3=renderTag(QStringLiteral("<h3>Hg</h3>"));
            if(h1>0)
                factors[1]=double(h1)/double(plainHeight);
            if(h2>0)
                factors[2]=double(h2)/double(plainHeight);
            if(h3>0)
                factors[3]=double(h3)/double(plainHeight);
        }

        for(int i=0; i<4; ++i)
            if(factors[i]<=0.0)
                factors[i]=1.0;

        qDebug() << "Heading scale factors H1/H2/H3:" << factors[1] << factors[2] << factors[3];
    }

    if(level<0 || level>3)
        return 1.0;

    return factors[level];
}


// Эталон блочных отступов уровня: снимается с временного документа
static void headingProbeMargins(int level, QTextBlockFormat &blockFormat)
{
    QTextDocument probe;
    if(level==1)
        probe.setHtml(QStringLiteral("<h1>x</h1>"));
    else if(level==2)
        probe.setHtml(QStringLiteral("<h2>x</h2>"));
    else if(level==3)
        probe.setHtml(QStringLiteral("<h3>x</h3>"));
    else
        probe.setHtml(QStringLiteral("<p>x</p>"));

    QTextBlock block=probe.begin();
    if(block.isValid())
    {
        blockFormat.setTopMargin(block.blockFormat().topMargin());
        blockFormat.setBottomMargin(block.blockFormat().bottomMargin());
    }
}


// Установка уровня заголовка для абзацев в выделении (или текущего
// абзаца без выделения). Повторный клик по своему уровню снимает
// его в обычный текст. Символьное форматирование не трогается:
// уровень заголовка живёт в блочном формате и в HTML едет как <h1>
void TypefaceFormatter::applyHeadingLevel(int level)
{
    QTextCursor cursor=textArea->textCursor();

    // Границы прохода по абзацам: без выделения — текущий абзац
    const int from=cursor.hasSelection() ? cursor.selectionStart() : cursor.position();
    const int to=cursor.hasSelection() ? cursor.selectionEnd() : cursor.position();

    // Тумблер: все затронутые абзацы уже этого уровня — снять в обычный
    bool allSame=true;

    for(QTextBlock block=textArea->document()->findBlock(from);
        block.isValid() && block.position()<=to;
        block=block.next())
    {
        if(block.blockFormat().headingLevel()!=level)
        {
            allSame=false;
            break;
        }
    }

    const int target=allSame ? 0 : level;

    // Эталон блочных отступов целевого уровня
    QTextBlockFormat probeBlockFormat;
    headingProbeMargins(target, probeBlockFormat);

    const double baseNormalSize=normalPointSize();

    cursor.beginEditBlock();

    // Разбить абзацы по внутристрочным разрывам строк (U+2028/U+2029
    // из вставок с веба) - но только когда заголовки ставятся, а не
    // снимаются. Иначе несколько зрительных строк остаются одним
    // блоком и уходят в один пункт оглавления. Справа налево, чтобы
    // позиции левее не ехали; курсор-выделение само едет за правками,
    // границы прохода перечитываются ниже
    int realFrom=from;
    int realTo=to;
    if(target>0)
    {
        QList<int> separators;
        for(QTextBlock block=textArea->document()->findBlock(from);
            block.isValid() && block.position()<=to;
            block=block.next())
        {
            const QString text=block.text();
            for(int i=0; i<text.length(); ++i)
                if(text[i]==QChar(0x2028) || text[i]==QChar(0x2029))
                    separators << block.position()+i;
        }

        for(int i=separators.size()-1; i>=0; --i)
        {
            QTextCursor splitCursor(textArea->document());
            splitCursor.setPosition(separators[i]);
            splitCursor.insertBlock();

            // Курсор встает в начало нового абзаца - прямо на
            // символ-разрыв: удаляется он сам, а не перенос
            // (удаление по старой позиции стирало перенос и
            // склеивало абзацы обратно)
            splitCursor.deleteChar();
        }

        realFrom=cursor.hasSelection() ? cursor.selectionStart() : cursor.position();
        realTo=cursor.hasSelection() ? cursor.selectionEnd() : cursor.position();
    }

    for(QTextBlock block=textArea->document()->findBlock(realFrom);
        block.isValid() && block.position()<=realTo;
        block=block.next())
    {
        const int oldLevel=block.blockFormat().headingLevel();

        HeadingStash *stash=static_cast<HeadingStash *>(block.userData());

        // Обычный абзац без stash трогать нечем: снимать нечего,
        // иначе сброс в обычный текст затирал бы размер начертания
        if(target==0 && oldLevel==0 && (stash==nullptr || !stash->hasData))
            continue;

        // Пустые абзацы заголовками не делаются: иначе плодятся
        // пустые H1/H2/H3. Снятие уровня с пустых работает как обычно
        if(target>0 && block.text().simplified().isEmpty())
            continue;

        // В заголовок: исходное запомнить один раз для точного отката.
        // Незаданный размер резолвится в базовый сразу: дальше везде
        // только явные значения, никаких снятий и нулей
        if(target>0 && stash==nullptr)
        {
            stash=new HeadingStash();
            QTextBlock::iterator it=block.begin();
            if(!it.atEnd())
            {
                const QTextCharFormat src=it.fragment().charFormat();
                stash->pointSize=src.fontPointSize()>0.0 ? src.fontPointSize() : baseNormalSize;
                stash->weight=src.fontWeight();
                stash->sizeAdjustment=src.property(QTextFormat::FontSizeAdjustment);
            }
            else
            {
                stash->pointSize=baseNormalSize;
            }
            stash->topMargin=block.blockFormat().topMargin();
            stash->bottomMargin=block.blockFormat().bottomMargin();
            stash->stashedLevel=oldLevel;
            stash->hasData=true;
            block.setUserData(stash);
        }

        QTextBlockFormat format=block.blockFormat();
        format.setHeadingLevel(target);
        format.setTopMargin(probeBlockFormat.topMargin());
        format.setBottomMargin(probeBlockFormat.bottomMargin());

        QTextCursor edit(block);
        edit.setBlockFormat(format);

        // Снапшот фрагментов абзаца: позиции и форматы. Применяется
        // вторым проходом, так как setCharFormat дробит фрагменты
        // и инвалидирует итератор. Позиции не едут: длина не меняется
        struct FragmentSnapshot
        {
            int position;
            int length;
            QTextCharFormat format;
        };
        QList<FragmentSnapshot> snapshots;
        for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
        {
            FragmentSnapshot snap;
            snap.position=it.fragment().position();
            snap.length=it.fragment().length();
            snap.format=it.fragment().charFormat();
            snapshots << snap;
        }

        // Размер заголовка - абсолютный: масштаб от базового. Свойство
        // FontSizeAdjustment не трогается выставлением, только сбросом
        // в нейтральный ноль: оно не откатывается undo и плодит мусор
        if(target>0)
        {
            double baseSize=baseNormalSize;
            if(stash!=nullptr && stash->hasData && stash->stashedLevel==0 && stash->pointSize>0.0)
                baseSize=stash->pointSize;
            else if(!snapshots.isEmpty() && snapshots.first().format.fontPointSize()>0.0)
                baseSize=snapshots.first().format.fontPointSize();

            const double absoluteSize=baseSize*headingScaleFactor(target);

            for(const FragmentSnapshot &snap : snapshots)
            {
                QTextCursor fragmentCursor(textArea->document());
                fragmentCursor.setPosition(snap.position);
                fragmentCursor.setPosition(snap.position+snap.length, QTextCursor::KeepAnchor);

                // Только merge явными значениями: судьба каждого
                // свойства предсказуема (ничего лишнего не задевается),
                // а полный setCharFormat на выделении в этой версии Qt
                // замечен в непересчете вида. Ступень FontSizeAdjustment
                // только снимается где есть: само ее присутствие
                // переключает раскладку на свой путь и глушит явный
                // размер, а выставление не откатывается undo.
                // Свежим фрагментам она не ставится
                QTextCharFormat fragmentFormat;
                fragmentFormat.setFontPointSize(absoluteSize);
                fragmentFormat.setFontWeight(QFont::Bold);
                if(snap.format.hasProperty(QTextFormat::FontSizeAdjustment))
                    fragmentFormat.clearProperty(QTextFormat::FontSizeAdjustment);
                fragmentCursor.mergeCharFormat(fragmentFormat);
            }
        }
        else
        {
            // Назад в обычный. Был обычным - вернуть свое из stash,
            // был загруженным заголовком - нормализовать: свой размер
            // (если задан), обычная жирность, ступень снимается.
            // Везде только merge явными значениями: снятий свойств,
            // кроме чужой ступени, нет, нейтральный ноль не пишется
            if(stash!=nullptr && stash->hasData && stash->stashedLevel==0)
            {
                format.setTopMargin(stash->topMargin);
                format.setBottomMargin(stash->bottomMargin);
                edit.setBlockFormat(format);

                for(const FragmentSnapshot &snap : snapshots)
                {
                    QTextCursor fragmentCursor(textArea->document());
                    fragmentCursor.setPosition(snap.position);
                    fragmentCursor.setPosition(snap.position+snap.length, QTextCursor::KeepAnchor);

                    QTextCharFormat fragmentFormat;
                    fragmentFormat.setFontPointSize(stash->pointSize);
                    fragmentFormat.setFontWeight(stash->weight);
                    if(stash->sizeAdjustment.isValid())
                        fragmentFormat.setProperty(QTextFormat::FontSizeAdjustment, stash->sizeAdjustment);
                    else
                        fragmentFormat.clearProperty(QTextFormat::FontSizeAdjustment);
                    fragmentCursor.mergeCharFormat(fragmentFormat);
                }
            }
            else
            {
                for(const FragmentSnapshot &snap : snapshots)
                {
                    QTextCursor fragmentCursor(textArea->document());
                    fragmentCursor.setPosition(snap.position);
                    fragmentCursor.setPosition(snap.position+snap.length, QTextCursor::KeepAnchor);

                    QTextCharFormat fragmentFormat;
                    const double fragSize=snap.format.fontPointSize();
                    fragmentFormat.setFontPointSize(fragSize>0.0 ? fragSize : baseNormalSize);
                    fragmentFormat.setFontWeight(QFont::Normal);
                    fragmentFormat.clearProperty(QTextFormat::FontSizeAdjustment);
                    fragmentCursor.mergeCharFormat(fragmentFormat);
                }
            }

            block.setUserData(nullptr);
        }
    }

    cursor.endEditBlock();

    // Подсветка актуального уровня в меню заголовков и на кнопках
    // обновляется сразу: курсор при смене уровня не двигается,
    // без этого галка висела бы до первого движения курсора
    emit updateOutlineButtonHiglight();
}


void TypefaceFormatter::onHeadingClicked(int index)
{
    // Пункты меню: 0-H1, 1-H2, 2-H3, 3-обычный текст
    if(index==0)
        applyHeadingLevel(1);
    else if(index==1)
        applyHeadingLevel(2);
    else if(index==2)
        applyHeadingLevel(3);
    else if(index==3)
        applyHeadingLevel(0);
}


double TypefaceFormatter::normalPointSize(void) const
{
    double size=textArea->document()->defaultFont().pointSizeF();
    if(!(size>0.0))
        size=QApplication::font().pointSizeF();
    if(!(size>0.0))
        size=10.0;

    return size;
}


// Enter в заголовке. В конце непустого заголовка новый
// абзац сразу обычный (иначе каждый Enter плодит пустые H1/H2/H3),
// на пустом заголовке уровень снимается на месте без новой строки.
// Выделение строго внутри одного заголовка вырезается как штатно,
// затем работают те же правила. В середине абзаца, через абзацы
// и вне заголовков - false, работает штатный путь
bool TypefaceFormatter::handleHeadingEnter(void)
{
    if(textArea->isReadOnly())
        return false;

    QTextCursor cursor=textArea->textCursor();

    bool eraseSelection=false;
    if(cursor.hasSelection())
    {
        // После выбора уровня из меню выделение обычно остается,
        // и Enter с ним уходил в штатный путь с наследованием уровня.
        // Выделение строго внутри одного заголовка вырезается здесь,
        // через абзацы - штатный путь
        QTextBlock startBlock=textArea->document()->findBlock(cursor.selectionStart());
        QTextBlock endBlock=textArea->document()->findBlock(cursor.selectionEnd());
        if(startBlock!=endBlock || startBlock.blockFormat().headingLevel()<1)
            return false;

        eraseSelection=true;
    }
    else if(cursor.block().blockFormat().headingLevel()<1)
        return false;

    QTextBlockFormat probeBlockFormat;
    headingProbeMargins(0, probeBlockFormat);

    const double baseSize=normalPointSize();

    cursor.beginEditBlock();

    if(eraseSelection)
        cursor.removeSelectedText();

    QTextBlock block=cursor.block();

    bool consumed=false;
    if(block.text().isEmpty())
    {
        // Пустой заголовок: уровень и вид снимаются на месте,
        // новой строки нет. Метка стирается: абзац снова обычный
        QTextBlockFormat format=block.blockFormat();
        format.setHeadingLevel(0);
        format.setTopMargin(probeBlockFormat.topMargin());
        format.setBottomMargin(probeBlockFormat.bottomMargin());

        QTextCursor edit(block);
        edit.setBlockFormat(format);
        block.setUserData(nullptr);

        textArea->setTextCursor(edit);
        QTextCharFormat current=textArea->currentCharFormat();
        current.setFontPointSize(baseSize);
        current.setFontWeight(QFont::Normal);
        current.clearProperty(QTextFormat::FontSizeAdjustment);
        textArea->setCurrentCharFormat(current);
        consumed=true;
    }
    else if(cursor.atBlockEnd())
    {
        // В конце заголовка: штатный сплит, затем новый абзац
        // приводится к обычному виду. В середине абзаца сплит
        // штатный: обе половины остаются заголовком
        cursor.insertBlock();

        QTextBlock newBlock=cursor.block();
        QTextBlockFormat format=newBlock.blockFormat();
        format.setHeadingLevel(0);
        format.setTopMargin(probeBlockFormat.topMargin());
        format.setBottomMargin(probeBlockFormat.bottomMargin());

        QTextCursor edit(newBlock);
        edit.setBlockFormat(format);

        textArea->setTextCursor(cursor);
        QTextCharFormat current=textArea->currentCharFormat();
        current.setFontPointSize(baseSize);
        current.setFontWeight(QFont::Normal);
        current.clearProperty(QTextFormat::FontSizeAdjustment);
        textArea->setCurrentCharFormat(current);
        consumed=true;
    }

    cursor.endEditBlock();

    return consumed;
}


// Префикс ссылок-переходов оглавления. Дальше через слэш:
// уровень, порядковый номер заголовка, кодированный текст.
// Номер - запасной ключ, если текст заголовка поменяли после вставки
QString TypefaceFormatter::tocGoPrefix(void)
{
    return QStringLiteral("mytetra://toc-go/");
}


QList<TypefaceFormatter::TocEntry> TypefaceFormatter::collectTocEntries(void) const
{
    QList<TocEntry> entries;

    for(QTextBlock block=textArea->document()->begin(); block.isValid(); block=block.next())
    {
        const int level=block.blockFormat().headingLevel();
        if(level<1 || level>3)
            continue;

        QString text=block.text().simplified();
        if(text.isEmpty())
            continue;
        if(text.length()>120)
            text=text.left(120)+QStringLiteral("…");

        TocEntry entry;
        entry.level=level;
        entry.text=text;
        entry.position=block.position();
        entries << entry;
    }

    return entries;
}


bool TypefaceFormatter::blockHasTocLink(const QTextBlock &block) const
{
    if(!block.isValid())
        return false;

    for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
        if(it.fragment().charFormat().anchorHref().startsWith(tocGoPrefix()))
            return true;

    return false;
}


// Вставка снимка оглавления: каждый заголовок - абзац с отступом
// уровня и ссылкой-переходом. Курсор под оглавлением или внутри него:
// соседние абзацы-ссылки вырезаются и пишутся заново (обновление
// повторным хоткеем). Иначе оглавление вставляется в курсор
void TypefaceFormatter::onTocClicked(void)
{
    const QList<TocEntry> entries=collectTocEntries();
    if(entries.isEmpty())
        return;

    QTextCursor cursor=textArea->textCursor();

    // Границы оглавления под курсором. Курсор после вставки стоит
    // на первой строке оглавления, поэтому без ссылки в текущем
    // абзаце проверяется еще и предыдущий
    QTextBlock first=cursor.block();
    if(!blockHasTocLink(first) &&
       first.previous().isValid() && blockHasTocLink(first.previous()))
        first=first.previous();

    bool haveToc=blockHasTocLink(first);
    QTextBlock last=first;
    if(haveToc)
    {
        while(first.previous().isValid() && blockHasTocLink(first.previous()))
            first=first.previous();
        while(last.next().isValid() && blockHasTocLink(last.next()))
            last=last.next();
    }

    QTextCursor edit(textArea->document());
    edit.beginEditBlock();

    if(haveToc)
    {
        // Старое оглавление вырезается целиком, дальше пишется
        // в оставшийся пустой абзац. Конец выделения - именно конец
        // текста последнего абзаца (EndOfBlock), его перенос строки
        // остается: иначе строка оглавления склеится со следующим.
        // Позиции - целыми числами до правок: хендлы QTextBlock после
        // removeSelectedText протухают и врут про position()
        const int firstPos=first.position();
        edit.setPosition(firstPos);
        edit.setPosition(last.position(), QTextCursor::KeepAnchor);
        edit.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        edit.removeSelectedText();
        edit.setPosition(firstPos);
    }
    else
    {
        // Новое оглавление пишется строго в пустой абзац, текст
        // существующих абзацев не разрезается и не затирается
        edit.setPosition(cursor.position());
        if(edit.block().text().isEmpty())
        {
            // Уже пустой абзац: пишем прямо сюда
        }
        else if(edit.atBlockStart())
        {
            // В начале абзаца: пустой абзац появляется перед ним
            edit.insertBlock();
            edit.movePosition(QTextCursor::PreviousBlock);
        }
        else
        {
            // Внутри или в конце абзаца: пустой абзац появляется после него
            edit.movePosition(QTextCursor::EndOfBlock);
            edit.insertBlock();
        }
    }

    const int tocStart=edit.position();

    for(int i=0; i<entries.size(); ++i)
    {
        if(i>0)
            edit.insertBlock();

        QTextBlockFormat blockFormat;
        blockFormat.setIndent(entries[i].level-1);
        edit.setBlockFormat(blockFormat);

        edit.setCharFormat(QTextCharFormat());

        const QString href=tocGoPrefix()
            +QString::number(entries[i].level)+QStringLiteral("/")
            +QString::number(i)+QStringLiteral("/")
            +QString::fromLatin1(QUrl::toPercentEncoding(entries[i].text));

        edit.insertText(entries[i].text);

        // Вставленный текст оформляется ссылкой-переходом
        QTextCursor linkCursor(edit.document());
        const int linkEnd=edit.position();
        linkCursor.setPosition(linkEnd-entries[i].text.length());
        linkCursor.setPosition(linkEnd, QTextCursor::KeepAnchor);

        QTextCharFormat linkFormat;
        linkFormat.setAnchor(true);
        linkFormat.setAnchorHref(href);
        linkFormat.setFontUnderline(true);
        linkCursor.mergeCharFormat(linkFormat);
    }

    edit.endEditBlock();

    // Курсор - в начало оглавления: повторный хоткей обновит на месте
    QTextCursor result(textArea->document());
    result.setPosition(tocStart);
    textArea->setTextCursor(result);
}


// Клик по пункту оглавления: прыжок в раздел. Сначала ищется
// заголовок с тем же уровнем и текстом (пережил правки вокруг),
// запасной вариант - тот же порядковый номер того же уровня.
// Ничего не нашлось - молча игнорируется (оглавление устарело)
void TypefaceFormatter::onTocGoClicked(const QString &href)
{
    if(!href.startsWith(tocGoPrefix()))
        return;

    const QStringList parts=href.mid(tocGoPrefix().length()).split(QStringLiteral("/"));
    if(parts.size()<3)
        return;

    bool okLevel=false;
    bool okIndex=false;
    const int level=parts[0].toInt(&okLevel);
    const int index=parts[1].toInt(&okIndex);
    const QString text=QString::fromUtf8(QByteArray::fromPercentEncoding(parts[2].toLatin1()));
    if(!okLevel || !okIndex)
        return;

    const QList<TocEntry> entries=collectTocEntries();

    int target=-1;
    for(int i=0; i<entries.size(); ++i)
        if(entries[i].level==level && entries[i].text==text)
        {
            target=i;
            break;
        }
    if(target<0)
        for(int i=0; i<entries.size(); ++i)
            if(entries[i].level==level && i==index)
            {
                target=i;
                break;
            }
    if(target<0)
        return;

    QTextCursor cursor(textArea->document());
    cursor.setPosition(entries[target].position);
    textArea->setTextCursor(cursor);
    textArea->ensureCursorVisible();
}


// Очистка форматирования, т.е. установка стандартного шрифта,
// размера и убирание утолщения, наклона, подчеркивания
// Картинки и таблицы не удаляются
void TypefaceFormatter::onClearClicked(void)
{
    // TRACELOG

    int startCursorPos=textArea->textCursor().anchor(); // Начало выделения
    int stopCursorPos=textArea->textCursor().position(); // Конец выделения
    qDebug() << "Cursor start position: " << startCursorPos << "Cursor stop position: " << stopCursorPos;

    // Если выделение было сзаду-наперед, надо поменять начальную и конечную позицию местами
    bool isSelectionReverse=false;
    if(startCursorPos>stopCursorPos)
    {
        // Меняется только начальная позиция. Конечная позиция далее не используется,
        // так как длина выделенного фрагмента будет вычисляться специальным образом
        // (будет удаляться выделенная часть и вставляться новый очищенный текст,
        // а его длина будет отличаться от длины начального выделения)
        startCursorPos=stopCursorPos;
        isSelectionReverse=true;
    }
    // qDebug() << "Cursor start position: " << startCursorPos << "Cursor stop position: " << stopCursorPos;

    bool flag_cursor_on_empty_line=editor->cursorPositionDetector->isCursorOnEmptyLine();
    bool flag_cursor_on_space_line=editor->cursorPositionDetector->isCursorOnSpaceLine();

    // Очистка возможна только если что-то выделено
    // Или курсор стоит на пустой строке с одним символом перевода строки
    // Или курсор стоит на строке, в которой нет текста (одни пробелы)
    if(!(textArea->textCursor().hasSelection() ||
         flag_cursor_on_empty_line ||
         flag_cursor_on_space_line))
        return;


    // Запоминается положение прокрутки окна редактирования.
    // Это нужно, чтобы вернуться к такому положению после всех действий.
    // Иначе прокрутка перепрыгнет наверх документа
    int scrollBarPosition=editor->getScrollBarPosition();


    // Начало изменения текста
    textArea->textCursor().beginEditBlock();


    // Если выбран НЕ четко блок, и есть форматирование списка
    if(editor->cursorPositionDetector->isBlockSelect()==false &&
            textArea->textCursor().currentList()!=nullptr)
    {
        // Выделение части строки в списке нельзя очищать обычным способом, так как появится перенос строки
        clearSimple(); // Запускается упрощенная очистка
    }
    else
    {
        // Если курсор стоит на строке, в которой нет текста (одни пробелы) - нужно выделить эту строку
        if(flag_cursor_on_space_line)
            (textArea->textCursor()).select(QTextCursor::LineUnderCursor);

        // К выделению применяется стандартный шрифт
        applyStandartFontForSelection();

        // Очищается цвет заднего фона - теперь не нужно, так как очистка происходит путем полного удаления атрибутов стиля
        // clearBackgroundColorForSelection();

        // *****************************
        // Установка начертания символов
        // *****************************

        // Получение HTML-кода очищенного фрагмента
        QString htmlCode=clearTypeFace( replaceSpacesOnlyTags( textArea->textCursor().selection().toHtml() ) );

        // Замена в HTML-коде пробелов на неразывные пробелы, иначе все повторяющиеся пробелы будут удален Qt-движком
        htmlCode=replaceSpaces(htmlCode);
        // qDebug() << "After replace spaces" << htmlSimplyfier( htmlCode );

        // Замена в HTML-коде табуляции на спецкоды, иначе символы табуляции будут удален Qt-движком
        // Проблема осталась: спецкоды тоже удаляются в момент insertHtml()
        // htmlCode=replaceTabs(htmlCode);
        // qDebug() << "After replace tabs" << htmlSimplyfier( htmlCode );

        // Удаление выделенного фрагмента
        textArea->textCursor().removeSelectedText();
        // qDebug() << "After remove selected text: "<< htmlSimplyfier( textArea->toHtml() );

        // С помощью дополнительного курсора выясняется последняя позиция в тексте, в котором удален выделенный фрагмент
        QTextCursor cursor=textArea->textCursor();
        cursor.movePosition(QTextCursor::End);
        int afterRemoveSelectionLen=cursor.position();
        // qDebug() << "After remove selection length: " << afterRemoveSelectionLen;

        // Вставка очищенного фрагмента
        textArea->textCursor().insertHtml(htmlCode);
        // qDebug() << "After insert HTML: "<< htmlSimplyfier( textArea->toHtml() );

        // С помощью дополнительного курсора выясняется последняя позиция в тексте, в котором вставлен очищенный фрагмент
        cursor.movePosition(QTextCursor::End);
        int afterClearLen=cursor.position();
        // qDebug() << "After clear length: " << afterClearLen;

        // Вычисляется последняя позиция выделения очищенного текста
        int calculateEndCursorPos=startCursorPos + (afterClearLen - afterRemoveSelectionLen);
        // qDebug() << "Calculate end cursor pos: " << calculateEndCursorPos;

        // Удаление самопроизвольно вставляемых Qt концевых пробелов в каждой текстовой ноде (компенсация поведения insertHtml())
        calculateEndCursorPos=removeSpaces(startCursorPos, calculateEndCursorPos);

        // Замена заранее внесенных символов ReplacementCharacter на пробелы (компенсация поведения insertHtml())
        replaceReplacementCharacterToSpaceInSelectedText(startCursorPos, calculateEndCursorPos);

        // Замена самопроизвольно вставляемых Qt концевых пробелов (последовательность пробел+QChar::ParagraphSeparator на QChar::ParagraphSeparator)
        // calculateEndCursorPos=replaceSpaceAndParagraphSeparatorToParagraphSeparator(startCursorPos, calculateEndCursorPos);

        // ********************************
        // Выделение вставленного фрагмента
        // Если его не сделать, то первая строка получит дополнительные вертикальные отступы. Это особенность Qt
        // ********************************
        if(!isSelectionReverse)
        {
            cursor.setPosition(startCursorPos, QTextCursor::MoveAnchor);
            cursor.setPosition(calculateEndCursorPos, QTextCursor::KeepAnchor);
        }
        else
        {
            cursor.setPosition(calculateEndCursorPos, QTextCursor::MoveAnchor);
            cursor.setPosition(startCursorPos, QTextCursor::KeepAnchor);
        }
        textArea->setTextCursor(cursor);
        // qDebug() << "Select text after insert HTML: " << htmlSimplyfier( textArea->textCursor().selection().toHtml() );


        // Установка форматирования абзаца
        // Если выделен блок
        // или курсор на пустой линии
        // или курсор на линии на которой нет символов
        if(editor->cursorPositionDetector->isBlockSelect() ||
                flag_cursor_on_empty_line ||
                flag_cursor_on_space_line)
            applyPureBlockFormatForSelection();

        // Если была работа со строкой, в которой нет символов,
        // курсор переносится на начало строки, чтобы не путать пользователя
        if(flag_cursor_on_space_line)
            textArea->moveCursor(QTextCursor::StartOfLine);
    }

    // Завершение изменения текста
    textArea->textCursor().endEditBlock();

    editor->setScrollBarPosition(scrollBarPosition);

    // Вызывается метод, как будто переместился курсор с выделением, чтобы
    // обновились состояния подсветок кнопок форматирования
    editor->onSelectionChanged();

    editor->updateIndentsliderToActualFormat();

    emit updateOutlineButtonHiglight();
}


void TypefaceFormatter::applyStandartFontForSelection()
{
    // Создается стандартный шрифт
    QFont font;
    font.fromString( editorConfig->get_default_font() ); // Стандартное начертание взятое из конфига
    font.setPointSize( editorConfig->get_default_font_size() ); // Стандартный размер взятый из конфига

    // К выделенному тексту применяется стандартный шрифт
    textArea->setCurrentFont(font);

    // Новый установленный шрифт показывается в выпадающем списке шрифтов
    emit changeFontselectOnDisplay(font.family());

    // В выпадающем списке размеров выставляется установленный размер
    emit changeFontsizeOnDisplay(editorConfig->get_default_font_size());
}


void TypefaceFormatter::applyPureBlockFormatForSelection()
{
    // qDebug() << "Set default text format";

    QTextBlockFormat format;

    // Убираются отступы
    format.setLeftMargin(0);   // Убирается левый отступ (который, возможно был установлен слайдером или кнопками изменения отступа)
    format.setRightMargin(0);
    format.setTopMargin(0);    // Убираются межстрочные интервалы, которые самопроизвольно появляются при вставке из других программ
    format.setBottomMargin(0);
    format.setAlignment(Qt::AlignLeft); // Выравнивание по левому краю

    // Применение форматирование
    textArea->textCursor().setBlockFormat(format);

    // qDebug() << "Select text after apply format: " << textArea->textCursor().selection().toHtml();
}


void TypefaceFormatter::removeAnchorDataForSelection()
{
    QTextCharFormat format;
    format.setAnchor(false);
    format.setAnchorHref("");

    // Применение форматирование
    textArea->textCursor().mergeCharFormat(format);
}


void TypefaceFormatter::clearColorForSelection()
{
    QColor clearColor;
    QBrush clearBrush( clearColor );

    QTextCharFormat format;

    // Вариант 1. QColor color=qApp->palette.text().color();
    // Вариант 2. qApp->palette("EditorTextArea").text() - так берется Brush
    // из CSS-псевдоправила CustomPalette, но оно не подходит, потому что
    // это правило для всего интерфейса, но область редактирования текста
    // в некоторых темах может быть не инверсной как весь остальной интерфейс
    format.setForeground( clearBrush );

    // Применение форматирование
    textArea->textCursor().mergeCharFormat(format);
}


// В настоящий момент этот метод не нужен, так как очистка происходит в стилях (через полное удаление атрибуторв стиля)
void TypefaceFormatter::clearBackgroundColorForSelection()
{
    // Сбрасывается цвет заднего фона
    QColor clearColor;
    QBrush clearBrush( clearColor );
    QTextCharFormat clearCharFormat;
    clearCharFormat.setForeground( clearBrush );
    textArea->textCursor().mergeCharFormat(clearCharFormat);
}


// Замена пробелов внутри тегов <span>, содержимое которых состоит из одних только пробелов
QString TypefaceFormatter::replaceSpacesOnlyTags(QString htmlCode)
{
    qDebug() << "In TypefaceFormatter::replaceSpacesOnlyTags(): " << htmlSimplyfier( htmlCode );

    QRegExp replaceSpaceTagsEx("<span[^>]*>\\s*</span>");
    replaceSpaceTagsEx.setMinimal(true);

    // Поиск тегов <span> с одними пробелами внутри, с запоминанием их
    QStringList list;
    int lastPos = 0;
    while( ( lastPos = replaceSpaceTagsEx.indexIn( htmlCode, lastPos ) ) != -1 )
    {
        lastPos += replaceSpaceTagsEx.matchedLength();

        list << replaceSpaceTagsEx.cap(0);
        // qDebug() << "Find space only sttring: " << list.last();
    }


    // Замена найденных тегов на теги с RC-символом вместо пробелов
    for( int n = 0; n < list.size(); n++)
    {
        QString line=list.at(n);

        // Здесь однозначно известно, что строка имеет вид "<span атрибуты>пробелы</span>"
        int spaceFrom=line.indexOf(">")+1;
        int spaceTo=line.indexOf("<", spaceFrom)-1;
        int spaceLen=spaceTo-spaceFrom+1;
        // qDebug() << "Space length:" << spaceLen;

        QString spaceLine;
        for(int i=0; i<spaceLen; ++i)
            spaceLine.append(QChar::ReplacementCharacter); // spaceLine+="&#65533;" - это была вставка как HTML-код, она работала, но лучше сразу вставлять Unicode символ

        QString replaceLineLeft=line.mid(0, spaceFrom);
        QString replaceLineRight=line.mid(spaceTo+1);
        QString replaceLine=replaceLineLeft+spaceLine+replaceLineRight;

        // qDebug() << "Replace space line from: " << list.at(n);
        // qDebug() << "Replace space line to  : " << replaceLine;
        htmlCode.replace( list.at(n), replaceLine);
    }

    return htmlCode;
}


// Очистка начертания символов
QString TypefaceFormatter::clearTypeFace(QString htmlCode)
{
    // Удаление какого-либо форматирования стилем
    // qDebug() << "Before clearTypeFace apply: " << htmlCode;

    // В регулярных выражениях Qt кванторы по-умолчанию жадные (greedy)
    // Поэтому напрямую регвыру указывается что кванторы должны быть ленивые
    QRegExp removeStyleEx("style=\".*\"");
    removeStyleEx.setMinimal(true);
    htmlCode.replace(removeStyleEx, "style=\"margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px;\"");
    // qDebug() << "After remove style: " << htmlCode;

    // Тег <pre> превращается в <p>, так как последующая вставка <pre>-тегов через insertHtml() автоматически добавляет системный моноширинный стиль
    htmlCode.replace("<pre ", "<p ");
    htmlCode.replace("</pre>", "</p>");

    htmlCode.replace("<!--StartFragment-->", "");
    // qDebug() << "After replace StartFragment: " << htmlSimplyfier( htmlCode );

    // Просто так удалять <!--EndFragment--> нельзя.
    // Нужно вначале удалить p-тег вида: <p style="..."><br /><!--EndFragment--></p>
    // Так как такая конструкция свидетельствует, что в конце выделения курсор находится на строке,
    // на которой ничего не выделено (т. е. находится в самой первой позиции строки).
    // Таким образом данный p-тег будет лишним, и будет преобразовываться в отдельную пустую строку,
    // если его сразу не убрать
    // Код лишний, но оставляю на будущее
    /*
    QRegExp removeEmptyFinishPTag("<p style=\"[^\"]*\"><br.?\\/><!--EndFragment--><\\/p>");
    removeEmptyFinishPTag.setMinimal(true);
    htmlCode.replace(removeEmptyFinishPTag, "<!--EndFragment-->");
    qDebug() << "After replace special tag EndFragment: " << htmlSimplyfier( htmlCode );
    */

    // Теперь однозначно удаляется признак конца фрагмента, на тот случай, если предыдущая замена не сработала
    htmlCode.replace("<!--EndFragment-->", "");
    // qDebug() << "After replace EndFragment: " << htmlCode;


    // Замена конструкции <p ...><br /></p>, которая вставляется автоматически Qt в конец HTML текста,
    // и тем самым создает лишнюю пустую строку
    // Жадная регулярка не всегда корректно захватывает строку (почему-то работает как ленивая), приходится разбивать на подстроки
    // Кроме того, в Qt нет возможности переключать режим multiline/не-multiline в регулярных выражениях
    QStringList list=htmlCode.split(QRegularExpression("\\n"));
    QString tempHtmlCode;
    for(int lineNum=0; lineNum<list.count(); ++lineNum)
    {
        // qDebug() << "L" << lineNum << " " << list[lineNum];

        QRegExp replacePBrP("<p.*><br.?\\/><\\/p.*>");
        replacePBrP.setMinimal(true);
        list[lineNum].replace(replacePBrP, "<p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; \"></p>");
        // qDebug() << "L" << lineNum << " " << list[lineNum];

        tempHtmlCode+=list[lineNum]+"\n";
    }
    htmlCode=tempHtmlCode;
    // qDebug() << "After replace p br p: " << htmlCode;

    QRegExp replaceOpenHeaderEx("<[hH]\\d.*>");
    replaceOpenHeaderEx.setMinimal(true);
    htmlCode.replace(replaceOpenHeaderEx, "<p>");

    QRegExp replaceCloseHeaderEx("</[hH]\\d.*>");
    replaceCloseHeaderEx.setMinimal(true);
    htmlCode.replace(replaceCloseHeaderEx, "</p>");

    QStringList chunks=htmlCode.split("<body>"); // Вместо удаления ".*<body>" через медленную регулярку
    if(chunks.length()==2)
        htmlCode=chunks.at(1);

    chunks=htmlCode.split("</body>"); // Вместо удаления "</body>.*" через медленную регулярку
    if(chunks.length()==2)
        htmlCode=chunks.at(0);

    return htmlCode;
}


// Метод, применяемый при выводе отладочной информации, чтобы проще было смотреть на код
QString TypefaceFormatter::htmlSimplyfier(QString htmlCode)
{
    QRegExp rx("style=\"([^\"]*)\"");
    rx.setMinimal(true);
    htmlCode.replace(rx, "");
    // qDebug() << "After replace htmlSimplyfier: " << htmlCode;

    return htmlCode;
}


// Замена в HTML-коде пробелов на спец-последовательности, иначе все повторяющиеся пробелы будут удален Qt-движком
QString TypefaceFormatter::replaceSpaces(QString htmlCode)
{
    QDomDocument doc;
    bool isDocParse=doc.setContent("<root>"+htmlCode+"</root>");

    if(!isDocParse)
    {
        // Вывод сообщения: Слишком сложное форматирование текста. Не могу разобрать выделенный текст. Возможно некоторое нарушение верстки
        showMessageBox(tr("Unreliable parsing of complexly formatted text. Expect formatting inconsistencies."));
        return htmlCode;
    }

    // Рекурсивная правка текстовых узлов, начиная с корневого элемента
    recurseReplaceSpaces(doc.documentElement());

    // Особенность Qt. Преобразование DOM-XML портит символ амперсанда. Поэтому он восстанавливается
    htmlCode=doc.toString(0).replace("&amp;#65533;", "&#65533;");

    htmlCode.replace("<root>", "");
    htmlCode.replace("</root>", "");

    return htmlCode;
}


void TypefaceFormatter::recurseReplaceSpaces(const QDomNode &node)
{
    QDomNode domNode = node.firstChild();

    // Если текущий узел существует
    while(!(domNode.isNull()))
    {
        if(domNode.isText()) // Если текущий узел - это текст
        {
            QDomText domText = domNode.toText();
            if(!domText.isNull())
            {
                QString text=domText.data();
                // qDebug() << "Found text node: " << text;

                // Чтобы не смыкаликись повторяющиеся пробелы, они временно заменяются на RC
                text.replace(" ", "&#65533;"); // REPLACEMENT CHARACTER

                // В узле устанавливается новая строка
                domNode.setNodeValue(text);
            }
        }

        recurseReplaceSpaces(domNode);
        domNode = domNode.nextSibling();
    }
}


// Замена заранее внесенных символов ReplacementCharacter на пробелы
// Если предварительно не заменять пробелы на ReplacementCharacter,
// то пробелы исчезнут, ибо Qt самопроизвольно удаляет ведущие пробелы при вызове textArea->textCursor().insertHtml(htmlCode);
void TypefaceFormatter::replaceReplacementCharacterToSpaceInSelectedText(int startCursorPos, int endCursorPos)
{
    QTextCursor replacementCursor=textArea->textCursor();

    for(int pos=startCursorPos; pos<endCursorPos; pos++)
    {
        // Выделяется один символ
        replacementCursor.setPosition(pos, QTextCursor::MoveAnchor);
        replacementCursor.setPosition(pos+1, QTextCursor::KeepAnchor);

        // Если выделение символа прошло успешно
        if(replacementCursor.selectedText().length()>0)
        {
            QChar currentChar=replacementCursor.selectedText().at(0);
            // qDebug() << "Pos: " << pos << " Char: " << currentChar << " Char code: " << currentChar.unicode();

            if(currentChar==QChar::ReplacementCharacter)
            {
                replacementCursor.insertText(" ");
                // qDebug() << "Replace RC to space in position: " << pos;
            }
        }
    }
}


// Удаление пробелов происходит из предположения о том, во вставляем тексте вообще не должно быть пробелов,
// т. к. все пробелы были предварительно заменены на RC-символ
int TypefaceFormatter::removeSpaces(int startCursorPos, int calculateEndCursorPos)
{
    // qDebug() << "Replace spaces from pos" << startCursorPos << "to pos" << calculateEndCursorPos;

    QTextCursor replacementCursor=textArea->textCursor();

    for(int pos=startCursorPos; pos<calculateEndCursorPos; pos++)
    {
        // Выделяется один символ
        replacementCursor.setPosition(pos, QTextCursor::MoveAnchor);
        replacementCursor.setPosition(pos+1, QTextCursor::KeepAnchor);

        // Если выделение символа прошло успешно
        if(replacementCursor.selectedText().length()>0)
        {
            QChar currentChar=replacementCursor.selectedText().at(0);

            if(currentChar==QChar::Space)
            {
                // qDebug() << "Find space in pos" << pos;

                // Удаляется пробел
                replacementCursor.deleteChar();

                // Номер текущего символа уменьшается на единицу, чтобы не пропустить следующий символ
                --pos;

                // Так как символ удален, длина всего вставляемого фрагмента уменьшается на единицу
                --calculateEndCursorPos;

            }
        }
    }

    return calculateEndCursorPos; // Возвращается последняя позиция в выделении, так как количество символов в выделении изменилось
}


QString TypefaceFormatter::replaceTabs(QString htmlCode)
{
    htmlCode.replace(QChar::Tabulation, "&#9;");
    return htmlCode;
}


// Замена самопроизвольно вставляемых Qt концевых пробелов (последовательность пробел+QChar::ParagraphSeparator на QChar::ParagraphSeparator)
// Эти самопроизвольно встваляемые пробелы появляются при вызове textArea->textCursor().insertHtml(htmlCode);
// Метод временно не используется
int TypefaceFormatter::replaceSpaceAndParagraphSeparatorToParagraphSeparator(int startCursorPos, int calculateEndCursorPos)
{
    QTextCursor replacementCursor=textArea->textCursor();

    bool flagPreviousSpace=false;

    for(int pos=startCursorPos; pos<=calculateEndCursorPos; pos++)
    {
        // Выделяется один символ
        replacementCursor.setPosition(pos, QTextCursor::MoveAnchor);
        replacementCursor.setPosition(pos+1, QTextCursor::KeepAnchor);

        // Если выделение символа прошло успешно
        if(replacementCursor.selectedText().length()>0)
        {
            QChar currentChar=replacementCursor.selectedText().at(0);
            // qDebug() << "Pos: " << pos << " Char: " << currentChar << " Char code: " << currentChar.unicode();

            if(currentChar==QChar::ParagraphSeparator && flagPreviousSpace)
            {
                // qDebug() << "Find space + paragraph separator";

                // Снимается выделение, чтобы правильно сработало удаление символа
                replacementCursor.setPosition(pos, QTextCursor::MoveAnchor);

                // Удаляется предыдущий символ
                replacementCursor.deletePreviousChar();

                // Номер текущего символа уменьшается на единицу, чтобы не пропустить следующую строку из пробела+QChar::ParagraphSeparator
                --pos;

                // Так как символ удален, длина всего вставляемого фрагмента уменьшается на единицу
                --calculateEndCursorPos;

                // Снимается флаг, что предыдущий символ был пробелом
                flagPreviousSpace=false;
            }

            if(currentChar==QChar::Space)
                flagPreviousSpace=true;
        }
    }

    return calculateEndCursorPos; // Возвращается последняя позиция в выделении, так как количество символов в выделении изменилось
}


// Упрощенная очистка начертания текста
// Взята из старой версии MyTetra 1.32.163
// Используется для очистки начертания в строках, отформатированных как нумерованное или ненумерованное перечисление
// Так как при использовании стандартного более агрессивного метода очистки, происходит создание новой строки в месте выделения,
// а условие в агресивный метод невозможно вставить, так как на вход очищающего кода (даже в случае одного
// слова из нумерованной строки) подается конструкция <ul><li>слово</li></ul>, которую невозможно отличить от конструкции
// с реальным одельным пунктом перечисления
void TypefaceFormatter::clearSimple(void)
{
    bool flagCursorOnEmptyLine=editor->cursorPositionDetector->isCursorOnEmptyLine();
    bool flagCursorOnSpaceLine=editor->cursorPositionDetector->isCursorOnSpaceLine();

    // Очистка возможна только если что-то выделено
    // Или курсор стоит на пустой строке с одним символом перевода строки
    // Или курсор стоит на строке, в которой нет текста
    if(!(textArea->textCursor().hasSelection() ||
         flagCursorOnEmptyLine ||
         flagCursorOnSpaceLine))
        return;

    if(flagCursorOnSpaceLine)
        (textArea->textCursor()).select(QTextCursor::LineUnderCursor);

    applyStandartFontForSelection();

    // Очищается формат символов
    QColor clearColor;
    QBrush clearBrush( clearColor );
    QTextCharFormat clearCharFormat;
    clearCharFormat.setForeground( clearBrush );
    textArea->setCurrentCharFormat( clearCharFormat );

    // Если выделен блок
    // или курсор на пустой линии
    // или курсор на линии на которой нет символов
    if(editor->cursorPositionDetector->isBlockSelect() ||
            flagCursorOnEmptyLine ||
            flagCursorOnSpaceLine)
    {
        QTextBlockFormat format;
        // Убираются отступы
        format.setLeftMargin(0); // Убирается левый отступ (который, возможно был установлен слайдером или кнопками изменения отступа)
        format.setRightMargin(0);
        format.setTopMargin(0); // Убираются межстрочные интервалы, которые самопроизвольно появляются при вставке из других программ
        format.setBottomMargin(0);
        format.setAlignment(Qt::AlignLeft); // Выравнивание по левому краю
        // Применение форматирование
        textArea->textCursor().setBlockFormat(format);
    }

    // Если была работа со строкой, в которой нет символов,
    // курсор переносится на начало строки, чтобы не путать пользователя
    if(flagCursorOnSpaceLine)
        textArea->moveCursor(QTextCursor::StartOfLine);
}


// todo: Сравнить с методом onClearClicked() и сделать общий код
void TypefaceFormatter::onTextOnlyClicked()
{
    int startCursorPos=textArea->textCursor().anchor(); // Начало выделения
    int stopCursorPos=textArea->textCursor().position(); // Конец выделения
    // qDebug() << "Cursor start position: " << startCursorPos << "Cursor stop position: " << stopCursorPos;

    // Если выделение было сзаду-наперед, надо поменять начальную и конечную позицию местами
    bool isSelectionReverse=false;
    if(startCursorPos>stopCursorPos)
    {
        // Меняется только начальная позиция. Конечная позиция далее не используется,
        // так как длина выделенного фрагмента будет вычисляться специальным образом
        // (будет удаляться выделенная часть и вставляться новый очищенный текст,
        // а его длина будет отличаться от длины начального выделения)
        startCursorPos=stopCursorPos;
        isSelectionReverse=true;
    }
    // qDebug() << "Cursor start position: " << startCursorPos << "Cursor stop position: " << stopCursorPos;

    bool flag_cursor_on_empty_line=editor->cursorPositionDetector->isCursorOnEmptyLine();
    bool flag_cursor_on_space_line=editor->cursorPositionDetector->isCursorOnSpaceLine();

    // Очистка возможна только если что-то выделено
    // Или курсор стоит на пустой строке с одним символом перевода строки
    // Или курсор стоит на строке, в которой нет текста
    if(!(textArea->textCursor().hasSelection() ||
         flag_cursor_on_empty_line ||
         flag_cursor_on_space_line))
        return;

    // Запоминается положение прокрутки окна редактирования.
    // Это нужно, чтобы вернуться к такому положению после всех действий.
    // Иначе прокрутка перепрыгнет наверх документа
    int scrollBarPosition=editor->getScrollBarPosition();

    textArea->textCursor().beginEditBlock();

    // Если курсор на строке, у которой нет текста
    if(flag_cursor_on_space_line)
        (textArea->textCursor()).select(QTextCursor::LineUnderCursor);

    // Запоминается выделенный текст - только текст, без форматирования
    QString text=textArea->textCursor().selectedText();
    // qDebug() << "Text without formatting: " << text;

    // Удаляются спецсимволы, замещающие картинки. Если этого не сделать, после вставки на экране текст будет виден,
    // а во внутреннем представлении исчезнет целый абзац, где была картинка, и текст абзаца не будет сохранен
    text=text.replace(QChar::ObjectReplacementCharacter, QChar::Space);
    // qDebug() << "Text without difficul characters: " << text;

    // Удаление выделенного фрагмента
    textArea->textCursor().removeSelectedText();

    // С помощью дополнительного курсора выясняется последняя позиция в тексте, в котором удален выделенный фрагмент
    QTextCursor cursor=textArea->textCursor();
    cursor.movePosition(QTextCursor::End);
    int afterRemoveSelectionLen=cursor.position();
    // qDebug() << "After remove selection length: " << afterRemoveSelectionLen;

    // Вставка запомненного текста
    textArea->textCursor().insertText(text);
    // qDebug() << "After insert HTML: "<< textArea->toHtml();

    // С помощью дополнительного курсора выясняется последняя позиция в тексте, в котором вставлен очищенный фрагмент
    cursor.movePosition(QTextCursor::End);
    int afterClearLen=cursor.position();
    // qDebug() << "After clear length: " << afterClearLen;

    // Вычисляется последняя позиция выделения очищенного текста
    int calculateEndCursorPos=startCursorPos + (afterClearLen - afterRemoveSelectionLen);
    // qDebug() << "Calculate end cursor pos: " << calculateEndCursorPos;


    // ********************************
    // Выделение вставленного фрагмента
    // Если его не сделать, то первая строка получит дополнительные вертикальные отступы. Это особенность Qt
    // ********************************
    if(!isSelectionReverse)
    {
        cursor.setPosition(startCursorPos, QTextCursor::MoveAnchor);
        cursor.setPosition(calculateEndCursorPos, QTextCursor::KeepAnchor);
    }
    else
    {
        cursor.setPosition(calculateEndCursorPos, QTextCursor::MoveAnchor);
        cursor.setPosition(startCursorPos, QTextCursor::KeepAnchor);
    }
    textArea->setTextCursor(cursor);

    applyStandartFontForSelection();
    removeAnchorDataForSelection();
    clearColorForSelection();

    // Если выделен блок
    // или курсор на пустой линии
    // или курсор на линии на которой нет символов
    if(editor->cursorPositionDetector->isBlockSelect() ||
            flag_cursor_on_empty_line ||
            flag_cursor_on_space_line)
        applyPureBlockFormatForSelection();

    // Если была работа со строкой, в которой нет символов,
    // курсор переносится на начало строки, чтобы не путать пользователя
    if(flag_cursor_on_space_line)
        textArea->moveCursor(QTextCursor::StartOfLine);

    textArea->textCursor().endEditBlock();

    editor->setScrollBarPosition(scrollBarPosition);

    // Вызывается метод, как будто переместился курсор с выделением, чтобы
    // обновились состояния подсветок кнопок форматирования
    editor->onSelectionChanged();
    editor->updateIndentsliderToActualFormat();

    emit updateOutlineButtonHiglight();
}


/*
void TypefaceFormatter::onTextOnlyClickedV1()
{
  int startCursorPos=textArea->textCursor().position();
  int stopCursorPos=textArea->textCursor().anchor();

  qDebug() << "Cursor start position: " << startCursorPos << "Cursor stop position: " << stopCursorPos;
  bool flag_cursor_on_empty_line=editor->cursorPositionDetector->isCursorOnEmptyLine();
  bool flag_cursor_on_space_line=editor->cursorPositionDetector->isCursorOnSpaceLine();

  // Очистка возможна только если что-то выделено
  // Или курсор стоит на пустой строке с одним символом перевода строки
  // Или курсор стоит на строке, в которой нет текста
  if(!(textArea->textCursor().hasSelection() ||
       flag_cursor_on_empty_line ||
       flag_cursor_on_space_line))
    return;

  textArea->textCursor().beginEditBlock();

  // Если курсор на строке, у которой нет текста
  if(flag_cursor_on_space_line)
    (textArea->textCursor()).select(QTextCursor::LineUnderCursor);

  // Создается стандартный шрифт
  QFont font;
  font.fromString( editorConfig->get_default_font() ); // Стандартное начертание взятое из конфига
  font.setPointSize( editorConfig->get_default_font_size() ); // Стандартный размер взятый из конфига

  // Применяется стандартный шрифт
  textArea->setCurrentFont(font);

  // Новый установленный шрифт показывается в выпадающем списке шрифтов
  emit changeFontselectOnDisplay(font.family());

  // В выпадающем списке размеров выставляется установленный размер
  emit changeFontsizeOnDisplay(editorConfig->get_default_font_size());

  // Очищается формат символов
  QColor clearColor;
  QBrush clearBrush( clearColor );
  QTextCharFormat clearCharFormat;
  clearCharFormat.setForeground( clearBrush );
  textArea->textCursor().mergeCharFormat(clearCharFormat);

  // Если выделен блок
  // или курсор на пустой линии
  // или курсор на линии на которой нет символов
  if(editor->cursorPositionDetector->isBlockSelect() ||
     flag_cursor_on_empty_line ||
     flag_cursor_on_space_line)
  {
    QTextBlockFormat format;

    // Убираются отступы
    format.setLeftMargin(0); // Убирается левый отступ (который, возможно был установлен слайдером или кнопками изменения отступа)
    format.setRightMargin(0);
    format.setTopMargin(0); // Убираются межстрочные интервалы, которые самопроизвольно появляются при вставке из других программ
    format.setBottomMargin(0);
    format.setAlignment(Qt::AlignLeft); // Выравнивание по левому краю

    // Применение форматирование
    textArea->textCursor().setBlockFormat(format);
  }

  // Если была работа со строкой, в которой нет символов,
  // курсор переносится на начало строки, чтобы не путать пользователя
  if(flag_cursor_on_space_line)
    textArea->moveCursor(QTextCursor::StartOfLine);

  textArea->textCursor().endEditBlock();

  // Вызывается метод, как будто переместился курсор с выделением, чтобы
  // обновились состояния подсветок кнопок форматирования
  editor->onSelectionChanged();
  editor->updateIndentsliderToActualFormat();
}
*/

void TypefaceFormatter::onFixBreakSymbolClicked()
{
    textArea->textCursor().beginEditBlock();

    int startCursorPos=textArea->textCursor().position();
    int stopCursorPos=textArea->textCursor().anchor();

    // Получение исходного кода выделенного фрагмента
    QString htmlCode=textArea->textCursor().selection().toHtml();

    // qDebug() << "Before replace break in onFixBreakSymbolClicked: " << htmlCode;

    // В регулярных выражениях Qt кванторы по-умолчанию жадные (greedy)
    // Поэтому напрямую регвыру указывается что кванторы должны быть ленивые
    QRegExp replace_expression("<br\\s?/>");
    replace_expression.setMinimal(true);

    htmlCode.replace(replace_expression, "</p><p>");

    // qDebug() << "After remove style: " << htmlCode;

    // Старый код убирается
    textArea->textCursor().removeSelectedText();

    // Новый код вставляется
    textArea->textCursor().insertHtml(htmlCode);

    textArea->textCursor().setPosition(startCursorPos);
    textArea->textCursor().setPosition(stopCursorPos, QTextCursor::KeepAnchor);
    // textArea->setTextCursor( textArea->textCursor() );

    textArea->textCursor().endEditBlock();
}


// Преобразование табуляций в пробелы в выделении (или во всей записи
// без выделения). Ширина таба — из настроек редактора. Табы ищутся
// поиском по документу и меняются с конца к началу, поэтому картинки,
// таблицы и прочее нетекстовое содержимое не затрагивается
void TypefaceFormatter::onTabsToSpacesClicked(void)
{
    const int tabSpaces=editorConfig->get_tab_size()>0 ? editorConfig->get_tab_size() : 4;
    const QString spaces=QString(tabSpaces, QChar(' '));

    QTextDocument *doc=textArea->document();
    QTextCursor cursor=textArea->textCursor();

    int from;
    int to;
    int anchor;
    int curPos=cursor.position();

    if(cursor.hasSelection())
    {
        from=cursor.selectionStart();
        to=cursor.selectionEnd();
        anchor=cursor.anchor()<=cursor.position() ? -1 : 1;
    }
    else
    {
        from=0;
        to=doc->characterCount()-1;
        anchor=0;
    }

    if(to<=from)
        return;

    // Позиции табов прямым проходом, замена обратным
    QList<int> tabs;
    int pos=from;

    while(true)
    {
        QTextCursor found=doc->find(QStringLiteral("\t"), pos);

        if(found.isNull() || found.selectionStart()>=to)
            break;

        tabs.append(found.selectionStart());
        pos=found.selectionEnd();
    }

    if(tabs.isEmpty())
        return;

    cursor.beginEditBlock();

    for(int i=tabs.size()-1; i>=0; --i)
    {
        const int tabPos=tabs.at(i);
        QTextCursor edit(doc);
        edit.setPosition(tabPos);
        edit.setPosition(tabPos+1, QTextCursor::KeepAnchor);
        edit.insertText(spaces);
    }

    cursor.endEditBlock();

    // Выделение натягивается поверх того же текста
    const int grown=tabs.size()*(tabSpaces-1);

    if(anchor==0)
    {
        int before=0;

        foreach(int tabPos, tabs)
            if(tabPos<curPos)
                before++;

        QTextCursor restore(doc);
        restore.setPosition(curPos+before*(tabSpaces-1));
        textArea->setTextCursor(restore);
    }
    else if(anchor<0)
    {
        cursor.setPosition(from);
        cursor.setPosition(to+grown, QTextCursor::KeepAnchor);
        textArea->setTextCursor(cursor);
    }
    else
    {
        cursor.setPosition(to+grown);
        cursor.setPosition(from, QTextCursor::KeepAnchor);
        textArea->setTextCursor(cursor);
    }
}


// Слот, срабатывающий при изменении шрифта в списке шрифтов
void TypefaceFormatter::onFontselectChanged(const QFont &font)
{
    // TRACELOG

    // todo: Разобраться с этим флагом. Его нужно поместить в Editor или ToolBar, или вообще от него отказаться
    // if(editor->flagSetFontParametersEnabled==false)
    //   return;
    if( !(editor->editorToolBarAssistant->getFlagSetFontParametersEnabled()) )
        return;

    // Нельзя устанавливать через интерфейс пользователя неизвестный шрифт
    if(font.family()=="") {
        return;
    }

    // textArea->setFontFamily(font.family()); // Устанавливается выбранный шрифт в области редактирования
    // editor->currentFontFamily=font.family();
    this->doChangeFontFamily( font.family() );

    // Примененный шрифт — наверх списка недавних (без дублей, максимум 5).
    // Комбобокс перестраивается и возвращается на текущий шрифт
    editorConfig->add_recentFont(font.family());
    editor->editorToolBarAssistant->fontSelect->reloadRecentFonts(editorConfig->get_recentFonts(),
                                                                  font.family());

    // Курсор после выбора возвращается в область редактирования
    textArea->setFocus();
}


// Слот, срабатывающий когда изменен размер шрифта через список размеров
void TypefaceFormatter::onFontsizeChanged(int n)
{
    // TRACELOG

    // todo: Разобраться с этим флагом. Его нужно поместить в Editor или ToolBar, или вообще от него отказаться
    // if(editor->flagSetFontParametersEnabled==false)
    //   return;
    if( !(editor->editorToolBarAssistant->getFlagSetFontParametersEnabled()) )
        return;

    // Номер выбранного размера преобразуется в размер
    n=editor->editorToolBarAssistant->getFontSizeByNum(n);

    if(n<MINIMUM_ALLOWED_FONT_SIZE || n>MAXIMUM_ALLOWED_FONT_SIZE)
        return;

    // textArea->setFontPointSize(n);
    // editor->currentFontSize=n;
    this->doChangeFontPointSize( n );

    // Курсор после выбора возвращается в область редактирования
    textArea->setFocus();
}


// Слот, срабатыващий при нажатии на кнопки выбора цвета текста
// Параметр n - это номер выбранного пункта в выпадающей кнопке выбора цвета,
// счет кнопок с нуля.
// При n=0 нужно запустить диалог выбора цвета,
// При n=1 нужно установить стандартный цвет (нет цвета)
void TypefaceFormatter::onFontcolorClicked(int n)
{
    // TRACELOG

    QColor selectedColor;

    if (n==0)
    {
        // Текущий цвет шрифта возле курсором
        QColor currentColor = textArea->textColor();

        // Формат символов под курсором
        QTextCharFormat textAreaCharFormat = textArea->currentCharFormat();

        // Есть ли ForegroundBrush под курсором в тексте
        bool hasForegroundBrush = textAreaCharFormat.hasProperty(QTextFormat::ForegroundBrush);

        // Если нет ForegroundBrush в тексте под курсором, то
        // за цвет кнопки берется цвет foreground редактора textArea (QTextEdit)
        // (это позволяет учитывать также цвет шрифта, заданный в файле stylesheet.css)
        if(!hasForegroundBrush)
            currentColor = textArea->palette().windowText().color();

        // Диалог запроса цвета текста
        selectedColor = QColorDialog::getColor(currentColor,
                                               editor,
                                               tr("Select text color"),
                                               QColorDialog::ShowAlphaChannel);

        // Если цвет выбран, и он правильный
        if(selectedColor.isValid())
        {
            // Запоминается как последний: клик по кнопке красит в него
            // без пикера (если включено в настройках)
            editorConfig->set_lastFontColor(selectedColor.name(QColor::HexArgb));

            // Меняется цвет кнопки и устанавливается цвет текста
            this->doChangeFontcolor( selectedColor );
        }
    }

    if (n==1)
    {
        selectedColor = QApplication::style()->standardPalette().color(QPalette::WindowText);

        // Меняется цвет кнопки и устанавливается цвет текста
        this->doChangeFontcolor( selectedColor );
    }
}


// Клик по самой кнопке цвета: красит выделение в последний выбранный
// цвет без вызова пикера. Выключено настройкой — ничего не делает.
// Цвета еще нет (ни разу не выбирали) — открывается пикер
void TypefaceFormatter::onFontcolorApplyLast(void)
{
    if(editorConfig->get_applyLastColorOnClick()==false)
        return;

    const QColor lastColor(editorConfig->get_lastFontColor());

    if(lastColor.isValid())
    {
        this->doChangeFontcolor(lastColor);
        return;
    }

    onFontcolorClicked(0);
}


// Вставка горизонтальной линии в "пустой" абзац, где расположен курсор (пустой абзац заменяется на горизонтальную линию)
// Если есть выделение в тексте, или курсор стоит на тексте, вставка не производится
void TypefaceFormatter::onInsertHorizontalLineClicked()
{
    // Если есть выделение в тексте, то вставка не производится
    if (textArea->textCursor().hasSelection())
        return;

    QTextCursor textCursor = textArea->textCursor();
    textCursor = textArea->textCursor();

    // Если курсор стоит на тексте, то вставка не производится
    textCursor.select(QTextCursor::LineUnderCursor);
    if(!textCursor.selectedText().isNull())
        return;

    int cursorPosition = textCursor.position();

    // Выбираем 1 символ слева от позиции курсора
    textCursor = textArea->textCursor();
    textCursor.movePosition(QTextCursor::StartOfLine);
    textCursor.setPosition(cursorPosition-1, QTextCursor::MoveAnchor);
    textCursor.setPosition(cursorPosition, QTextCursor::KeepAnchor);

    // Определяем, не является ли этот символ слева от курсора мягким переносом
    QString html = textCursor.selection().toHtml();
    QRegExp regExp("<span\\s+style=\"(?:(?:(?:\\s*font-family:'(?:[^<]+)';)(?:\\s*font-size:(?:\\d+)pt;))|(?:(?:\\s*font-size:(?:\\d+)pt;)(?:\\s*font-family:'(?:[^<]+)';)))\">\\s*<br\\s*/\\s*>\\s*</span>");
    regExp.setMinimal(true);
    if(html.indexOf(regExp) != -1)
    {
        // Если это мягкий перенос - заменяем его на абзац
        textArea->setTextCursor(textCursor);
        textArea->textCursor().insertText("\n");
    }

    // Вставка горизонтальной линии в "пустой" абзац (заменяем его на <hr>)
    textCursor.setPosition(cursorPosition, QTextCursor::MoveAnchor);
    textCursor.setPosition(cursorPosition+1, QTextCursor::KeepAnchor);
    textArea->setTextCursor(textCursor);
    textArea->textCursor().removeSelectedText();
    textArea->moveCursor(QTextCursor::Left);
    textArea->insertHtml("<hr>");
    textArea->moveCursor(QTextCursor::Down);
    textArea->moveCursor(QTextCursor::StartOfLine);
}


// Обработка мягкого переноса
// Учитываются мягкие переносы до выделенного текста (1-й символ до выделения),
// после выделенного текста и в выделенных абзацах
void TypefaceFormatter::workingSoftCarryInSelection()
{
    // Если нет выделения, то возврат
    if(!textArea->textCursor().hasSelection())
        return;

    int scrollBarPosition=editor->getScrollBarPosition();

    // Запоминаем первоначальное выделение текста
    int selectionStart = textArea->textCursor().selectionStart();
    int selectionEnd   = textArea->textCursor().selectionEnd();

    // Выбираем 1 символ слева от начала выделения текста
    QTextCursor textCursor = textArea->textCursor();
    textCursor.movePosition(QTextCursor::StartOfLine);
    textCursor.setPosition(selectionStart-1, QTextCursor::MoveAnchor);
    textCursor.setPosition(selectionStart, QTextCursor::KeepAnchor);

    // Определяем, является ли этот 1-й символ слева от выделения текста мягким переносом
    if(textCursor.anchor() != 0 && textCursor.position() != 0) // Пропускаем начало документо
    {
        QString html = textCursor.selection().toHtml();
        QRegExp regExp("<span\\s+style=\"(?:(?:(?:\\s*font-family:'(?:[^<]+)';)(?:\\s*font-size:(?:\\d+)pt;))|(?:(?:\\s*font-size:(?:\\d+)pt;)(?:\\s*font-family:'(?:[^<]+)';)))\">\\s*(?:<br\\s*/\\s*>\\s*){1,}\\s*</span>");
        regExp.setMinimal(true);
        if(html.indexOf(regExp) != -1)
        {
            // Если это мягкий перенос - заменяем его на абзац
            textArea->setTextCursor(textCursor);
            textArea->textCursor().insertText("\n");
        }
    }

    // Расширяем выделение на 1 символ вправо, чтобы захватить мягкий перенос в конце выделения, если он есть
    textCursor.setPosition(selectionStart, QTextCursor::MoveAnchor);
    textCursor.setPosition(selectionEnd+1, QTextCursor::KeepAnchor);
    textArea->setTextCursor(textCursor);

    // Ищем мягкий перенос в качестве пустого абзаца в расширенном вправо выделении
    QString htmlCode = textArea->textCursor().selection().toHtml();
    QRegExp regExp("<span\\s+style=\"(?:(?:(?:\\s*font-family:'(?:[^<]+)';)(?:\\s*font-size:(?:\\d+)pt;))|(?:(?:\\s*font-size:(?:\\d+)pt;)(?:\\s*font-family:'(?:[^<]+)';)))\">\\s*(?:<br\\s*/\\s*>\\s*){1,}\\s*</span>");
    regExp.setMinimal(true);
    if(htmlCode.indexOf(regExp) != -1)
    {
        // Заменяем проблемный код в html
        htmlCode.replace(regExp, "</p><p><br/>");
        textArea->textCursor().insertHtml(htmlCode);
    }

    // Теперь обрабатываем смитуацию, когда на конце строки - мягкий перенос
    htmlCode = textArea->textCursor().selection().toHtml();
    regExp.setPattern("(<span\\s+style=\"\\s*((?:[^<]+);\">)(?:.+)\\s*)(?:(?:<br\\s*/\\s*>\\s*){1,}\\s*)(</span>)");
    regExp.setMinimal(true);
    if(htmlCode.indexOf(regExp) != -1)
    {
        // Удаляем в html код <br/>
        htmlCode.replace(regExp, "\\1");
        textArea->textCursor().insertHtml(htmlCode);
    }

    // Восстанавливаем выделение курсора
    textCursor.setPosition(selectionStart, QTextCursor::MoveAnchor);
    textCursor.setPosition(selectionEnd, QTextCursor::KeepAnchor);
    textArea->setTextCursor(textCursor);

    editor->setScrollBarPosition(scrollBarPosition);
}


void TypefaceFormatter::onLowerCase()
{
    this->replaceSymbolCase(QChar::Letter_Lowercase);
}


void TypefaceFormatter::onUpperCase()
{
    this->replaceSymbolCase(QChar::Letter_Uppercase);
}


void TypefaceFormatter::replaceSymbolCase(const QChar::Category &category)
{
    QTextCursor cursor = textArea->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    int startCursorPos=cursor.anchor(); // Начало выделения
    int stopCursorPos=cursor.position(); // Конец выделения

    // Если выделение было сзаду-наперед, надо поменять начальную и конечную позицию местами
    bool isSelectionReverse=false;
    if(startCursorPos>stopCursorPos)
    {
        int tempCursorPos=startCursorPos;
        startCursorPos=stopCursorPos;
        stopCursorPos=tempCursorPos;

        isSelectionReverse=true;
    }

    if( isSelectionReverse )
    {
        cursor.setPosition(startCursorPos, QTextCursor::MoveAnchor);
        cursor.setPosition(stopCursorPos, QTextCursor::KeepAnchor);
    }

    QTextCursor replacementCursor=textArea->textCursor();

    replacementCursor.beginEditBlock();

    for(int pos=startCursorPos; pos<stopCursorPos; pos++)
    {
        // Выделяется один символ
        replacementCursor.setPosition(pos, QTextCursor::MoveAnchor);
        replacementCursor.setPosition(pos+1, QTextCursor::KeepAnchor);

        QString symbol=replacementCursor.selectedText();

        if( category==QChar::Letter_Lowercase )
        {
            symbol=symbol.toLower();
        }
        else if ( category==QChar::Letter_Uppercase )
        {
            symbol=symbol.toUpper();
        }

        replacementCursor.insertText(symbol);
    }

    replacementCursor.endEditBlock();
}


QList<QTextCursor> TypefaceFormatter::getComplexSelectionCursors()
{
    QList<QTextCursor> cursors;

    QTextCursor mainCursor = textArea->textCursor();

    if (!mainCursor.hasSelection())
    {
        return cursors;
    }

    QTextTable* table = mainCursor.currentTable();

    if (!table)
    {
        cursors << mainCursor;
        return cursors;
    }

    int selectionStart = mainCursor.selectionStart();
    int selectionEnd = mainCursor.selectionEnd();

    // Границы выделения в координатах таблицы
    QTextCursor startCursor = mainCursor;
    startCursor.setPosition(selectionStart);
    QTextTableCell startCell = table->cellAt(startCursor);

    QTextCursor endCursor = mainCursor;
    endCursor.setPosition(selectionEnd);
    QTextTableCell endCell = table->cellAt(endCursor);

    if (!startCell.isValid() || !endCell.isValid()) return cursors;

    // Прямоугольник выделения
    int startRow = qMin(startCell.row(), endCell.row());
    int endRow = qMax(startCell.row(), endCell.row());
    int startCol = qMin(startCell.column(), endCell.column());
    int endCol = qMax(startCell.column(), endCell.column());

    // qDebug() << "Выделенный прямоугольник: строки" << startRow << "-" << endRow
    //          << ", столбцы" << startCol << "-" << endCol;

    // Для каждой ячейки в выделенном прямоугольнике создается отдельный курсор
    for (int row = startRow; row <= endRow; ++row) {
        for (int col = startCol; col <= endCol; ++col) {
            QTextTableCell cell = table->cellAt(row, col);

            if (cell.isValid()) {
                // Создается курсор для этой конкретной ячейки
                QTextCursor cellCursor = cell.firstCursorPosition();
                cellCursor.setPosition(cell.lastCursorPosition().position(), QTextCursor::KeepAnchor);
                cursors.append(cellCursor);

                /*
                qDebug() << "Ячейка [" << row << "," << col << "]:"
                         << cellCursor.selectedText()
                         << "позиции:" << cellCursor.selectionStart()
                         << "-" << cellCursor.selectionEnd();
                */
            }
        }
    }

    return cursors;
}


void TypefaceFormatter::clearBackgroundInSimpleSelection(const QTextCursor &cursor)
{
    QTextDocument *doc = textArea->document();
    const int selStart = cursor.selectionStart();
    const int selEnd   = cursor.selectionEnd();

    // 1) Очистка фона символов в выделении
    {
        QTextCursor cur(doc);
        cur.setPosition(selStart);
        cur.setPosition(selEnd, QTextCursor::KeepAnchor);

        QTextCharFormat cf;
        cf.setBackground(Qt::NoBrush); // Убирается установленный фон
        cur.mergeCharFormat(cf);
    }

    // 2) Очистка фона блоков, которые пересекают выделение
    {
        QTextBlock firstBlock = doc->findBlock(selStart);
        QTextBlock lastBlock  = doc->findBlock(selEnd);

        if (firstBlock.isValid()) {
            const int lastPos =
                lastBlock.isValid() ? lastBlock.position() : selEnd;

            for (QTextBlock b = firstBlock; b.isValid(); b = b.next()) {
                if (b.position() > lastPos)
                    break;

                QTextCursor bc(b);
                QTextBlockFormat bf = b.blockFormat();
                bf.setBackground(Qt::NoBrush);
                bc.setBlockFormat(bf);
            }
        }
    }

    // 3) Очистка фона ячеек таблиц и самих таблиц только внутри выделения
    clearTableCellsBackground(doc, selStart, selEnd);
}


void TypefaceFormatter::clearBackgroundInSelection()
{
    QTextCursor cur = textArea->textCursor();

    if ( !cur.hasSelection() )
    {
        return;
    }

    QList<QTextCursor> cursors;

    // Если это не простое непрерывное выделение
    if ( cur.hasComplexSelection() )
    {
        cursors << this->getComplexSelectionCursors();
    }
    else
    {
        cursors << cur;
    }


    for ( const auto &cursor : cursors )
    {
        this->clearBackgroundInSimpleSelection(cursor);
    }
}


// Обход документа: ищутся таблицы и чистятся в них ячейки, а так же чистятся сами таблицы
void TypefaceFormatter::clearTableCellsBackground(QTextDocument *doc, int selStart, int selEnd)
{
    QTextFrame *root = doc->rootFrame();
    findAndClearTables(root, selStart, selEnd);
}


// Рекурсивный обход фреймов, таблицы — это тоже QTextFrame-наследники
void TypefaceFormatter::findAndClearTables(QTextFrame *frame, int selStart, int selEnd)
{
    // Если этот фрейм — таблица
    if (QTextTable *table = qobject_cast<QTextTable *>(frame)) {
        clearSingleTable(table, selStart, selEnd);
    }

    // Обход дочерних фреймов
    for (QTextFrame::iterator it = frame->begin(); !it.atEnd(); ++it) {
        if (QTextFrame *childFrame = it.currentFrame()) {
            findAndClearTables(childFrame, selStart, selEnd); // Рекурсивный вызов
        }
        // currentBlock() здесь можно игнорировать, блоки уже обработаны выше
    }
}


// Очистка фона в ОДНОЙ таблице только в области пересечения с выделением
void TypefaceFormatter::clearSingleTable(QTextTable *table, int selStart, int selEnd)
{
    QTextDocument *doc = table->document();

    const int rows = table->rows();
    const int cols = table->columns();

    // Очистка ячеек
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {

            QTextTableCell cell = table->cellAt(r, c);

            const int cellStart = cell.firstCursorPosition().position();
            const int cellEnd   = cell.lastCursorPosition().position();

            // Ячейка НЕ пересекается с выделением — пропускаем
            if (cellEnd < selStart || cellStart > selEnd)
                continue;

            // Сброс фонового цвета самой ячейки
            {
                QTextTableCellFormat cf =
                    cell.format().toTableCellFormat();   // важно: toTableCellFormat()

                cf.setBackground(Qt::NoBrush);
                cell.setFormat(cf); // принимает QTextCharFormat по значению
            }

            // Вычисляется пересечение [cellStart, cellEnd] с [selStart, selEnd]
            const int from = qMax(cellStart, selStart);
            const int to   = qMin(cellEnd,   selEnd);

            // 2) Сброс фонового цвета символов внутри этой части ячейки
            {
                QTextCursor cursor(doc);
                cursor.setPosition(from);
                cursor.setPosition(to, QTextCursor::KeepAnchor);

                QTextCharFormat ch;
                ch.setBackground(Qt::NoBrush);
                cursor.mergeCharFormat(ch);
            }

            // 3) Сброс фонового цвета блоков внутри той же области
            {
                QTextBlock b = doc->findBlock(from);

                while (b.isValid() && b.position() <= to) {
                    QTextCursor bc(b);
                    QTextBlockFormat bf = b.blockFormat();
                    bf.setBackground(Qt::NoBrush);
                    bc.setBlockFormat(bf);
                    b = b.next();
                }
            }
        }
    }


    // Позиция начала и конца выделения таблицы
    const int tableStart = table->firstCursorPosition().position();
    const int tableEnd   = table->lastCursorPosition().position();

    // Если выделение полностью захватывает таблицу
    if ( selStart<=tableStart && selEnd>=tableEnd )
    {
        // Выясняется, имеет ли таблица установленный цвет фона
        QTextTableFormat tf = table->format();
        bool tableHasBackground = tf.hasProperty(QTextFormat::BackgroundBrush) &&
                                  tf.background().style() != Qt::NoBrush;

        // Если есть цвет фона, то он сбрасывается для всей таблицы
        if (tableHasBackground)
        {
            tf.setBackground(Qt::NoBrush);
            table->setFormat(tf);
        }
    }

}


// Слот, срабатыващий при нажатии на кнопку выбора цвета фона текста
// Параметр n - это номер выбранного пункта в выпадающей кнопке выбора цвета,
// счет кнопок с нуля.
// При n=0 нужно запустить диалог выбора цвета,
// При n=1 нужно установить стандартный цвет (нет цвета)
void TypefaceFormatter::onBackgroundcolorClicked(int n)
{
    // TRACELOG

    QColor selectedColor;

    // Пункт для выбора цвета
    if (n==0)
    {
        // Текущий цвет фона под курсором
        QColor currentColor = textArea->textBackgroundColor();

        // Формат символов под курсором
        QTextCharFormat textAreaCharFormat = textArea->currentCharFormat();

        // Есть ли BackgroundBrush под курсором в тексте
        bool hasTextBackgroundBrush = textAreaCharFormat.hasProperty(QTextFormat::BackgroundBrush);

        // Есть ли BackgroundBrush в тексте под курсором
        if(hasTextBackgroundBrush)
        {
            // Если есть BackgroundBrush в тексте под курсором, то
            // в диалог выбора цвета передаем цвет заливки текста под курсором
            currentColor = textArea->textBackgroundColor();
        }
        else
        {
            // Проверка, есть ли таблица под курсором и/или подключены стили из stylesheet.css
            QTextCursor txtCursor = textArea->textCursor();
            QTextTable *textTable = txtCursor.currentTable();
            if(textTable != nullptr)
            {
                // Если курсор находится в таблице
                QTextTableFormat textTableFormat = textTable->format();
                QTextTableCell tableCell = textTable->cellAt(txtCursor);
                QTextCharFormat tableCellFormat = tableCell.format();
                QColor tableColor = textTableFormat.background().color();
                QColor charColor = tableCellFormat.background().color();

                // Есть ли BackgroundBrush в таблице под курсором
                bool hasTableBackgroundBrush = textTableFormat.hasProperty(QTextFormat::BackgroundBrush);

                // Есть ли BackgroundBrush в ячейке под курсором
                bool hasCelBackgroundBrush = tableCellFormat.hasProperty(QTextFormat::BackgroundBrush);

                if(hasTableBackgroundBrush && hasCelBackgroundBrush && charColor.isValid())
                {
                    // Если есть BackgroundBrush в таблице под курсором и
                    // есть BackgroundBrush в ячейке под курсором, то
                    // в диалог выбора цвета передаем цвет заливки ячейки
                    currentColor = charColor;
                }
                else if(hasTableBackgroundBrush && !hasCelBackgroundBrush && tableColor.isValid())
                {
                    // Если есть BackgroundBrush в таблице под курсором но
                    // нет BackgroundBrush в ячейке под курсором, то
                    // в диалог выбора цвета передаем цвет заливки таблицы
                    currentColor = tableColor;
                }
                else
                {
                    // Если нет BackgroundBrush в таблице под курсором и
                    // нет BackgroundBrush в ячейке под курсором, то
                    // в диалог выбора цвета передаем цвет background редактора textArea (QTextEdit)
                    // (это позволяет учитывать также цвет фона, заданный в файле stylesheet.css)
                    currentColor = textArea->palette().window().color();
                }
            }
            else
            {
                // Если нет BackgroundBrush в тексте под курсором, то
                // в диалог выбора цвета передаем цвет background редактора textArea (QTextEdit)
                // (это позволяет учитывать также цвет фона, заданный в файле stylesheet.css)
                currentColor = textArea->palette().window().color();
            }
        }


        // Исправление прозрачности цвета, передаваемого в форму выбора цвета

        // Цвет с полной прозрачностью не может быть установлен как
        // инициализирующий в диалоге выбора цвета,
        // т.к. для установки полностью прозрачного цвета есть другой пункт - "без цвета"
        if ( currentColor.alpha() == 0)
        {
            // Начальный цвет в диалоге выбора цвета будет обычным, без прозрачности
            // Но наличие в диалоге настройки опции QColorDialog::ShowAlphaChannel
            // оставляет пользователю возможность менять прозразрачность как ему необходимо
            currentColor.setAlpha(255);
        }

        // Диалог запроса цвета фона
        selectedColor = QColorDialog::getColor(currentColor,
                                               editor,
                                               tr("Select background color"),
                                               QColorDialog::ShowAlphaChannel);
    }

    // Пункт для выбора "без цвета"
    if (n==1)
    {
        selectedColor = Qt::transparent;
    }

    // Если цвет выбран, и он правильный
    if(selectedColor.isValid())
    {
        // Запоминается как последний: клик по кнопке красит в него
        // без пикера (если включено в настройках)
        editorConfig->set_lastBackgroundColor(selectedColor.name(QColor::HexArgb));

        // Установка цвета фона текста и изменение цвета кнопки
        this->doChangeBackgroundColor( selectedColor );
    }

}


// Клик по самой кнопке фона: красит выделение в последний выбранный
// цвет без вызова пикера. Выключено настройкой — ничего не делает.
// Цвета еще нет (ни разу не выбирали) — открывается пикер
void TypefaceFormatter::onBackgroundcolorApplyLast(void)
{
    if(editorConfig->get_applyLastColorOnClick()==false)
        return;

    const QColor lastColor(editorConfig->get_lastBackgroundColor());

    if(lastColor.isValid())
    {
        this->doChangeBackgroundColor(lastColor);
        return;
    }

    onBackgroundcolorClicked(0);
}


// Вставка спецсимволов из диалога с таблицей и недавними.
// Диалог не закрывается на каждый клик: каждый символ прилетает
// сигналом symbolPicked и вставляется сразу, так можно набрать
// несколько подряд. Закрытие диалога — Cancel, Esc или крестик
void TypefaceFormatter::onSpecialCharClicked(void)
{
    EditorSpecialCharDialog dialog(editor);

    connect(&dialog, &EditorSpecialCharDialog::symbolPicked,
            this,    &TypefaceFormatter::insertSpecialChar);

    dialog.exec();
}


// Вставка одного символа в курсор (выделение заменяется),
// символ запоминается в недавние
void TypefaceFormatter::insertSpecialChar(const QString &symbol)
{
    if(symbol.isEmpty())
        return;

    textArea->textCursor().beginEditBlock();
    textArea->textCursor().removeSelectedText();
    textArea->textCursor().insertText(symbol);
    textArea->textCursor().endEditBlock();

    editorConfig->add_recentSymbol(symbol);
}


void TypefaceFormatter::doChangeFontcolor(const QColor &selectedColor)
{
    // TRACELOG

    // Если выделение есть
    if ( textArea->textCursor().hasSelection() )
    {
        textArea->setTextColor( selectedColor ); // Меняется цвет текста
    }
    else
    {
        // Иначе надо выделить дополнительным курсором слово на
        // котором стоит курсор
        QTextCursor cursor=textArea->textCursor();
        cursor.select( QTextCursor::WordUnderCursor );

        // Создается формат с нужным цветом текста и подмешивается в существующий
        QTextCharFormat format;
        format.setForeground( selectedColor );

        cursor.mergeCharFormat( format );
    }

    emit changeFontcolor( selectedColor );
}


// Изменение цвета фона текста
void TypefaceFormatter::doChangeBackgroundColor(const QColor &selectedColor)
{
    // TRACELOG

    // Если выбран цвет без полной прозрачности
    if ( selectedColor.alpha()!=0 )
    {
        // Если выделение есть
        if ( textArea->textCursor().hasSelection() )
        {
            // Меняется цвет фона
            // В движке QTextEdit есть особенность - если выделен текст и таблица,
            // то у таблицы в ячейках (в тегах <td>) будет выставляться цвет ячейки
            // через атрибут bgcolor. То есть, не для всей таблицы будет задан цвет фона,
            // а для всех ячеек. Это надо учитывать в дальнейшем
            textArea->setTextBackgroundColor( selectedColor );
        }
        else
        {
            // Иначе надо выделить дополнительным курсором слово на
            // котором стоит курсор
            QTextCursor cursor=textArea->textCursor();
            cursor.select( QTextCursor::WordUnderCursor );

            QTextCharFormat format;
            format.setBackground( selectedColor );

            cursor.mergeCharFormat( format );
        }
    }
    else
    {
        // Иначе выбран полностью прозрачный цвет заливки,
        // и надо не устанавливать прозрачный цвет, а сбрасывать цвет заливки
        textArea->textCursor().beginEditBlock();

        this->clearBackgroundInSelection();

        textArea->textCursor().endEditBlock();
    }

    // Устранение внутреннего бага QTextArea, когда после изменения цвета фона
    // перед таблицами может появиться пустая строка,
    // хотя нет никакого символа или блока форматирования, создающего данную пустую строку
    textArea->softRedraw();

    emit changeBackgroundcolor( selectedColor );
}


void TypefaceFormatter::doChangeFontFamily(QString fontFamily)
{
    // TRACELOG

    qDebug() << "Apply font family " << fontFamily;

    // Ранее для установки шрифта хватало одной команды setFontFamily(fontFamily);
    // Теперь так не работает, новый код сделан на основе Qt примера Text Edit

    QFont font(fontFamily);

    QTextCharFormat format;

    // Эта команда напрямую установит шрифт
    format.setFont(font);

    // Если делать только эту команду, то шрифт будет добавлен как рекомендуемый
    // к существующему, а не заменен. Команда оставлена, но возможно ее надо удалить
    format.setFontFamily( fontFamily );

    // Жирность и курсив существующего текста сохраняются, иначе setFont()
    // со шрифтом по умолчанию сбросит их в обычные (баг #94: форматирование
    // в Code убивало предыдущее форматирование)
    format.clearProperty(QTextFormat::FontWeight);
    format.clearProperty(QTextFormat::FontItalic);

    // Сброс свойства размера шрифта
    // Текущий HTML-преобразователь в Qt все равно дописывает в span размер шрифта,
    // если есть команда format.setFont().
    // Но если размер у формата будет очищен, то преобразователь добавит в span
    // тот размер, который для данного выделенного текста уже установлен
    // в тегах форматирования выше по структуре документа
    format.clearProperty(QTextFormat::FontPointSize);
    format.clearProperty(QTextFormat::FontPixelSize);
    format.clearProperty(QTextFormat::FontSizeAdjustment);

    qDebug() << "Font from font name: " << format.font().toString();

    // Если нет выделения, дополнительным курсором выделяется слово, на котором стоит курсор
    QTextCursor cursor = textArea->textCursor();
    if ( !cursor.hasSelection() )
    {
        cursor.select( QTextCursor::WordUnderCursor );
    }

    cursor.mergeCharFormat( format );
    textArea->mergeCurrentCharFormat( format );

    emit changeFontFamily( fontFamily );
}


void TypefaceFormatter::doChangeFontPointSize(int n)
{
    // TRACELOG

    textArea->setFontPointSize( n );

    emit changeFontPointSize( n );
}
