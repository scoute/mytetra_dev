#include "CryptService.h"
#include "CipherBackend.h"
#include "RC5CipherBackend.h"
#include "XChaChaCipherBackend.h"

#include <QFile>
#include <QApplication>

#include "libraries/helpers/DebugHelper.h"
#include "models/appConfig/AppConfig.h"

extern AppConfig mytetraConfig;


using std::vector;


CryptService::CryptService()
{

}


CryptService::~CryptService()
{

}


void CryptService::convertByteArrayToVector(const QByteArray &qba, std::vector<unsigned char> &vec)
{
  unsigned int size=qba.size();
  vec.resize(size, 0);
  memcpy(&vec[0], qba.constData(), size*sizeof(unsigned char));
}


void CryptService::convertVectorToByteArray(const std::vector<unsigned char> &vec, QByteArray &qba)
{
  unsigned int size=vec.size();
  qba.clear();
  qba.append( (const char *)&vec[0], size*sizeof(unsigned char) );

  /*
  printf("\n");
  qDebug() << "Vector: ";
  for(unsigned int i=0; i<size; i++)
    printf("%02hhx", vec[i]);
  printf("\n");

  printf("\n");
  qDebug() << "QByteArray: ";
  for(unsigned int i=0; i<size; i++)
    printf("%02hhx", qba.at(i));
  printf("\n");
  */
}


// Выбор бэкенда: шифрование по настройке cryptMethod,
// расшифровка по магии блоба (MTENC1 -> новый, иначе RC5)
QString CryptService::detectMethod(const QByteArray &data)
{
    if(data.startsWith(MTENC1_MAGIC))
        return QString::fromLatin1(CIPHER_XCHACHA);

    return QString::fromLatin1(CIPHER_RC5);
}


CipherBackend *CryptService::backend(const QString &method)
{
    // Бэкенды без состояния, один экземпляр на программу
    static RC5CipherBackend rc5;
    static XChaChaCipherBackend xchacha;

    if(method==QString::fromLatin1(CIPHER_RC5))
        return &rc5;

    if(method==QString::fromLatin1(CIPHER_XCHACHA))
        return &xchacha;

    return nullptr;
}


// Шифрация строки
// Результирующая зашифрованная строка - это набор шифрованных байт,
// представленный в кодировке Base64
QString CryptService::encryptString(QByteArray key, QString line)
{
 if(line=="") return QString();

 QByteArray encrypted=encryptByteArray(key, line.toUtf8());

 QString resultLine=QString::fromLatin1(encrypted.toBase64());

 return resultLine;
}


// Расшифровка строки
// Принимаемая зашифрованная строка - это набор шифрованных байт,
// представленный в кодировке Base64
QString CryptService::decryptString(QByteArray key, QString line)
{
 if(line=="") return QString();

 // Заменен вызов line.toAscii на line.toLatin1 чтобы шла компиляция в Qt 5.2.
 // Эта замена допустима, так как в Base64 используются только символы латиницы и ограниченный набор прочих символов
 QByteArray encrypted=QByteArray::fromBase64( line.toLatin1() );

 QByteArray result=decryptByteArray(key, encrypted);

 QString resultLine=QString::fromUtf8( result.data() );

 return resultLine;
}


QByteArray CryptService::encryptByteArray(QByteArray key, QByteArray data)
{
  if(data.size()==0)
    return QByteArray();

  CipherBackend *cipher=backend(mytetraConfig.get_cryptMethod());

  if(cipher==nullptr)
    criticalError("CryptService::encryptByteArray() : Unknown cipher method.");

  return cipher->encryptBytes(key, data);
}


QByteArray CryptService::decryptByteArray(QByteArray key, QByteArray data)
{
  if(data.size()==0)
    return QByteArray();

  CipherBackend *cipher=backend(detectMethod(data));

  if(cipher==nullptr)
    return QByteArray();

  return cipher->decryptBytes(key, data);
}


QByteArray CryptService::encryptStringToByteArray(QByteArray key, QString line)
{
 if(line=="")
   return QByteArray();

 return encryptByteArray(key, line.toUtf8());
}


QString CryptService::decryptStringFromByteArray(QByteArray key, QByteArray data)
{
 if(data.length()==0)
   return QString();

 QByteArray result=decryptByteArray(key, data);

 // Расшифровка с явным размером и вычищением 0x00, иначе fromUtf8()
 // обрежет текст по первому нулевому байту (баг #134)
 QString resultLine=QString::fromUtf8( result.constData(), result.size() );
 resultLine.remove(QChar('\0'));

 return resultLine;
}


// Шифрование файла на диске, вместо незашифрованного файла появляется зашифрованный
void CryptService::encryptFile(QByteArray key, QString fileName)
{
 encDecFileSmart(key, fileName, 0);
}


// Расшифровка файла на диске, вместо зашифрованного файла появляется нешифрованный
void CryptService::decryptFile(QByteArray key, QString fileName)
{
 encDecFileSmart(key, fileName, 1);
}


void CryptService::encDecFileSmart(QByteArray key, QString fileName, int mode)
{
 // Headless-процессы (MCP-сервер) живут без QApplication:
 // курсор дёргается только при живом GUI
 if(qobject_cast<QApplication *>(QCoreApplication::instance())!=nullptr)
   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

 QFile file(fileName);

 if(!file.open(QIODevice::ReadOnly))
  criticalError("encDecFileSmart() : Cant open binary file "+fileName+" for reading.");

 QByteArray result;

 if(mode==0)
  result=encryptByteArray(key, file.readAll());
 else
  result=decryptByteArray(key, file.readAll());

 file.close();

 if(!file.open(QIODevice::WriteOnly))
  criticalError("encryptFile() : Cant open binary file "+fileName+" for write.");
 file.write(result);

 if(qobject_cast<QApplication *>(QCoreApplication::instance())!=nullptr)
   QApplication::restoreOverrideCursor();
}
