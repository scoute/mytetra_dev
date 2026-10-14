#include <QPainter>
#include <QFontMetrics>
#include <QApplication>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QAbstractTextDocumentLayout>

#include "VoiceBubbleObject.h"
#include "AudioFormatter.h"
#include "libraries/VoiceNoteHelper.h"


// Геометрия бабла: высота строкой, кружок слева,
// справа колонка имени и времени, снизу полоса прогресса
static const int bubbleHeight=40;
static const int bubblePad=6;
static const int circleDiameter=28;
static const int bubbleMaxTextWidth=220;
static const int progressHeight=3;

// Бабл в полтора раза длиннее текстовой части: имя и прогресс дышат
static const qreal bubbleLengthFactor=1.5;


VoiceBubbleObject::VoiceBubbleObject(AudioFormatter *formatter) : formatter(formatter)
{

}


int VoiceBubbleObject::toggleZoneWidth(void)
{
    // Кружок плюс поля по бокам: совпадает с отрисовкой
    return bubblePad+circleDiameter+bubblePad;
}


qreal VoiceBubbleObject::bubbleLeftEdge(QTextDocument *doc, int bubblePos)
{
    if(doc==nullptr || bubblePos<0 || bubblePos>=doc->characterCount())
        return -1.0;

    QTextBlock block=doc->findBlock(bubblePos);

    if(!block.isValid() || block.layout()==nullptr)
        return -1.0;

    const int relative=bubblePos-block.position();
    const QRectF blockRect=doc->documentLayout()->blockBoundingRect(block);

    for(int i=0; i<block.layout()->lineCount(); ++i)
    {
        const QTextLine line=block.layout()->lineAt(i);

        if(relative>=line.textStart() &&
           relative<line.textStart()+line.textLength())
            return blockRect.left()+line.cursorToX(relative);
    }

    return -1.0;
}


QSizeF VoiceBubbleObject::intrinsicSize(QTextDocument *doc, int posInDocument,
                                        const QTextFormat &format)
{
    Q_UNUSED(posInDocument);

    const QString fileName=format.property(VoiceNoteHelper::BubblePropFileName).toString();

    QFont nameFont=doc!=nullptr ? doc->defaultFont() : QApplication::font();
    const int nameWidth=QFontMetrics(nameFont).horizontalAdvance(fileName);

    // Время вида "0:07 / 0:37" меряется тем же шрифтом с запасом
    const int timeWidth=QFontMetrics(nameFont).horizontalAdvance(QStringLiteral("000:00 / 000:00"));

    const int textWidth=qMin(bubbleMaxTextWidth, qMax(nameWidth, timeWidth));

    return QSizeF((bubblePad+circleDiameter+bubblePad+textWidth+bubblePad)*
                  bubbleLengthFactor,
                  bubbleHeight);
}


void VoiceBubbleObject::drawObject(QPainter *painter, const QRectF &rect, QTextDocument *doc,
                                   int posInDocument, const QTextFormat &format)
{
    Q_UNUSED(doc);
    Q_UNUSED(posInDocument);

    const QString href=format.property(VoiceNoteHelper::BubblePropHref).toString();
    const QString fileName=format.property(VoiceNoteHelper::BubblePropFileName).toString();
    const qint64 storedMs=format.property(VoiceNoteHelper::BubblePropDurationMs).toLongLong();

    bool playing=false;
    qint64 posMs=0;
    qint64 totalMs=storedMs;

    if(formatter!=nullptr)
        formatter->bubblePlaybackState(href, playing, posMs, totalMs);

    if(totalMs<=0)
        totalMs=storedMs;

    const QPalette palette=QApplication::palette();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Тело бабла: скругленный прямоугольник в цветах палитры,
    // поэтому работает и в темных, и в кастомных темах
    QRectF body=rect.adjusted(0, 0, 0, 0);
    body.setHeight(bubbleHeight);

    painter->setPen(palette.color(QPalette::Mid));
    painter->setBrush(palette.color(QPalette::AlternateBase));
    painter->drawRoundedRect(body, 12, 12);

    // Кружок кнопки: цвет выделения темы, глиф контрастный к нему
    const QRectF circle(body.left()+bubblePad,
                        body.top()+(bubbleHeight-circleDiameter)/2,
                        circleDiameter, circleDiameter);

    painter->setPen(Qt::NoPen);
    painter->setBrush(palette.color(QPalette::Highlight));
    painter->drawEllipse(circle);

    painter->setBrush(palette.color(QPalette::HighlightedText));

    if(playing)
    {
        // Пауза: две планки
        const qreal barW=circleDiameter*0.16;
        const qreal barH=circleDiameter*0.42;
        const qreal cx=circle.center().x();
        const qreal cy=circle.center().y();
        painter->drawRect(QRectF(cx-barW-1, cy-barH/2, barW, barH));
        painter->drawRect(QRectF(cx+1, cy-barH/2, barW, barH));
    }
    else
    {
        // Треугольник play
        const QPointF center=circle.center();
        const qreal size=circleDiameter*0.30;
        QPolygonF triangle;
        triangle << QPointF(center.x()-size*0.6, center.y()-size)
                 << QPointF(center.x()-size*0.6, center.y()+size)
                 << QPointF(center.x()+size*0.9, center.y());
        painter->drawPolygon(triangle);
    }

    // Тексты: имя сверху, время снизу приглушенным
    const qreal textX=circle.right()+bubblePad;
    const qreal textW=body.right()-bubblePad-textX;

    QFont nameFont=painter->font();
    painter->setPen(palette.color(QPalette::WindowText));
    painter->setFont(nameFont);

    const QString nameShown=QFontMetrics(nameFont).elidedText(fileName, Qt::ElideMiddle,
                                                              static_cast<int>(textW));
    painter->drawText(QRectF(textX, body.top()+3, textW, bubbleHeight/2),
                      Qt::AlignLeft | Qt::AlignVCenter, nameShown);

    QFont timeFont(nameFont);
    timeFont.setPointSize(qMax(1, nameFont.pointSize()-2));
    painter->setFont(timeFont);
    painter->setPen(palette.color(QPalette::WindowText));

    QString timeText;

    if(playing && totalMs>0)
        timeText=VoiceNoteHelper::formatDuration(posMs)+
                 QStringLiteral(" / ")+
                 VoiceNoteHelper::formatDuration(totalMs);
    else
        timeText=VoiceNoteHelper::formatDuration(totalMs);

    painter->setOpacity(0.65);
    painter->drawText(QRectF(textX, body.top()+bubbleHeight/2-2, textW, bubbleHeight/2),
                      Qt::AlignLeft | Qt::AlignVCenter, timeText);
    painter->setOpacity(1.0);

    // Прогресс: тонкая полоса по низу бабла
    if(totalMs>0 && posMs>0)
    {
        const qreal fraction=qBound(0.0, static_cast<qreal>(posMs)/static_cast<qreal>(totalMs), 1.0);
        const QRectF track(textX, body.bottom()-progressHeight-3,
                           textW, progressHeight);

        painter->setPen(Qt::NoPen);
        painter->setBrush(palette.color(QPalette::Mid));
        painter->drawRoundedRect(track, 1, 1);

        if(fraction>0.0)
        {
            painter->setBrush(palette.color(QPalette::Highlight));
            painter->drawRoundedRect(QRectF(track.left(), track.top(),
                                            track.width()*fraction, track.height()), 1, 1);
        }
    }

    painter->restore();
}
