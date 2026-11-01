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


// Форматирование Bold
void TypefaceFormatter::onBoldClicked(void)
{
    // TRACELOG

    smartFormat(Bold);
}


// Форматирование Italic


// Форматирование Italic
void TypefaceFormatter::onItalicClicked(void)
{
    // TRACELOG

    smartFormat(Italic);
}


// Форматирование подчеркивания


// Форматирование подчеркивания
void TypefaceFormatter::onUnderlineClicked(void)
{
    // TRACELOG

    smartFormat(Underline);
}


// Форматирование Strike


// Форматирование Strike
void TypefaceFormatter::onStrikeOutClicked(void)
{
    // TRACELOG

    smartFormat(StrikeOut);
}


// Форматирование SuperScript


// Форматирование SuperScript
void TypefaceFormatter::onSuperScriptClicked(void)
{
    // TRACELOG

    smartFormat(SuperScript);
}


// Форматирование SubScript


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
