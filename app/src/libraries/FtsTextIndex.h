#ifndef _FTSTEXTINDEX_H_
#define _FTSTEXTINDEX_H_

#include <QObject>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QSqlDatabase>

#include <functional>

class KnowTreeModel;
class TreeItem;

// FTS5-индекс текстов записей для быстрого полнотекстового поиска.
// Файл индекса лежит рядом с conf.ini (рабочая директория), ВНЕ базы:
// git базы остается чистым, индекс это локальный кеш.
//
// Схема обхода отсутствия триграммного токенизатора в SQLite из Qt:
// индексируется склейка триграмм нормализованного текста
// (только буквы/цифры, регистр отдает FTS), запрос это фраза
// из триграмм слова. Склейка монотонна (удаление символов),
// поэтому кандидаты FTS всегда надмножество совпадений, а точное
// совпадение проверяется существующим findInTextProcess.
// Шифрованные записи не индексируются никогда (утечка at rest).

class FtsTextIndex : public QObject
{
    Q_OBJECT

public:

    explicit FtsTextIndex(QObject *parent=nullptr);
    virtual ~FtsTextIndex(void);

    // Путь к файлу индекса: рабочая директория (там же conf.ini)
    static QString indexFilePath(void);

    // HTML записи в плоский текст, та же конвертация что в поиске
    static QString plainFromHtml(const QString &html);

    // Слово запроса в кусок MATCH-запроса (фраза из триграмм без кавычек).
    // Пустая строка если слово для FTS непригодно (после нормализации
    // меньше 3 букв/цифр): тогда только линейный поиск
    static QString wordGrams(const QString &word);

    // Число шифрованных записей в базе: они никогда не индексируются,
    // изменение их числа делает индекс несвежим. Обход только по флагам,
    // без чтения файлов текстов
    static int cryptRecordCount(KnowTreeModel *model);

    // Открыть/создать базу и схему. Повторный вызов дешев.
    // false только при битой недоступности SQLite, индекс тогда
    // молча не используется, поиск идет линейно
    bool ensureOpen(void);

    // Свежесть индекса против текущей базы. Причина расхождения
    // возвращается текстом для вопроса пользователю
    bool isFresh(const QString &tetradir,
                 qint64 xmlSize,
                 qint64 xmlMtime,
                 int recordCount,
                 int cryptCount,
                 QString *reason=nullptr);

    // Полная пересборка по модели дерева. progress(done,total)
    // возвращает false для отмены. true если собрано до конца.
    // Шифрованные записи пропускаются всегда
    bool rebuildFull(KnowTreeModel *model,
                     std::function<bool(int,int)> progress);

    // Обновить/удалить одну запись. htmlText в исходном виде (HTML),
    // конвертация внутри. Пустой текст удаляет строку.
    // Шифрованные записи сюда передавать нельзя: их отсутствие
    // в индексе отмечается вызовом remove
    void upsert(const QString &recordId, const QString &htmlText);
    void remove(const QString &recordId);

    // Id записей-кандидатов по словам запроса. matchAll=false это ИЛИ.
    // ok=false если индекс неприменим: поиск идет линейно
    QSet<QString> search(const QStringList &words, bool matchAll, bool *ok);

    // Число проиндексированных записей, для диагностики
    int indexedCount(void);

    void close(void);

private:

    // Плоский текст в строку триграмм через пробел для FTS5 unicode61.
    // Оставляются только буквы и цифры, остальное выброшено
    static QString gramsOf(const QString &plain);

    // Обход ветки для пересборки: только флаги и тексты,
    // без участия поиска
    void rebuildBranch(const TreeItem *curritem,
                       int &done,
                       int total,
                       std::function<bool(int,int)> &progress,
                       bool &cancelled);

    bool createSchema(void);
    bool writeMeta(const QMap<QString, QString> &meta);
    QMap<QString, QString> readMeta(void);

    bool opened=false;
    QSqlDatabase db;

    // Флаг идущей пересборки и факт правок в ее ходе: правка во время
    // сборки может потеряться (обход прочитал файл раньше записи),
    // тогда мета пишется грязной и следующий поиск пересоберет заново
    bool rebuildRunning=false;
    bool touchedDuringRebuild=false;
};

#endif /* _FTSTEXTINDEX_H_ */
