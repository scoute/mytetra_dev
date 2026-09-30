#include <QtTest>

#include <vector>

#include <QApplication>
#include <QFile>
#include <QTemporaryDir>

#include "libraries/crypt/CryptService.h"
#include "libraries/crypt/RC5Simple.h"

// Заглушка для criticalError из DebugHelper.
// В тестах файловые методы не вызываются, поэтому
// заглушка никогда не должна срабатывать. Она нужна
// только для линковки CryptService.cpp без DebugHelper.cpp,
// который тянет глобалы приложения.

void criticalError(QString message)
{
    qCritical() << "Unexpected criticalError in test:" << message;
    QFAIL(qPrintable(QString("Неожиданный вызов criticalError: %1").arg(message)));
    abort();
}

// Тестовый класс для проверки CryptService в памяти.
// Файловые методы encryptFile/decryptFile не тестируются:
// они показывают курсор ожидания через QApplication и
// требуют диска, их очередь во второй фазе с harness.

class TestCryptService : public QObject
{
    Q_OBJECT

private slots:

    // Проверка предусловия всех тестов: RC5 принимает ключ ровно RC5_B байт.
    // Ключ неверной длины RC5_SetKey молча игнорирует (оставляет нулевой ключ),
    // поэтому все ключи в тестах должны быть ровно такой длины.
    void keySizeRequirement()
    {
        QCOMPARE(static_cast<int>(RC5_B), 16);
    }

    // Проверка преобразования QByteArray <-> vector
    void convertRoundTrip()
    {
        QByteArray source("abc\x00\xff\x41", 6);
        std::vector<unsigned char> vec;
        CryptService::convertByteArrayToVector(source, vec);
        QCOMPARE(static_cast<int>(vec.size()), source.size());

        QByteArray restored;
        CryptService::convertVectorToByteArray(vec, restored);
        QCOMPARE(restored, source);
    }

    // Проверка преобразования пустого вектора в массив
    void convertEmpty()
    {
        // Прямое направление с пустым входом не вызываем:
        // convertByteArrayToVector обращается к &vec[0],
        // что для пустого вектора небезопасно.
        std::vector<unsigned char> emptyVec;
        QByteArray restored;
        CryptService::convertVectorToByteArray(emptyVec, restored);
        QVERIFY(restored.isEmpty());
    }

    // Проверка кругового шифрования строки
    void stringRoundTrip()
    {
        QByteArray key("0123456789abcdef");
        QString plain = "Hello, MyTetra!";

        QString crypted = CryptService::encryptString(key, plain);
        QVERIFY(!crypted.isEmpty());
        QVERIFY(crypted != plain);
        QCOMPARE(CryptService::decryptString(key, crypted), plain);
    }

    // Проверка строк с UTF-8, включая русский текст
    void stringRoundTripUtf8()
    {
        // Ключ обязан быть ровно RC5_B (16) байт, иначе RC5_SetKey его молча игнорирует
        QByteArray key("utf8-test-key!!!");
        QString plain = QString::fromUtf8("Привет, MyTetra! Формула: x^2 + y");

        QString crypted = CryptService::encryptString(key, plain);
        QCOMPARE(CryptService::decryptString(key, crypted), plain);
    }

    // Проверка пустой строки: возвращается пустая строка
    void stringEmpty()
    {
        QByteArray key("0123456789abcdef");

        QVERIFY(CryptService::encryptString(key, "").isEmpty());
        QVERIFY(CryptService::decryptString(key, "").isEmpty());
    }

    // Проверка кругового шифрования массива байт
    void byteArrayRoundTrip()
    {
        QByteArray key("byte-array-key!!");
        QByteArray plain("Binary \x00 data \xff test");
        plain.resize(18);

        QByteArray crypted = CryptService::encryptByteArray(key, plain);
        QVERIFY(!crypted.isEmpty());
        QVERIFY(crypted != plain);
        QCOMPARE(CryptService::decryptByteArray(key, crypted), plain);
    }

    // Проверка пустого массива байт
    void byteArrayEmpty()
    {
        QByteArray key("0123456789abcdef");

        QVERIFY(CryptService::encryptByteArray(key, QByteArray()).isEmpty());
        QVERIFY(CryptService::decryptByteArray(key, QByteArray()).isEmpty());
    }

    // Проверка связки строка -> массив -> строка
    void stringToByteArrayRoundTrip()
    {
        QByteArray key("string-byte-key!");
        QString plain = "Text for byte array conversion";

        QByteArray crypted = CryptService::encryptStringToByteArray(key, plain);
        QVERIFY(!crypted.isEmpty());
        QCOMPARE(CryptService::decryptStringFromByteArray(key, crypted), plain);
        QVERIFY(CryptService::encryptStringToByteArray(key, "").isEmpty());
        QVERIFY(CryptService::decryptStringFromByteArray(key, QByteArray()).isEmpty());
    }

    // Проверка чувствительности к ключу
    void differentKeys()
    {
        // Оба ключа ровно по 16 байт (RC5_B)
        QByteArray key1("first-key-012345");
        QByteArray key2("second-key-23456");
        QString plain("Same text, different keys");

        QString crypted1 = CryptService::encryptString(key1, plain);
        QString crypted2 = CryptService::encryptString(key2, plain);
        QVERIFY(crypted1 != crypted2);

        // Расшифровка чужим ключом не дает исходник
        QVERIFY(CryptService::decryptString(key2, crypted1) != plain);
    }

    // Проверка создания и уничтожения объекта (методы статические,
    // но конструктор и деструктор должны существовать и работать)
    void constructorDestructor()
    {
        CryptService service;
        Q_UNUSED(service);
    }

    // Проверка файлового round-trip: шифрование заменяет файл шифрованным,
    // расшифровка возвращает исходное содержимое
    void fileRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QString filePath = dir.filePath("secret.bin");
        QByteArray key("0123456789abcdef");
        QByteArray plain("File secret content \x00\xff with binary");

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(plain), static_cast<qint64>(plain.size()));
        file.close();

        CryptService::encryptFile(key, filePath);

        QFile cryptedFile(filePath);
        QVERIFY(cryptedFile.open(QIODevice::ReadOnly));
        QByteArray crypted = cryptedFile.readAll();
        cryptedFile.close();
        QVERIFY(crypted != plain);

        CryptService::decryptFile(key, filePath);

        QFile restoredFile(filePath);
        QVERIFY(restoredFile.open(QIODevice::ReadOnly));
        QCOMPARE(restoredFile.readAll(), plain);
    }
};


// Собственная функция main вместо QTEST_MAIN.
// Файловые методы CryptService показывают курсор ожидания через
// QApplication::setOverrideCursor, поэтому без объекта QApplication
// они падают. Платформа offscreen включается принудительно,
// чтобы тест работал и без дисплея.
int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    QApplication app(argc, argv);

    TestCryptService test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_cryptservice.moc"
