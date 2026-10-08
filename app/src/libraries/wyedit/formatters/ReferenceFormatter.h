#ifndef REFERENCEFORMATTER_H
#define REFERENCEFORMATTER_H

#include "Formatter.h"

class QDialog;
class QLineEdit;

// Класс форматирования ссылок


class ReferenceFormatter : public Formatter
{
    Q_OBJECT

public:
    ReferenceFormatter();

signals:

public slots:

    void onReferenceClicked(void);
    void onInsertNoteReferenceClicked(void);
    void onEditReferenceAtCursor(void);
    void onContextMenuGotoReference(void);
    void onClickedGotoReference(QString href);
    void onTextChanged(void);

private:

    // Выделить ссылку под курсором если она там есть.
    // Возвращает href или пусто. Курсор без выделения не трогается
    QString selectReferenceUnderCursor(void);

    // Защита от вставки в середину слова: буквы с обеих сторон курсора —
    // почти всегда случайный тык. Натягивает слово целиком, дальше
    // обычный сценарий с выделением. Возвращает true если натянула
    bool snapWordUnderCursor(void);

    // Имя записи по внутренней ссылке mytetra://note/<id>.
    // Пусто если запись не найдена (удалена, шифр, опечатка)
    QString targetRecordName(const QString &internalHref) const;

    // Глобальный выбор заметки в поля диалога ссылки.
    // Имя не затирается если уже введено вручную
    void pickNoteIntoFields(QDialog *parent,
                            QLineEdit *urlEdit,
                            QLineEdit *nameEdit);

    // Вставка titled-ссылки в курсор + пробел-разделитель,
    // чтобы рядом стоящие ссылки не сливались в одну
    void insertTitledLink(const QString &internalHref,
                                  const QString &targetName);

};


#endif // REFERENCEFORMATTER_H
