#ifndef CLIPPER_PRIVATE_H
#define CLIPPER_PRIVATE_H

// P3: общие хелперы Clipper между TU (были file-static)
#include <QImage>
#include <QRegularExpression>

extern int clipMaxImages;
extern qint64 clipMaxImageBytes;
QString clipImageExtension(const QImage &image);
QImage clipLimitImageSize(const QImage &image);
const QRegularExpression &browserCaptionSuffix(void);

#endif // CLIPPER_PRIVATE_H
