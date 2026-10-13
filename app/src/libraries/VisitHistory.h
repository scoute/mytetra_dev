#ifndef _VISITHISTORY_H_
#define _VISITHISTORY_H_

#include <QObject>
#include <QString>
#include <QList>

// Журнал посещений заметок. Дописывается из WalkHistory::add() при каждом
// реальном переходе (повторы подряд отсекаются там же), читается панелью
// истории. Формат: JSON по строке {t, id}.
// Отдельно от навигационной истории WalkHistory: там указатель туда-сюда
// и позиции курсора, здесь только факты посещений со временем.
// Сайдкар рядом с базой, формат базы не затрагивается: без файла
// история пуста, битый файл игнорируется, удаленные заметки при показе
// пропускаются

struct VisitStat
{
    QString id;
    int count;
    qint64 lastMsecs;
};


class VisitHistory : public QObject
{
    Q_OBJECT

public:

    VisitHistory(void);
    virtual ~VisitHistory(void);

    // Записать посещение. Пустые id молча отсекаются
    void logVisit(const QString &id);

    // Агрегат по журналу: визиты и последнее время для каждого id
    QList<VisitStat> stats(void) const;

    // Убрать одну заметку из журнала (память и файл)
    void forget(const QString &id);

    // Очистить журнал на диске и в памяти
    void clear(void);

signals:

    // Новое посещение записано. Панель истории обновляется живьем
    void visitLogged(const QString &id);

private:

    QString filePath(void) const;

    // Подгрузить файл в память один раз, лениво.
    // Const чтобы агрегат мог подгружать при первом чтении
    void ensureLoaded(void) const;

    // Дописать строку в файл с ротацией
    void appendLine(const QString &line);

    // Переписать файл целиком из памяти (для forget)
    void rewriteFile(void) const;

    struct VisitEntry
    {
        qint64 msecs;
        QString id;
    };

    mutable QList<VisitEntry> entries;
    mutable bool loaded;
};

#endif /* _VISITHISTORY_H_ */
