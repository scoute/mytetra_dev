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

private:

    AudioFormatter *formatter;
};

#endif // VOICEBUBBLEOBJECT_H
