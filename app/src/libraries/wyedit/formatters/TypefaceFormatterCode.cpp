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


#include "../Editor.h"
#include "../EditorConfig.h"
#include "../EditorTextArea.h"
#include "../EditorCursorPositionDetector.h"


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


// Выбор пункта меню кнопки кода: форматирование с подсветкой выбранного языка
void TypefaceFormatter::onCodeLanguageClicked(int language)
{
    if(language<CodePlain || language>=CodeCount)
        return;

    formatCodeWithLanguage(language);
}


// Форматирование в качестве кода


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
    case CodeJson:    return QStringLiteral("JSON");
    case CodeLua:     return QStringLiteral("Lua");
    case CodeMake:    return QStringLiteral("Make");
    case CodePhp:     return QStringLiteral("PHP");
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


// Распознать наш маркер в строке, вернуть язык или -1.
// Адрес ссылки-кнопки [copy] в строке маркера
QString TypefaceFormatter::copyCodeHref(void)
{
    return QStringLiteral("mytetra://copy-code");
}


// Текст кнопки копирования, добавляемый к строке маркера через пробел


// Текст кнопки копирования, добавляемый к строке маркера через пробел
QString TypefaceFormatter::copyCodeSuffix(void)
{
    return QStringLiteral("[copy]");
}


// Цвет плашки блока кода


// Цвет плашки блока кода
QColor TypefaceFormatter::codeCardColor(void)
{
    return QColor(QStringLiteral("#F2F4F7"));
}


// Распознать наш маркер в строке, вернуть язык или -1.
// Форматы: // {lang = Имя}, # {lang = Имя}, -- {lang = Имя},
// ; {lang = Имя}, /* {lang = Имя} */, <!-- {lang = Имя} -->.
// Совпадение строгое: вся строка - только маркер


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

    // JSON маркер имеет: копирование идет строками под маркером,
    // сам маркер в буфер не попадает, поэтому настоящий комментарий
    // в языке не требуется
    for(int lang=CodeAngular; lang<CodeCount; ++lang)
        if(codeMarkerName(lang)==name)
            return lang;

    return -1;
}


// Обеспечить маркер языка в начале выделения: заменить наш маркер
// (в выделении или строкой выше) на новый, вставить новый сверху
// или убрать маркер для языка без маркера (Plain).
// Вызывается внутри edit-блока formatCodeWithLanguage.
// Возвращает false, если после обработки выделять нечего


// Обеспечить маркер языка в начале выделения: заменить наш маркер
// (в выделении или строкой выше) на новый, вставить новый сверху
// или убрать маркер для языка без маркера (Plain).
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
        unlinkMarkerText(markerPos, newMarker.length());

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
        unlinkMarkerText(insertPos, newMarker.length());

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
// без суффикса. Вызывается внутри edit-блока.
// Подчеркивание специально НЕ ставится: ссылка-кнопка выглядит как
// обычный текст (узнается по руке курсора), а HTML-импортер при
// загрузке подчеркивает все ссылки - расхождение чистится
// в normalizeCopyLinkUnderline


// Оформить суффикс-кнопку [copy] в строке маркера как ссылку.
// lineStart - начало строки, markerLen - длина текста маркера
// без суффикса. Вызывается внутри edit-блока.
// Подчеркивание специально НЕ ставится: ссылка-кнопка выглядит как
// обычный текст (узнается по руке курсора), а HTML-импортер при
// загрузке подчеркивает все ссылки - расхождение чистится
// в normalizeCopyLinkUnderline
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

    linkCursor.mergeCharFormat(linkFormat);
}


// Снять ссылку с текста маркера и пробела после него: insertText
// при замене маркера наследует формат конца выделения (ссылку
// [copy]), и вся строка сохраняется одной ссылкой. Суффикс
// [copy] не трогается. Вызывается внутри edit-блока


// Снять ссылку с текста маркера и пробела после него: insertText
// при замене маркера наследует формат конца выделения (ссылку
// [copy]), и вся строка сохраняется одной ссылкой. Суффикс
// [copy] не трогается. Вызывается внутри edit-блока
void TypefaceFormatter::unlinkMarkerText(int lineStart, int markerLen)
{
    QTextCursor edit(textArea->document());
    edit.setPosition(lineStart);
    edit.setPosition(lineStart+markerLen+1, QTextCursor::KeepAnchor);

    QTextCharFormat plainFormat;
    plainFormat.setAnchor(false);
    plainFormat.setAnchorHref(QString());

    edit.mergeCharFormat(plainFormat);
}


// Проверка, что абзац выглядит как строка блока кода: либо залит
// плашкой карточки, либо набран шрифтом кода (блоки, оформленные
// до появления карточек, опознаются по шрифту)


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


// Язык блока кода под курсором (индекс CodeLanguage) или -1.
// Как isCursorInCodeBlock, но возвращает язык: стоим на маркере —
// язык маркера, стоим в коде — язык маркера выше по блоку


// Язык блока кода под курсором (индекс CodeLanguage) или -1.
// Как isCursorInCodeBlock, но возвращает язык: стоим на маркере —
// язык маркера, стоим в коде — язык маркера выше по блоку
int TypefaceFormatter::codeLanguageAtCursor(void)
{
    QTextCursor cursor=textArea->textCursor();

    const int pos=cursor.hasSelection() ? cursor.selectionStart() : cursor.position();

    QTextDocument *doc=textArea->document();

    QTextBlock block=doc->findBlock(pos);

    if(block.isValid())
    {
        const int markerLang=codeMarkerLanguage(block.text());

        if(markerLang>=0)
            return markerLang;
    }

    int guard=0;
    while(block.isValid() && isCodeBlock(block) && guard<100000)
    {
        QTextBlock prev=block.previous();

        if(!prev.isValid())
            break;

        const int prevLang=codeMarkerLanguage(prev.text());

        if(prevLang>=0)
            return prevLang;

        block=prev;
        ++guard;
    }

    return -1;
}


// Тот же копипаст, но точка берется из текущего курсора
// (для контекстного меню: начало выделения или позиция)


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





// Масштабный коэффициент размера заголовка: во сколько раз h1/h2/h3
// крупнее обычного текста. Qt хранит крупность в свойстве
// FontSizeAdjustment, а оно не откатывается undo и плодит мусор,
// поэтому в работу берутся абсолютные размеры. Коэффициент замеряется
// один раз за сессию рендером эталона: допущений о внутренностях Qt нет
