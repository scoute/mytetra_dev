#ifndef VOICEBUBBLEOBJECT_H
#define VOICEBUBBLEOBJECT_H

#include <QObject>
#include <QTextObjectInterface>

class AudioFormatter;

// Рисовальщик инлайн-бабла голосовой заметки в стиле мессенджера:
// кружок play/pause, имя файла, время, тонкая полоса прогресса.
// Состояние воспроизведения спрашивается у форматтера

class VoiceBubbleObject : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:

    explicit VoiceBubbleObject(AudioFormatter *formatter);

    QSizeF intrinsicSize(QTextDocument *doc, int posInDocument,
                        const QTextFormat &format) override;

    void drawObject(QPainter *painter, const QRectF &rect, QTextDocument *doc,
                    int posInDocument, const QTextFormat &format) override;

    // Левая граница бабла в координатах документа, -1 если не найти.
    // Нужна чтобы отличать клик по кружку (toggle) от клика
    // по полосе (seek). Только X: вертикаль не используется
    static qreal bubbleLeftEdge(QTextDocument *doc, int bubblePos);

    // Ширина зоны кружка от левого края бабла: клик левее
    // переключает play/pause, правее мотает
    static int toggleZoneWidth(void);

private:

    AudioFormatter *formatter;
};

#endif // VOICEBUBBLEOBJECT_H
