#include <QtTest>

#include <vector>

#include <QFile>
#include <QTemporaryDir>

#include "libraries/crypt/RC5Simple.h"

// Тестовый класс для проверки шифра RC5Simple в памяти.
// Файловые методы не тестируются: они работают с диском.

class TestRc5Simple : public QObject
{
    Q_OBJECT

    // Создание 16-байтного ключа из строки
    std::vector<unsigned char> makeKey(const QByteArray &source)
    {
        std::vector<unsigned char> key(RC5_B, 0);
        for (int i = 0; i < RC5_B && i < source.size(); ++i)
        {
            key[static_cast<size_t>(i)] = static_cast<unsigned char>(source.at(i));
        }
        return key;
    }

    // Создание входного вектора из QByteArray
    std::vector<unsigned char> makeInput(const QByteArray &source)
    {
        std::vector<unsigned char> data;
        data.resize(static_cast<size_t>(source.size()));
        for (int i = 0; i < source.size(); ++i)
        {
            data[static_cast<size_t>(i)] = static_cast<unsigned char>(source.at(i));
        }
        return data;
    }

    // Преобразование вектора в QByteArray для сравнения
    QByteArray toByteArray(const std::vector<unsigned char> &data)
    {
        QByteArray result;
        result.resize(static_cast<int>(data.size()));
        for (size_t i = 0; i < data.size(); ++i)
        {
            result[static_cast<int>(i)] = static_cast<char>(data[i]);
        }
        return result;
    }

    // Чтение всего файла в массив, с проверкой открытия
    QByteArray readFile(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            return QByteArray();
        }
        return file.readAll();
    }

    // Запись массива в файл, с проверкой открытия
    bool writeFile(const QString &path, const QByteArray &data)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
        {
            return false;
        }
        return file.write(data) == data.size();
    }

private slots:

    // Проверка версии библиотеки
    void versionNotEmpty()
    {
        RC5Simple rc5;
        QVERIFY(strlen(rc5.RC5_GetVersion()) > 0);
    }

    // Проверка кругового шифрования произвольных данных
    void roundTrip()
    {
        std::vector<unsigned char> key = makeKey("0123456789abcdef");
        QByteArray plain("Hello, MyTetra! Test data 123");

        RC5Simple rc5;
        rc5.RC5_SetKey(key);
        QCOMPARE(rc5.RC5_GetErrorCode(), 0u);

        std::vector<unsigned char> in = makeInput(plain);
        std::vector<unsigned char> crypted;
        rc5.RC5_Encrypt(in, crypted);
        QVERIFY(!crypted.empty());
        QVERIFY(toByteArray(crypted) != plain);

        std::vector<unsigned char> decrypted;
        rc5.RC5_Decrypt(crypted, decrypted);
        QCOMPARE(toByteArray(decrypted), plain);
    }

    // Проверка данных разного размера, включая границы блока (8 байт)
    void roundTripVariousSizes()
    {
        std::vector<unsigned char> key = makeKey("key-for-sizes!!!");
        QList<int> sizes;
        sizes << 1 << 7 << 8 << 9 << 15 << 16 << 100;

        foreach (int size, sizes)
        {
            QByteArray plain(size, 'x');
            // Разбавляем содержимое, чтобы не было одинаковых блоков
            for (int i = 0; i < size; ++i)
            {
                plain[i] = static_cast<char>('a' + (i % 26));
            }

            RC5Simple rc5;
            rc5.RC5_SetKey(key);

            std::vector<unsigned char> in = makeInput(plain);
            std::vector<unsigned char> crypted;
            rc5.RC5_Encrypt(in, crypted);

            std::vector<unsigned char> decrypted;
            rc5.RC5_Decrypt(crypted, decrypted);

            QVERIFY2(toByteArray(decrypted) == plain,
                     qPrintable(QString("Размер %1 не прошел round-trip").arg(size)));
        }
    }

    // Проверка бинарных данных с нулевыми байтами
    void roundTripBinary()
    {
        std::vector<unsigned char> key = makeKey("binary-key-012345");
        QByteArray plain("\x00\x01\x02\xff\xfe\x00\x41", 7);

        RC5Simple rc5;
        rc5.RC5_SetKey(key);

        std::vector<unsigned char> in = makeInput(plain);
        std::vector<unsigned char> crypted;
        rc5.RC5_Encrypt(in, crypted);

        std::vector<unsigned char> decrypted;
        rc5.RC5_Decrypt(crypted, decrypted);

        QCOMPARE(toByteArray(decrypted), plain);
    }

    // Проверка шифрования пустых данных: ошибка 5
    void encryptEmptySetsError()
    {
        RC5Simple rc5;
        std::vector<unsigned char> key = makeKey("0123456789abcdef");
        rc5.RC5_SetKey(key);

        std::vector<unsigned char> in;
        std::vector<unsigned char> out;
        rc5.RC5_Encrypt(in, out);

        QCOMPARE(rc5.RC5_GetErrorCode(), static_cast<unsigned int>(RC5_ERROR_CODE_5));
        QVERIFY(out.empty());
    }

    // Проверка расшифровки пустых данных: ошибка 6
    void decryptEmptySetsError()
    {
        RC5Simple rc5;
        std::vector<unsigned char> key = makeKey("0123456789abcdef");
        rc5.RC5_SetKey(key);

        std::vector<unsigned char> in;
        std::vector<unsigned char> out;
        rc5.RC5_Decrypt(in, out);

        QCOMPARE(rc5.RC5_GetErrorCode(), static_cast<unsigned int>(RC5_ERROR_CODE_6));
        QVERIFY(out.empty());
    }

    // Проверка ключа неверной длины: ошибка 1
    void badKeyLengthSetsError()
    {
        RC5Simple rc5;
        std::vector<unsigned char> shortKey(4, 'k');
        rc5.RC5_SetKey(shortKey);

        QCOMPARE(rc5.RC5_GetErrorCode(), static_cast<unsigned int>(RC5_ERROR_CODE_1));
    }

    // Проверка расшифровки чужим ключом: результат не совпадает с исходником
    void wrongKeyDoesNotDecrypt()
    {
        std::vector<unsigned char> key1 = makeKey("key-number-one!!!");
        std::vector<unsigned char> key2 = makeKey("key-number-two!!!");

        QByteArray plain("Secret message for key test");

        RC5Simple rc5;
        rc5.RC5_SetKey(key1);
        std::vector<unsigned char> in = makeInput(plain);
        std::vector<unsigned char> crypted;
        rc5.RC5_Encrypt(in, crypted);

        rc5.RC5_SetKey(key2);
        std::vector<unsigned char> decrypted;
        rc5.RC5_Decrypt(crypted, decrypted);

        QVERIFY(toByteArray(decrypted) != plain);
    }

    // Проверка конструктора с инициализацией генератора случайных чисел
    void randomInitConstructor()
    {
        RC5Simple rc5(true);
        std::vector<unsigned char> key = makeKey("0123456789abcdef");
        rc5.RC5_SetKey(key);

        QByteArray plain("Data with random init");
        std::vector<unsigned char> in = makeInput(plain);
        std::vector<unsigned char> crypted;
        rc5.RC5_Encrypt(in, crypted);

        std::vector<unsigned char> decrypted;
        rc5.RC5_Decrypt(crypted, decrypted);
        QCOMPARE(toByteArray(decrypted), plain);
    }

    // Проверка кругового шифрования в устаревшем формате 1.
    // Расшифровка ведется свежим объектом без force: формат определяется
    // по отсутствию сигнатуры.
    void roundTripFormatV1()
    {
        std::vector<unsigned char> key = makeKey("0123456789abcdef");
        QByteArray plain("Legacy format one data 123");

        RC5Simple rc5enc;
        rc5enc.RC5_SetKey(key);
        rc5enc.RC5_SetFormatVersionForce(RC5_FORMAT_VERSION_1);

        std::vector<unsigned char> in = makeInput(plain);
        std::vector<unsigned char> crypted;
        rc5enc.RC5_Encrypt(in, crypted);
        QVERIFY(!crypted.empty());

        RC5Simple rc5dec;
        rc5dec.RC5_SetKey(key);

        std::vector<unsigned char> decrypted;
        rc5dec.RC5_Decrypt(crypted, decrypted);
        QCOMPARE(toByteArray(decrypted), plain);
    }

    // Проверка кругового шифрования в формате 2 с автоопределением при расшифровке
    void roundTripFormatV2()
    {
        std::vector<unsigned char> key = makeKey("0123456789abcdef");
        QByteArray plain("Format two data payload");

        RC5Simple rc5enc;
        rc5enc.RC5_SetKey(key);
        rc5enc.RC5_SetFormatVersionForce(RC5_FORMAT_VERSION_2);

        std::vector<unsigned char> in = makeInput(plain);
        std::vector<unsigned char> crypted;
        rc5enc.RC5_Encrypt(in, crypted);
        QVERIFY(!crypted.empty());

        RC5Simple rc5dec;
        rc5dec.RC5_SetKey(key);

        std::vector<unsigned char> decrypted;
        rc5dec.RC5_Decrypt(crypted, decrypted);
        QCOMPARE(toByteArray(decrypted), plain);
    }

    // Проверка расшифровки с принудительно заданным форматом (без автодетекта)
    void decryptWithForcedFormat()
    {
        std::vector<unsigned char> key = makeKey("0123456789abcdef");
        QByteArray plain("Forced format decrypt check");

        RC5Simple rc5enc;
        rc5enc.RC5_SetKey(key);

        std::vector<unsigned char> in = makeInput(plain);
        std::vector<unsigned char> crypted;
        rc5enc.RC5_Encrypt(in, crypted);

        RC5Simple rc5dec;
        rc5dec.RC5_SetKey(key);
        rc5dec.RC5_SetFormatVersionForce(RC5_FORMAT_VERSION_CURRENT);

        std::vector<unsigned char> decrypted;
        rc5dec.RC5_Decrypt(crypted, decrypted);
        QCOMPARE(toByteArray(decrypted), plain);
    }

    // Проверка файлового round-trip: шифрование и расшифровка файлов на диске.
    // Задействуются обе перегрузки (const char* и unsigned char*).
    void fileRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QString inPath = dir.filePath("plain.bin");
        QString encPath = dir.filePath("crypted.bin");
        QString decPath = dir.filePath("restored.bin");

        QByteArray plain("File content with binary \x00\xff bytes inside");
        QVERIFY(writeFile(inPath, plain));

        std::vector<unsigned char> key = makeKey("0123456789abcdef");

        QByteArray inBytes = inPath.toLocal8Bit();
        QByteArray encBytes = encPath.toLocal8Bit();
        QByteArray decBytes = decPath.toLocal8Bit();

        RC5Simple rc5enc;
        rc5enc.RC5_SetKey(key);
        rc5enc.RC5_EncryptFile(inBytes.constData(), encBytes.constData());
        QCOMPARE(rc5enc.RC5_GetErrorCode(), 0u);
        QVERIFY(readFile(encPath) != plain);

        RC5Simple rc5dec;
        rc5dec.RC5_SetKey(key);
        rc5dec.RC5_DecryptFile(encBytes.constData(), decBytes.constData());
        QCOMPARE(rc5dec.RC5_GetErrorCode(), 0u);
        QCOMPARE(readFile(decPath), plain);
    }

    // Проверка перегрузок с unsigned char*: шифрование и расшифровка файлов
    void fileOverloadsUchar()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QString inPath = dir.filePath("plain.bin");
        QString encPath = dir.filePath("crypted.bin");
        QString decPath = dir.filePath("restored.bin");

        QByteArray plain("Uchar overloads check data");
        QVERIFY(writeFile(inPath, plain));

        QByteArray inBytes = inPath.toLocal8Bit();
        QByteArray encBytes = encPath.toLocal8Bit();
        QByteArray decBytes = decPath.toLocal8Bit();

        std::vector<unsigned char> key = makeKey("0123456789abcdef");

        RC5Simple rc5enc;
        rc5enc.RC5_SetKey(key);
        rc5enc.RC5_EncryptFile(reinterpret_cast<unsigned char *>(inBytes.data()),
                               reinterpret_cast<unsigned char *>(encBytes.data()));
        QCOMPARE(rc5enc.RC5_GetErrorCode(), 0u);

        RC5Simple rc5dec;
        rc5dec.RC5_SetKey(key);
        rc5dec.RC5_DecryptFile(reinterpret_cast<unsigned char *>(encBytes.data()),
                               reinterpret_cast<unsigned char *>(decBytes.data()));
        QCOMPARE(rc5dec.RC5_GetErrorCode(), 0u);
        QCOMPARE(readFile(decPath), plain);
    }

    // Проверка кода ошибки при отсутствии входного файла
    void fileMissingInputSetsError()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QByteArray outBytes = dir.filePath("out.bin").toLocal8Bit();
        QByteArray missingBytes = dir.filePath("no-such-file.bin").toLocal8Bit();

        RC5Simple rc5;
        std::vector<unsigned char> key = makeKey("0123456789abcdef");
        rc5.RC5_SetKey(key);

        rc5.RC5_EncryptFile(missingBytes.constData(), outBytes.constData());
        QCOMPARE(rc5.RC5_GetErrorCode(), static_cast<unsigned int>(RC5_ERROR_CODE_2));
    }

    // Проверка кода ошибки при пустом входном файле
    void fileEmptyInputSetsError()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QString inPath = dir.filePath("empty.bin");
        QVERIFY(writeFile(inPath, QByteArray()));
        QByteArray inBytes = inPath.toLocal8Bit();
        QByteArray outBytes = dir.filePath("out.bin").toLocal8Bit();

        RC5Simple rc5;
        std::vector<unsigned char> key = makeKey("0123456789abcdef");
        rc5.RC5_SetKey(key);

        rc5.RC5_EncryptFile(inBytes.constData(), outBytes.constData());
        QCOMPARE(rc5.RC5_GetErrorCode(), static_cast<unsigned int>(RC5_ERROR_CODE_3));
    }

    // Проверка кода ошибки при невозможности создать выходной файл
    void fileBadOutputSetsError()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QString inPath = dir.filePath("plain.bin");
        QVERIFY(writeFile(inPath, QByteArray("some data")));
        QByteArray inBytes = inPath.toLocal8Bit();
        // Выходной файл в несуществующей директории создать нельзя
        QByteArray outBytes = dir.filePath("no-such-dir/out.bin").toLocal8Bit();

        RC5Simple rc5;
        std::vector<unsigned char> key = makeKey("0123456789abcdef");
        rc5.RC5_SetKey(key);

        rc5.RC5_EncryptFile(inBytes.constData(), outBytes.constData());
        QCOMPARE(rc5.RC5_GetErrorCode(), static_cast<unsigned int>(RC5_ERROR_CODE_4));
    }
};

QTEST_APPLESS_MAIN(TestRc5Simple)

#include "tst_rc5simple.moc"
