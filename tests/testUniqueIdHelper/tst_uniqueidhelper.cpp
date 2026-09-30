#include <QtTest>

#include <QSet>

#include "libraries/helpers/UniqueIdHelper.h"

// Тестовый класс для проверки генератора уникальных идентификаторов.

class TestUniqueIdHelper : public QObject
{
    Q_OBJECT

private slots:

    // Проверка формата идентификатора: 20 символов, 10 цифр + 10 символов [0-9a-z]
    void idFormat()
    {
        QString id = getUniqueId();

        QCOMPARE(id.length(), 20);

        QString secondsPart = id.left(10);
        QString randomPart = id.mid(10, 10);

        foreach (QChar ch, secondsPart)
        {
            QVERIFY2(ch.isDigit(), qPrintable(QString("Символ времени не цифра: %1 в %2").arg(ch).arg(id)));
        }

        QString allowed = "0123456789abcdefghijklmnopqrstuvwxyz";
        foreach (QChar ch, randomPart)
        {
            QVERIFY2(allowed.contains(ch), qPrintable(QString("Недопустимый символ: %1 в %2").arg(ch).arg(id)));
        }
    }

    // Проверка уникальности идентификаторов в серии
    void idUniqueness()
    {
        QSet<QString> ids;
        const int count = 1000;
        for (int i = 0; i < count; ++i)
        {
            ids << getUniqueId();
        }

        QCOMPARE(ids.size(), count);
    }

    // Проверка формата имени картинки
    void imageNameFormat()
    {
        QString name = getUniqueImageName();

        QVERIFY2(name.startsWith("image"), qPrintable(name));
        QVERIFY2(name.endsWith(".png"), qPrintable(name));
        // "image" (5) + id (20) + ".png" (4) = 29 символов
        QCOMPARE(name.length(), 29);
    }

    // Проверка уникальности имен картинок
    void imageNameUniqueness()
    {
        QSet<QString> names;
        const int count = 200;
        for (int i = 0; i < count; ++i)
        {
            names << getUniqueImageName();
        }

        QCOMPARE(names.size(), count);
    }
};

QTEST_APPLESS_MAIN(TestUniqueIdHelper)

#include "tst_uniqueidhelper.moc"
