#include <QtTest>

#include "libraries/crypt/Pbkdf2Qt.h"

// Тестовый класс для проверки PBKDF2-HMAC-SHA1.
// Эталонные векторы взяты из RFC 6070.

class TestPbkdf2Qt : public QObject
{
    Q_OBJECT

private slots:

    // Проверка версии библиотеки
    void versionNotEmpty()
    {
        Pbkdf2Qt pbkdf2;
        QVERIFY(strlen(pbkdf2.GetVersion()) > 0);
    }

    // Проверка эталонного вектора RFC 6070: password/salt, 1 итерация
    // Значения сверены с Python hashlib.pbkdf2_hmac
    void rfc6070FirstVector()
    {
        Pbkdf2Qt pbkdf2;
        QByteArray result = pbkdf2.Pbkdf2("password", "salt", 1, 20);

        QCOMPARE(result.toHex(), QByteArray("0c60c80f961f0e71f3a9b524af6012062fe037a6"));
    }

    // Проверка эталонного вектора RFC 6070: 2 итерации
    void rfc6070TwoIterations()
    {
        Pbkdf2Qt pbkdf2;
        QByteArray result = pbkdf2.Pbkdf2("password", "salt", 2, 20);

        QCOMPARE(result.toHex(), QByteArray("ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957"));
    }

    // Проверка эталонного вектора RFC 6070: 4096 итераций
    void rfc6070SecondVector()
    {
        Pbkdf2Qt pbkdf2;
        QByteArray result = pbkdf2.Pbkdf2("password", "salt", 4096, 20);

        QCOMPARE(result.toHex(), QByteArray("4b007901b765489abead49d926f721d065a429c1"));
    }

    // Проверка детерминированности: одинаковый вход дает одинаковый выход
    void deterministic()
    {
        Pbkdf2Qt pbkdf2;
        QByteArray first = pbkdf2.Pbkdf2("mytetra", "somesalt", 100, 20);
        QByteArray second = pbkdf2.Pbkdf2("mytetra", "somesalt", 100, 20);

        QCOMPARE(first.size(), 20);
        QCOMPARE(first, second);
    }

    // Проверка чувствительности к паролю, соли и числу итераций
    void differentInputs()
    {
        Pbkdf2Qt pbkdf2;
        QByteArray base = pbkdf2.Pbkdf2("password", "salt", 10, 20);

        QVERIFY(pbkdf2.Pbkdf2("other", "salt", 10, 20) != base);
        QVERIFY(pbkdf2.Pbkdf2("password", "other", 10, 20) != base);
        QVERIFY(pbkdf2.Pbkdf2("password", "salt", 11, 20) != base);
    }

    // Проверка запрошенной длины ключа
    void keyLength()
    {
        Pbkdf2Qt pbkdf2;

        QCOMPARE(pbkdf2.Pbkdf2("password", "salt", 1, 16).size(), 16);
        QCOMPARE(pbkdf2.Pbkdf2("password", "salt", 1, 32).size(), 32);
    }

    // Проверка некорректных параметров: пустая соль, ноль итераций, нулевая длина
    void invalidParamsReturnEmpty()
    {
        Pbkdf2Qt pbkdf2;

        QVERIFY(pbkdf2.Pbkdf2("password", "", 1, 20).isEmpty());
        QVERIFY(pbkdf2.Pbkdf2("password", "salt", 0, 20).isEmpty());
        QVERIFY(pbkdf2.Pbkdf2("password", "salt", 1, 0).isEmpty());
    }

    // Проверка пароля длиннее блока SHA1 (64 байта): ключ предобрабатывается
    // хешированием, результат детерминирован и отличается от короткого пароля
    void longPassword()
    {
        Pbkdf2Qt pbkdf2;
        QByteArray longPass(100, 'p');

        QByteArray first = pbkdf2.Pbkdf2(longPass, "salt", 1, 20);
        QByteArray second = pbkdf2.Pbkdf2(longPass, "salt", 1, 20);

        QCOMPARE(first.size(), 20);
        QCOMPARE(first, second);
        QVERIFY(first != pbkdf2.Pbkdf2("password", "salt", 1, 20));
    }
};

QTEST_APPLESS_MAIN(TestPbkdf2Qt)

#include "tst_pbkdf2qt.moc"
