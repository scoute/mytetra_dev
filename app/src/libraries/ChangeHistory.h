#ifndef _CHANGEHISTORY_H_
#define _CHANGEHISTORY_H_

#include <QString>
#include <QList>

#include "libraries/GitHelper.h"

// История изменений текста заметок на git.
// Хранилище <tetradir>/.notehistory: файл <recordId>.html
// коммитится при каждом сохранении с реальными изменениями.
// Шифрованные заметки не снимаются. Без git в системе все
// методы тихо ничего не делают

class ChangeHistory
{
public:

    struct Version
    {
        QString rev;
        QString date;
        QString subject;
    };

    // Каталог хранилища, создается при первом снимке
    static QString historyDir(const QString &tetradir);

    // Снимок текста. Ложь если нечего снимать (без изменений,
    // пустой текст, нет git). Истина если закоммичена версия
    static bool snapshot(const QString &tetradir, const QString &recordId,
                         const QString &recordName, const QString &html);

    // Версии от новых к старым, пусто если истории нет
    static QList<Version> versions(const QString &tetradir,
                                   const QString &recordId);

    // Текст версии, пусто если нет
    static QString contentAt(const QString &tetradir, const QString &recordId,
                             const QString &rev);

    // Unified diff версии против переданного текущего текста
    static QString diffToCurrent(const QString &tetradir, const QString &recordId,
                                 const QString &rev, const QString &currentHtml,
                                 const QString &title);

    // Откат к версии с уничтожением всех более новых версий файла.
    // Нового коммита не создается: файл встает на целевую ревизию.
    // Истории других заметок сохраняются переигрыванием.
    // Текст ошибки опционально
    static bool resetToVersion(const QString &tetradir, const QString &recordId,
                               const QString &rev, QString *errorMessage=nullptr);

    // Сжать историю снизу: хранить genesis плюс keepNewest-1 свежих.
    // Ручной вызов и авточка по лимиту. Текст ошибки опционально
    static bool squashOldVersions(const QString &tetradir, const QString &recordId,
                                  int keepNewest, QString *errorMessage=nullptr);
};

#endif /* _CHANGEHISTORY_H_ */
