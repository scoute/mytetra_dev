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
#include "../EditorToolBarAssistant.h"
#include "../EditorFontFamilyComboBox.h"
#include "../EditorFontSizeComboBox.h"


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

    // Примененный шрифт — в историю использования (счетчик топ-10).
    // Комбобокс перестраивается и возвращается на текущий шрифт
    editorConfig->add_recentFont(font.family());
    editor->editorToolBarAssistant->fontSelect->reloadRecentFonts(editorConfig->get_recentFonts(),
                                                                  font.family());

    // Курсор после выбора возвращается в область редактирования
    textArea->setFocus();
}


// Слот, срабатывающий когда изменен размер шрифта через список размеров


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
// При n=1 сбрасывается цвет (нет цвета), сброс запоминается последним


// Слот, срабатыващий при нажатии на кнопки выбора цвета текста
// Параметр n - это номер выбранного пункта в выпадающей кнопке выбора цвета,
// счет кнопок с нуля.
// При n=0 нужно запустить диалог выбора цвета,
// При n=1 сбрасывается цвет (нет цвета), сброс запоминается последним
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
        // "Нет цвета": запоминается как последний (прозрачный),
        // клик по кнопке будет сбрасывать цвет, а не красить
        selectedColor = Qt::transparent;

        editorConfig->set_lastFontColor(selectedColor.name(QColor::HexArgb));

        // Меняется цвет кнопки и сбрасывается цвет текста
        this->doChangeFontcolor( selectedColor );
    }
}


// Клик по самой кнопке цвета: красит выделение в последний выбранный
// цвет без вызова пикера. Выключено настройкой — ничего не делает.
// Цвета еще нет (ни разу не выбирали) — открывается пикер


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


QChar TypefaceFormatter::pageBreakMark(void)
{
    return QChar(0x2702);
}


QString TypefaceFormatter::pageBreakMarkerLine(void)
{
    return QString(pageBreakMark())
           + QStringLiteral(" ")
           + tr("Page break")
           + QStringLiteral(" ")
           + QString(pageBreakMark());
}


// Вставка разрыва страницы в "пустой" абзац, где расположен курсор
// (пустой абзац заменяется на маркер). Если есть выделение в тексте,
// или курсор стоит на тексте, вставка не производится
void TypefaceFormatter::onInsertPageBreakClicked()
{
    // Если есть выделение в тексте, то вставка не производится
    if(textArea->textCursor().hasSelection())
        return;

    // Если курсор стоит на тексте, то вставка не производится.
    // Тот же контракт что у горизонтальной линии: сериализованный
    // <br/> из сохраненки дает символ U+2028, туда вставка не ходит
    QTextCursor probe=textArea->textCursor();
    probe.select(QTextCursor::LineUnderCursor);
    if(!probe.selectedText().isNull())
        return;

    // Работаем с блоком курсора: пустой абзац превращается в маркер.
    // insertHtml("<p ...>") здесь не годится: фрагмент сливается
    // с текущим блоком и page-break теряется (проверено пробой)
    QTextCursor edit=textArea->textCursor();
    edit.movePosition(QTextCursor::StartOfBlock);
    edit.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    textArea->setTextCursor(edit);
    textArea->textCursor().removeSelectedText();

    QTextBlockFormat breakFormat;
    breakFormat.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysAfter);
    breakFormat.setAlignment(Qt::AlignHCenter);
    breakFormat.setBackground(QColor(QStringLiteral("#e8e8e8")));
    textArea->textCursor().setBlockFormat(breakFormat);

    QTextCharFormat markFormat;
    markFormat.setForeground(QColor(QStringLiteral("#808080")));
    textArea->textCursor().insertText(pageBreakMarkerLine(), markFormat);

    textArea->moveCursor(QTextCursor::Down);
    textArea->moveCursor(QTextCursor::StartOfLine);
}


// Чистка визуала разрывов перед печатью: текст и фон убираются,
// пустой абзац с page-break остается и рвет страницу. Чужие
// page-break без маркера не трогаются
void TypefaceFormatter::stripPageBreaksForPrint(QTextDocument *doc)
{
    if(doc==nullptr)
        return;

    QTextBlock block=doc->begin();

    while(block.isValid())
    {
        const bool hasBreak=(block.blockFormat().pageBreakPolicy()!=QTextFormat::PageBreak_Auto);

        if(hasBreak && block.text().contains(pageBreakMark()))
        {
            // Наш маркер: стирается только текст блока (до конца блока,
            // разделитель не трогаем - иначе блок склеится со следующим
            // и разрыв уедет). Пустой абзац с page-break остается
            // и рвет страницу на печати
            QTextCursor cursor(block);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();

            QTextBlockFormat clean;
            clean.setPageBreakPolicy(block.blockFormat().pageBreakPolicy());
            cursor.setBlockFormat(clean);
        }

        block=block.next();
    }
}


// Обработка мягкого переноса
// Учитываются мягкие переносы до выделенного текста (1-й символ до выделения),
// после выделенного текста и в выделенных абзацах


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


void TypefaceFormatter::doChangeFontcolor(const QColor &selectedColor)
{
    // TRACELOG

    // Прозрачный цвет означает "Нет цвета": foreground сбрасывается,
    // а не красится (как сброс фона в doChangeBackgroundColor)
    if(selectedColor.alpha()==0)
    {
        textArea->textCursor().beginEditBlock();

        this->clearForegroundInSelection();

        textArea->textCursor().endEditBlock();
    }
    // Если выделение есть
    else if ( textArea->textCursor().hasSelection() )
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

    // Подчеркивание тоже не трогаем: setFont() несет underline=false
    // по умолчанию и гасил бы его у ссылки-кнопки [copy] в маркере
    // (она живет без подчеркивания, см. normalizeCopyLinkUnderline)
    format.clearProperty(QTextFormat::FontUnderline);

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
