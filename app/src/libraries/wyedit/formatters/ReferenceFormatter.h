#ifndef REFERENCEFORMATTER_H
#define REFERENCEFORMATTER_H

#include "Formatter.h"

// Класс форматирования ссылок


class ReferenceFormatter : public Formatter
{
    Q_OBJECT

public:
    ReferenceFormatter();

signals:

public slots:

    void onReferenceClicked(void);
    void onContextMenuGotoReference(void);
    void onClickedGotoReference(QString href);
    void onTextChanged(void);

private:

    // Увод курсора из середины слова в его конец перед запросом ссылки.
    // Возвращает true если курсор двигался
    bool moveCursorToWordEnd(void);

};


#endif // REFERENCEFORMATTER_H
