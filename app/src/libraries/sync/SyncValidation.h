#ifndef SYNCVALIDATION_H
#define SYNCVALIDATION_H

#include <QDir>
#include <QString>
#include <QStringList>

// Валидация имён/путей, приходящих от пира по sync/1.
// Вынесено в отдельный header без зависимостей от модели: это чистая
// логика, которую можно (и нужно) покрывать тестами без сборки всего
// приложения. Синк пишет файлы по путям из запроса, поэтому dir/file и
// имена вложений — недоверенные данные.
namespace SyncValidation
{

// Один сегмент пути: непустой, без '/', '\\', NUL, не "." / "..",
// не абсолютный.
inline bool isSafeFileName(const QString &name)
{
    if(name.isEmpty()
       || name==QStringLiteral(".")
       || name==QStringLiteral(".."))
        return false;

    if(name.contains(QLatin1Char('/'))
       || name.contains(QLatin1Char('\\'))
       || name.contains(QChar(0)))
        return false;

    return !QDir::isAbsolutePath(name);
}

// Относительный путь из безопасных сегментов (вложенные подкаталоги
// допустимы, ".."/"." — нет). Пустой и абсолютный — небезопасны.
inline bool isSafeRecordDir(const QString &dir)
{
    if(dir.isEmpty() || QDir::isAbsolutePath(dir))
        return false;

    const QString clean=QDir::cleanPath(dir);

    if(clean==QStringLiteral(".")
       || clean==QStringLiteral("..")
       || clean.startsWith(QStringLiteral("../")))
        return false;

    const QStringList parts=dir.split(QLatin1Char('/'), Qt::SkipEmptyParts);

    for(const QString &part : parts)
        if(!isSafeFileName(part))
            return false;

    return !parts.isEmpty();
}

// Путь остался внутри baseDir (после cleanPath). Страховка на случай
// будущих изменений валидации сегментов.
inline bool isInsideDir(const QString &baseDir, const QString &path)
{
    const QString base=QDir::cleanPath(baseDir);
    const QString clean=QDir::cleanPath(path);

    return clean==base || clean.startsWith(base+QStringLiteral("/"));
}

} // namespace SyncValidation

#endif // SYNCVALIDATION_H
