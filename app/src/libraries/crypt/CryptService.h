#ifndef __CRYPTSERVICE_H__
#define __CRYPTSERVICE_H__

#include <vector>

#include <QString>
#include <QByteArray>

#include "CipherBackend.h"

// Класс с функциями-помощниками, используемыми при шифрации/дешифровки


class CryptService
{

public:

  CryptService();
  virtual ~CryptService();

  static void convertByteArrayToVector(const QByteArray &qba, std::vector<unsigned char> &vec);
  static void convertVectorToByteArray(const std::vector<unsigned char> &vec, QByteArray &qba);

  static QString encryptString(QByteArray key, QString line);
  static QString decryptString(QByteArray key, QString line);

  static QByteArray encryptByteArray(QByteArray key, QByteArray data);
  static QByteArray decryptByteArray(QByteArray key, QByteArray data);

  static QByteArray encryptStringToByteArray(QByteArray key, QString line);
  static QString decryptStringFromByteArray(QByteArray key, QByteArray data);

  static void encryptFile(QByteArray key, QString fileName);
  static void decryptFile(QByteArray key, QString fileName);
  static void encDecFileSmart(QByteArray key, QString fileName, int mode);

  // Расшифровка ТЕКСТОВОГО файла на диске (текст записи).
  // Как decryptFile, но перед атомарной записью проверяет что
  // результат валидный UTF-8: тексты заметок всегда валидны,
  // а RC5-шум почти никогда. Двойная расшифровка уже открытого
  // файла роняла бы шум поверх единственной копии (см. 511)
  static void decryptTextFile(QByteArray key, QString fileName);

  // Проверка валидности UTF-8. Пустой массив валиден
  static bool isValidUtf8(const QByteArray &data);

  // Выбор бэкенда: шифрование по настройке cryptMethod,
  // расшифровка по магии блоба (MTENC1 -> новый, иначе RC5).
  // Пустой результат расшифровки = неуспех (чужой тег/пароль)
  static QString detectMethod(const QByteArray &data);

private:

  static CipherBackend *backend(const QString &method);
};

#endif // __CRYPTSERVICE_H__
