#ifndef SECRETFORMATTER_H
#define SECRETFORMATTER_H

#include <QString>
#include <QTextCharFormat>

#include "Formatter.h"


// Закрашиваемая область для секретов (пароли): текст и заливка одного
// цвета, снятие выделением. Своего тега в QTextEdit нет, поэтому спойлер
// это якорь с меткой: метка отличает его от вручную покрашенного текста,
// переживает сохранение (HTML) и копипаст, дает границы для копирования
// и позволяет поиску пропускать совпадения чтобы не выдавать содержимое

class SecretFormatter : public Formatter
{
  Q_OBJECT

public:

  // Метка секрета в anchorHref
  static QString secretHref(void);

  // Секрет ли формат (своя или старая метка)
  static bool isSecretFormat(const QTextCharFormat &format);

  // Цвет закраски: виден боксом в обеих темах, текст на нем скрыт
  static QColor secretColor(void);

public slots:

  // Кнопка тулбара: закрасить выделение. Без выделения только подсказка
  void onSecretClicked(void);

  // Контекстное меню: скопировать содержимое спойлера под курсором
  // в буфер без выделения
  void onContextMenuCopySecret(void);
};


#endif // SECRETFORMATTER_H
