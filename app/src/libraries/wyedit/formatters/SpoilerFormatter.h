#ifndef SPOILERFORMATTER_H
#define SPOILERFORMATTER_H

#include <QString>
#include <QTextCharFormat>

#include "Formatter.h"


// Закрашиваемая область для секретов (пароли): текст и заливка одного
// цвета, снятие выделением. Своего тега в QTextEdit нет, поэтому спойлер
// это якорь с меткой: метка отличает его от вручную покрашенного текста,
// переживает сохранение (HTML) и копипаст, дает границы для копирования
// и позволяет поиску пропускать совпадения чтобы не выдавать содержимое

class SpoilerFormatter : public Formatter
{
  Q_OBJECT

public:

  // Метка спойлера в anchorHref
  static QString spoilerHref(void);

  // Спойлер ли формат (метка совпала)
  static bool isSpoilerFormat(const QTextCharFormat &format);

  // Цвет закраски: виден боксом в обеих темах, текст на нем скрыт
  static QColor spoilerColor(void);

public slots:

  // Кнопка тулбара: закрасить выделение. Без выделения только подсказка
  void onSpoilerClicked(void);

  // Контекстное меню: скопировать содержимое спойлера под курсором
  // в буфер без выделения
  void onContextMenuCopySpoiler(void);
};


#endif // SPOILERFORMATTER_H
