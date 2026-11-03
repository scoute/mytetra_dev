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

    // Перемещение метки аттача в другую заметку: файл уезжает
    // в таблицу приемника, ссылка дописывается в его текст,
    // из исходника метка и аттач удаляются. Явное действие
    // по ПКМ с подтверждением, отмена Ctrl+Z его не откатывает
    void onMoveAttachClicked(void);

    // Метка брошенного файла: иконка скрепки + видимое имя
    // с расширением, href вида attach://<внутреннее имя>.
    // Вставляется как обычная titled-ссылка со своим цветом
    void insertAttachBadge(const QString &innerFileName,
                           const QString &displayFileName);

    // Метка аттача: свой канал attach://<внутреннее имя>.
    // Отдельно от относительных ссылок, чтобы клик без Ctrl
    // открывал только метки, а не любой относительный файл
    static bool isAttachHref(const QString &href);
    static QString attachInnerFromHref(const QString &href);

    // Имя MIME для переноса меток аттачей между заметками.
    // Пары href-абсолютный путь, как голосовой MIME у баблов
    static const char *attachMimeTypeName(void);

    // Абзац метки для дописки в текст закрытой заметки:
    // ссылка attach:// плюс [EXT] видимое имя
    static QString badgeParagraphHtml(const QString &innerName,
                                      const QString &displayName);

    // Дописка метки в текстовый файл закрытой заметки.
    // Вставка перед </body> (как голосовые ссылки), файла нет —
    // минимальный каркас. Не влезло: false и текст ошибки
    static bool appendAttachBadgeToTextFile(const QString &textFileName,
                                           const QString &innerName,
                                           const QString &displayName,
                                           QString *error);

private:

    // Выделить ссылку под курсором если она там есть.
    // Возвращает href или пусто. Курсор без выделения не трогается
    QString selectReferenceUnderCursor(void);

    // Метка аттача под курсором, пусто если курсор не на метке
    QString attachHrefUnderCursor(void);

    // Выделить метку с href в месте курсора. Нужно для удаления
    // метки из исходника после переноса файла в другую заметку
    bool selectAttachBadgeRange(const QString &href);

    // Защита от вставки в середину слова: буквы с обеих сторон курсора —
    // почти всегда случайный тык. Уводить курсор в конец слова,
    // чтобы слово осталось целым, а ссылка легла рядом со своим именем.
    // Возвращает true если подвинула
    bool moveCursorToWordEnd(void);

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
