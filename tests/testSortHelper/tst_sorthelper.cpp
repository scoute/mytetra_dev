#include <QtTest>

#include <QStringList>
#include <algorithm>

#include "libraries/helpers/SortHelper.h"

// Тестовый класс для проверки компаратора сортировки списков строк.

class TestSortHelper : public QObject
{
    Q_OBJECT

private slots:

    // Проверка прямого сравнения списков разной длины
    void compareByLength()
    {
        QStringList shortList({"a"});
        QStringList longList({"a", "b", "c"});

        QVERIFY(compareQStringListLen(shortList, longList));
        QVERIFY(!compareQStringListLen(longList, shortList));
    }

    // Проверка списков равной длины: ни один не меньше другого
    void compareEqualLength()
    {
        QStringList list1({"a", "b"});
        QStringList list2({"c", "d"});

        QVERIFY(!compareQStringListLen(list1, list2));
        QVERIFY(!compareQStringListLen(list2, list1));
    }

    // Проверка пустых списков
    void compareEmpty()
    {
        QStringList empty1;
        QStringList empty2;
        QStringList notEmpty({"x"});

        QVERIFY(!compareQStringListLen(empty1, empty2));
        QVERIFY(compareQStringListLen(empty1, notEmpty));
        QVERIFY(!compareQStringListLen(notEmpty, empty1));
    }

    // Проверка использования компаратора в сортировке
    void sortWithComparator()
    {
        QList<QStringList> data;
        data << QStringList({"a", "b", "c"})
             << QStringList({"a"})
             << QStringList({"a", "b"});

        std::sort(data.begin(), data.end(), compareQStringListLen);

        QCOMPARE(data.at(0).size(), 1);
        QCOMPARE(data.at(1).size(), 2);
        QCOMPARE(data.at(2).size(), 3);
    }
};

QTEST_APPLESS_MAIN(TestSortHelper)

#include "tst_sorthelper.moc"
