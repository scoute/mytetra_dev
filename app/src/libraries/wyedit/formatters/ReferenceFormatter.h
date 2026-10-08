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

    // Имя записи по внутренней ссылке mytetra://note/<id>.
    // Пусто если запись не найдена (удалена, шифр, опечатка)
    QString targetRecordName(const QString &internalHref) const;

    // Вставка titled-ссылки в курсор + пробел-разделитель,
    // чтобы рядом стоящие ссылки не сливались в одну
    void insertTitledLink(const QString &internalHref,
                                  const QString &targetName);

};


#endif // REFERENCEFORMATTER_H
