#ifndef _TAGSUGGESTER_H_
#define _TAGSUGGESTER_H_

#include <QList>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>

// Интеллектуальное маркирование тегами (Qt-only, без ML-зависимостей).
//
// Задача: у пользователя есть размеченные заметки (текст -> теги), он пишет
// новые без тегов. Движок находит похожие слова и предлагает теги из
// существующего списка. Никаких эмбеддингов: TF-IDF на коленке, предсказуемо
// и быстро на сотнях/тысячах заметок. Русская морфология — только нормализация
// (lowercase, ё->е); стемминга нет осознанно (предсказуемость важнее recall).
//
// Использование:
//   TagSuggester suggester;
//   suggester.addDocument(tags, text);  // по всем размеченным
//   QList<TagSuggestion> top=suggester.suggest(text, 5);

// Размеченный документ для обучения (id не нужен — только текст и теги)
struct TaggedDocument
{
    QStringList tags;
    QString text;
};

// Один вариант подсказки
struct TagSuggestion
{
    QString tag;
    double score=0.0;
    QStringList matchedWords; // Топ слов-совпадений (объяснимость)
};

class TagSuggester
{
public:
    TagSuggester() = default;

    // Добавить размеченный документ в профили тегов
    void addDocument(const QStringList &tags, const QString &text);

    // Топ-N тегов для текста (отсортированы по убыванию score).
    // minScore отсекает шум (по умолчанию 0 — вернуть всё ненулевое)
    QList<TagSuggestion> suggest(const QString &text, int topN=5,
                                 double minScore=0.0) const;

    // Число размеченных документов в профилях
    int documentCount(void) const;

    // Токенизация: lowercase, ё->е, только буквы/цифры, стоп-слова RU/EN,
    // длина >= 3, лёгкий стемминг русских окончаний (свеклу/свекла -> свекл).
    // Стемминг одинаков при обучении и подсказке, поэтому согласован.
    // Публична для тестов
    static QStringList tokenize(const QString &text);

private:
    // Лёгкий стемминг русских окончаний (см. tokenize)
    static QString stem(const QString &word);
    // Вес слова в теге: (доля документов тега со словом) * idf(слово).
    // Считается лениво при suggest (профили маленькие — дёшево)
    double wordWeight(const QString &tag, const QString &word) const;

    // Document frequency слова по всем документам (для idf)
    int docFrequency(const QString &word) const;

    // tag -> (word -> число документов тега с этим словом)
    QMap<QString, QMap<QString, int>> tagWordDocs;
    // tag -> число документов с тегом
    QMap<QString, int> tagDocCounts;
    // слово -> число документов (всех) с этим словом
    QMap<QString, int> globalWordDocs;
    int totalDocs=0;

    static const QSet<QString> &stopWords(void);
};

#endif // _TAGSUGGESTER_H_
