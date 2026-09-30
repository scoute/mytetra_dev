#include <QtTest>

#include "libraries/OrderedMap.h"

// Тестовый класс для проверки OrderedMap.
// Проверяется порядок вставки, доступ, итераторы и удаление.

class TestOrderedMap : public QObject
{
    Q_OBJECT

private slots:

    // Проверка вставки и чтения в порядке добавления
    void insertKeepsOrder()
    {
        OrderedMap<QString, int> map;
        map.insert("b", 2);
        map.insert("a", 1);
        map.insert("c", 3);

        QCOMPARE(map.size(), 3);
        QVERIFY(!map.isEmpty());
        QCOMPARE(map.keys(), QList<QString>({"b", "a", "c"}));
        QCOMPARE(map.values(), QList<int>({2, 1, 3}));
        QCOMPARE(map.value("a"), 1);
        QVERIFY(map.contains("b"));
        QVERIFY(!map.contains("z"));
    }

    // Проверка конструктора со списком инициализации
    void initListConstructor()
    {
        OrderedMap<QString, QString> map({{"k1", "v1"}, {"k2", "v2"}});

        QCOMPARE(map.size(), 2);
        QCOMPARE(map.value("k1"), QString("v1"));
        QCOMPARE(map.value("k2"), QString("v2"));
        QCOMPARE(map.keys(), QList<QString>({"k1", "k2"}));
    }

    // Проверка перезаписи существующего ключа без дублирования порядка
    void overwriteKeepsOrder()
    {
        OrderedMap<QString, int> map;
        map.insert("a", 1);
        map.insert("b", 2);
        map.insert("a", 10);

        QCOMPARE(map.size(), 2);
        QCOMPARE(map.keys(), QList<QString>({"a", "b"}));
        QCOMPARE(map.value("a"), 10);
    }

    // Проверка удаления и очистки
    void removeAndClear()
    {
        OrderedMap<QString, int> map({{"a", 1}, {"b", 2}, {"c", 3}});

        map.remove("b");
        QCOMPARE(map.size(), 2);
        QVERIFY(!map.contains("b"));
        QCOMPARE(map.keys(), QList<QString>({"a", "c"}));

        // Удаление отсутствующего ключа не должно ничего ломать
        map.remove("missing");
        QCOMPARE(map.size(), 2);

        map.clear();
        QVERIFY(map.isEmpty());
        QCOMPARE(map.size(), 0);
        QVERIFY(map.keys().isEmpty());
    }

    // Проверка значения по умолчанию для отсутствующего ключа
    void defaultValue()
    {
        OrderedMap<QString, int> map;

        QCOMPARE(map.value("missing", -1), -1);
        QCOMPARE(map.value("missing"), 0);
    }

    // Проверка оператора доступа с автосозданием ключа
    void squareBrackets()
    {
        OrderedMap<QString, QString> map;

        map["new"] = "hello";
        QVERIFY(map.contains("new"));
        QCOMPARE(map.value("new"), QString("hello"));

        map["new"] = "world";
        QCOMPARE(map.size(), 1);
        QCOMPARE(map.value("new"), QString("world"));
    }

    // Проверка безопасного const-доступа через at()
    void atAccess()
    {
        OrderedMap<QString, int> map({{"a", 5}});

        QCOMPARE(map.at("a"), 5);

        bool thrown = false;
        try
        {
            map.at("missing");
        }
        catch (const std::out_of_range &)
        {
            thrown = true;
        }
        QVERIFY2(thrown, "at() для отсутствующего ключа должен бросать std::out_of_range");
    }

    // Проверка итераторов и поиска
    void iteratorsAndFind()
    {
        OrderedMap<QString, int> map({{"x", 1}, {"y", 2}, {"z", 3}});

        QList<QString> iteratedKeys;
        for (auto it = map.cbegin(); it != map.cend(); ++it)
        {
            iteratedKeys << (*it).first;
        }
        QCOMPARE(iteratedKeys, QList<QString>({"x", "y", "z"}));

        QVERIFY(map.find("y") != map.end());
        QCOMPARE((*map.find("y")).second, 2);
        QVERIFY(map.find("missing") == map.end());

        QCOMPARE((*map.first()).first, QString("x"));
        QCOMPARE((*map.last()).first, QString("z"));

        OrderedMap<QString, int> empty;
        QVERIFY(empty.first() == empty.end());
        QVERIFY(empty.last() == empty.end());
        QVERIFY(empty.begin() == empty.end());
    }

    // Проверка доступа к элементам через оператор ->
    void arrowAccess()
    {
        OrderedMap<QString, int> map({{"x", 1}, {"y", 2}});

        auto it = map.find("y");
        QVERIFY(it != map.end());
        QCOMPARE(it->first, QString("y"));
        QCOMPARE(it->second, 2);
    }

    // Проверка обратного обхода через декремент
    void decrementWalk()
    {
        OrderedMap<QString, int> map({{"x", 1}, {"y", 2}, {"z", 3}});

        // Префиксный декремент от конца
        auto it = map.end();
        --it;
        QCOMPARE((*it).first, QString("z"));

        // Постфиксный декремент возвращает старое значение
        auto previous = it--;
        QCOMPARE((*previous).first, QString("z"));
        QCOMPARE((*it).first, QString("y"));

        // Обход до начала
        --it;
        QCOMPARE((*it).first, QString("x"));
        QVERIFY(it == map.begin());
    }

    // Проверка операторов сравнения итераторов
    void iteratorComparisons()
    {
        OrderedMap<QString, int> map({{"x", 1}, {"y", 2}, {"z", 3}});

        auto first = map.begin();
        auto second = map.begin();
        ++second;
        auto last = map.end();

        QVERIFY(first == map.begin());
        QVERIFY(first != second);
        QVERIFY(first < second);
        QVERIFY(first <= second);
        QVERIFY(first <= map.begin());
        QVERIFY(second > first);
        QVERIFY(second >= first);
        QVERIFY(second >= second);
        QVERIFY(last > first);
        QVERIFY(last >= first);
        QVERIFY(!(first > second));
        QVERIFY(!(first >= second));
    }

    // Проверка постфиксного инкремента: возвращает старое значение
    void postIncrement()
    {
        OrderedMap<QString, int> map({{"x", 1}, {"y", 2}});

        auto it = map.begin();
        auto previous = it++;
        QCOMPARE((*previous).first, QString("x"));
        QCOMPARE((*it).first, QString("y"));
        QCOMPARE((*it).second, 2);
    }
};

QTEST_APPLESS_MAIN(TestOrderedMap)

#include "tst_orderedmap.moc"
