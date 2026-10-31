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


void TypefaceFormatter::clearForegroundInSimpleSelection(const QTextCursor &cursor)
{
    QTextDocument *doc = textArea->document();
    const int selStart = cursor.selectionStart();
    const int selEnd   = cursor.selectionEnd();

    // Свойство именно снимается: mergeCharFormat неустановленное
    // свойство не перезаписывает, поэтому формат фрагмента
    // копируется, чистится и кладется обратно целиком
    QTextBlock block = doc->findBlock(selStart);

    while(block.isValid() && block.position()<selEnd)
    {
        for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
        {
            QTextFragment fragment=it.fragment();

            const int fragStart=fragment.position();
            const int fragEnd=fragStart+fragment.length();

            if(fragEnd<=selStart)
                continue;

            if(fragStart>=selEnd)
                break;

            QTextCharFormat format=fragment.charFormat();

            if(!format.hasProperty(QTextFormat::ForegroundBrush))
                continue;

            format.clearProperty(QTextFormat::ForegroundBrush);

            QTextCursor fragmentCursor(doc);
            fragmentCursor.setPosition(qMax(fragStart, selStart));
            fragmentCursor.setPosition(qMin(fragEnd, selEnd), QTextCursor::KeepAnchor);
            fragmentCursor.setCharFormat(format);
        }

        block=block.next();
    }
}


void TypefaceFormatter::clearForegroundInSelection()
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
        this->clearForegroundInSimpleSelection(cursor);
    }
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


// Обход документа: ищутся таблицы и чистятся в них ячейки, а так же чистятся сами таблицы
void TypefaceFormatter::clearTableCellsBackground(QTextDocument *doc, int selStart, int selEnd)
{
    QTextFrame *root = doc->rootFrame();
    findAndClearTables(root, selStart, selEnd);
}


// Рекурсивный обход фреймов, таблицы — это тоже QTextFrame-наследники


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
