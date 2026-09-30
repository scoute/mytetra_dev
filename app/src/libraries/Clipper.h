#ifndef _CLIPPER_H_
#define _CLIPPER_H_

#include <QString>

class QMimeData;
class KnowTreeModel;

// Клиппер: создание заметки из содержимого системного буфера обмена
// в служебной ветке верхнего уровня. Запускается горячей клавишей ОС
// через mytetra --control=--clipboard или из меню Tools

class Clipper
{
public:

    // Имя служебной ветки, в которую складываются вырезки.
    // Ветка создается автоматически при первом клипе, если ее нет.
    // Поиск идет по имени по всему дереву, поэтому переименование
    // ветки пользователем приведет к созданию новой ветки
    static QString clipboardBranchName(void);

    // Проверка, похожа ли строка на http(s) ссылку
    static bool looksLikeUrl(const QString &value);

    // Поиск первой http(s) ссылки в тексте, пустая строка если ссылки нет.
    // У концевого мусора (точки, скобки, кавычки) отрезается хвост
    static QString extractUrl(const QString &text);

    // Имя заметки по тексту: первая непустая строка, обрезанная
    // до разумной длины. Если текста нет, имя по текущей дате
    static QString makeNoteName(const QString &plainText);

    // HTML для тела заметки: готовый HTML из буфера, иначе
    // экранированный plain text с переносами строк
    static QString buildNoteHtml(const QMimeData *mime);

    // Поиск ветки для вырезок по имени, создание ветки верхнего уровня
    // если она отсутствует. Возвращает идентификатор ветки.
    // Сохранение дерева на диск остается на вызывающей стороне
    static QString ensureClipboardBranch(KnowTreeModel *model);

    // Полный цикл: прочитать буфер обмена и создать заметку в ветке
    // для вырезок. Необязательный urlHint (из --control=--clipboard --url)
    // имеет приоритет над ссылкой, найденной в тексте.
    // Возвращает true если заметка создана
    static bool clipFromClipboard(const QString &urlHint=QString());

private:

    // Запрет создания экземпляров, только статические методы
    Clipper(void);
};

#endif // _CLIPPER_H_
