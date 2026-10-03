#include "TagSuggester.h"

#include <algorithm>
#include <QtMath>


// Стоп-слова RU/EN: мусор, не несущий тематики
const QSet<QString> &TagSuggester::stopWords(void)
{
    static QSet<QString> words;
    if(words.isEmpty())
    {
        const char *stop[]={
            // Русские
            "это", "как", "так", "или", "при", "для", "что", "все", "всё",
            "его", "ее", "её", "их", "вам", "вас", "мне", "меня", "нас",
            "они", "оно", "она", "оно", "был", "была", "были", "было",
            "есть", "нет", "даже", "если", "когда", "где", "куда", "откуда",
            "можно", "нельзя", "надо", "нужно", "просто", "очень", "самый",
            "такой", "такая", "такие", "этот", "этого", "этой", "которые",
            "который", "которая", "которые", "между", "через", "после",
            "перед", "над", "под", "без", "про", "уже", "еще", "ещё",
            "только", "также", "именно", "почему", "потому", "поэтому",
            "однако", "хотя", "чтобы", "меня", "себя", "тебя", "него",
            // Английские
            "the", "and", "for", "with", "from", "that", "this", "have",
            "has", "had", "were", "was", "are", "but", "not", "you",
            "your", "our", "their", "they", "them", "then", "than",
            "will", "would", "could", "should", "about", "into", "over",
            "after", "before", "between", "more", "most", "some", "such",
            "only", "also", "very", "just", "into", "out", "own", "same",
            "can", "all", "any", "each", "few", "how", "its", "may",
            "new", "now", "old", "see", "two", "who", "why", "one",
            nullptr
        };
        for(int i=0; stop[i]; ++i)
            words.insert(QString::fromLatin1(stop[i]));
    }
    return words;
}


QStringList TagSuggester::tokenize(const QString &text)
{
    QString normalized=text.toLower();
    normalized.replace(QChar(0x0451), QChar(0x0435)); // ё -> е

    QStringList tokens;
    QString current;
    for(QChar ch : normalized)
    {
        if(ch.isLetterOrNumber())
            current+=ch;
        else if(!current.isEmpty())
        {
            if(current.size()>=3 && !stopWords().contains(current))
                tokens.append(stem(current));
            current.clear();
        }
    }
    if(!current.isEmpty() && current.size()>=3 && !stopWords().contains(current))
        tokens.append(stem(current));

    return tokens;
}


// Лёгкий стемминг русских окончаний: грубо сводит словоформы
// (проекта/проекту/проектом -> проект). Только кириллица, только если
// остаётся >= 3 символов. Перестемминг возможен, но для ранжирования
// тегов приемлем: обучение и подсказка стеммятся одинаково
QString TagSuggester::stem(const QString &word)
{
    static const char *endings[]={
        "иями", "ями", "ами",
        "ов", "ев", "ей", "ой", "ью", "ию",
        "ам", "ям", "ом", "ем", "ах", "ях",
        "а", "я", "у", "ю", "о", "е", "ы", "и", "й", "ь",
        nullptr
    };

    bool cyrillic=true;
    for(QChar ch : word)
    {
        const ushort code=ch.unicode();
        if(code<0x0430 || code>0x0451) // а..я, ё
        {
            cyrillic=false;
            break;
        }
    }
    if(!cyrillic || word.size()<=3)
        return word;

    const QString w=word;
    for(int i=0; endings[i]; ++i)
    {
        const QString ending=QString::fromUtf8(endings[i]);
        if(w.endsWith(ending) && w.size()-ending.size()>=3)
            return w.left(w.size()-ending.size());
    }
    return word;
}


void TagSuggester::addDocument(const QStringList &tags, const QString &text)
{
    // Уникальные слова документа (частотность внутри документа не учитываем —
    // короткие заметки, бинарное присутствие устойчивее)
    QSet<QString> words;
    for(const QString &token : tokenize(text))
        words.insert(token);

    // Уникальные теги документа
    QSet<QString> uniqueTags;
    for(QString tag : tags)
    {
        tag=tag.trimmed().toLower();
        if(!tag.isEmpty())
            uniqueTags.insert(tag);
    }
    if(uniqueTags.isEmpty() || words.isEmpty())
        return;

    totalDocs++;

    for(const QString &word : words)
        globalWordDocs[word]++;

    for(const QString &tag : uniqueTags)
    {
        tagDocCounts[tag]++;
        for(const QString &word : words)
            tagWordDocs[tag][word]++;
    }
}


int TagSuggester::documentCount(void) const
{
    return totalDocs;
}


int TagSuggester::docFrequency(const QString &word) const
{
    return globalWordDocs.value(word, 0);
}


double TagSuggester::wordWeight(const QString &tag, const QString &word) const
{
    const int tagDocs=tagDocCounts.value(tag, 0);
    if(tagDocs==0)
        return 0.0;

    const int wordInTagDocs=tagWordDocs.value(tag).value(word, 0);
    if(wordInTagDocs==0)
        return 0.0;

    // TF (внутри тега) * IDF (редкость по базе), сглаживание +1
    const double tf=static_cast<double>(wordInTagDocs)/tagDocs;
    const double idf=qLn(1.0+static_cast<double>(totalDocs)
                         /(1.0+docFrequency(word)));
    return tf*idf;
}


QList<TagSuggestion> TagSuggester::suggest(const QString &text, int topN,
                                           double minScore) const
{
    QList<TagSuggestion> result;

    QSet<QString> words;
    for(const QString &token : tokenize(text))
        words.insert(token);
    if(words.isEmpty() || tagDocCounts.isEmpty())
        return result;

    for(auto tagIt=tagDocCounts.constBegin(); tagIt!=tagDocCounts.constEnd(); ++tagIt)
    {
        const QString &tag=tagIt.key();

        double score=0.0;
        // Слово -> вклад, для объяснимости топ-3
        QList<QPair<QString, double>> contributions;
        for(const QString &word : words)
        {
            const double weight=wordWeight(tag, word);
            if(weight>0.0)
            {
                score+=weight;
                contributions.append(QPair<QString, double>(word, weight));
            }
        }

        if(score<=minScore)
            continue;

        std::sort(contributions.begin(), contributions.end(),
                  [](const QPair<QString, double> &a, const QPair<QString, double> &b)
                  { return a.second>b.second; });

        TagSuggestion suggestion;
        suggestion.tag=tag;
        suggestion.score=score;
        for(int i=0; i<qMin(3, contributions.size()); ++i)
            suggestion.matchedWords.append(contributions.at(i).first);
        result.append(suggestion);
    }

    std::sort(result.begin(), result.end(),
              [](const TagSuggestion &a, const TagSuggestion &b)
              { return a.score>b.score; });

    if(topN>0 && result.size()>topN)
        result=result.mid(0, topN);

    return result;
}
