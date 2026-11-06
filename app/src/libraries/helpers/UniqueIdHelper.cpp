#include <ctime>

#include <QString>
#include <QRandomGenerator>

#include "UniqueIdHelper.h"


// Функция всегда возвращает уникальный идентификатор
QString getUniqueId(void)
{
 // Уникальный идентификатор состоит из 10 цифр количества секунд с эпохи UNIX
 // и 10 случайных символов 0-9 a-z

 // Количество секунд как число
 long seconds;
 seconds=static_cast<long>( time(nullptr) );

 // Количество секунд как строка
 QString secondsLine=QString::number(seconds, 10);
 secondsLine=secondsLine.rightJustified(10, '0');

 // Строка из 10 случайных символов
 QString symbols="0123456789abcdefghijklmnopqrstuvwxyz";
 QString line;

 // QRandomGenerator вместо rand(): без srand() rand() давал
 // одинаковые серии и коллизии id
 for(int i=0; i<10; i++)
  line+=symbols.mid(static_cast<int>(QRandomGenerator::global()->bounded(symbols.length())), 1);

 QString result=secondsLine+line;

 return result;
}


// Получение псевдо-уникального имени картинки
// Внутренний формат для храния картинок в MyTetra - png
// Вызывающий код должен при необходимости проверять уникальность имени картинки в пределах одной записи
QString getUniqueImageName(void)
{
    return "image"+getUniqueId()+".png"; // "image"+QString::number(rand())+".png";
}


// Имя картинки с заданным расширением. Расширение без точки:
// "png" или "jpg". Неизвестное приводится к png
QString getUniqueImageName(const QString &extension)
{
    QString ext=extension.toLower();

    if(ext!="jpg" && ext!="jpeg" && ext!="png")
        ext="png";

    // Внутри всегда короткое "jpg", "jpeg" только читается снаружи
    if(ext=="jpeg")
        ext="jpg";

    return "image"+getUniqueId()+"."+ext;
}

