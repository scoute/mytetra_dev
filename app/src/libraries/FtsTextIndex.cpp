#include <QDir>
#include <QFileInfo>
#include <QTextDocument>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

#include "libraries/FtsTextIndex.h"
#include "libraries/AppContext.h"
#include "libraries/GlobalParameters.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppConfigStorage.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"


FtsTextIndex::FtsTextIndex(QObject *parent) : QObject(parent)
{
}


FtsTextIndex::~FtsTextIndex(void)
{
    close();
}


// Путь к файлу индекса: рабочая директория (там же conf.ini),
// вне базы чтобы git базы оставался чистым
QString FtsTextIndex::indexFilePath(void)
{
    QString workdir=AppContext::instance().globalParameters()->getWorkDirectory();

    return QDir(workdir).absoluteFilePath(QStringLiteral("mytetra-fts5.db"));
}


// HTML записи в плоский текст, та же конвертация что в поиске
QString FtsTextIndex::plainFromHtml(const QString &html)
{
    QTextDocument textdoc;
    textdoc.setHtml(html);

    return textdoc.toPlainText();
}


// Плоский текст в строку триграмм через пробел для FTS5 unicode61.
// Оставляются только буквы и цифры (регистр отдает FTS),
// остальное выброшено. Склейка монотонна: если исходный текст
// содержит слово, склейка содержит склейку слова, поэтому
// кандидаты FTS всегда надмножество точных совпадений
QString FtsTextIndex::gramsOf(const QString &plain)
{
    QString norm;
    norm.reserve(plain.size());

    for(int i=0; i<plain.size(); ++i)
    {
        QChar c=plain.at(i);

        if(c.isLetterOrNumber())
            norm.append(c);
    }

    if(norm.size()<3)
        return QString();

    QString grams;
    grams.reserve(norm.size()*4);

    for(int i=0; i+3<=norm.size(); ++i)
    {
        if(i>0)
            grams.append(QLatin1Char(' '));

        grams.append(norm.mid(i, 3));
    }

    return grams;
}


// Слово запроса в кусок MATCH-запроса (фраза из триграмм).
// Пусто если слово для FTS непригодно: тогда только линейный поиск
QString FtsTextIndex::wordGrams(const QString &word)
{
    return gramsOf(word);
}


// Открыть/создать базу и схему. Повторный вызов дешев
bool FtsTextIndex::ensureOpen(void)
{
    if(opened)
        return true;

    if(QSqlDatabase::contains(QStringLiteral("fts5")))
        db=QSqlDatabase::database(QStringLiteral("fts5"));
    else
        db=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("fts5"));

    db.setDatabaseName(indexFilePath());

    if(!db.open())
    {
        qWarning() << "FtsTextIndex: cannot open" << indexFilePath() << db.lastError().text();
        return false;
    }

    if(!createSchema())
    {
        qWarning() << "FtsTextIndex: cannot create schema";
        close();
        return false;
    }

    opened=true;

    return true;
}


void FtsTextIndex::close(void)
{
    if(QSqlDatabase::contains(QStringLiteral("fts5")))
    {
        if(db.isOpen())
            db.close();

        db=QSqlDatabase();
        QSqlDatabase::removeDatabase(QStringLiteral("fts5"));
    }

    opened=false;
}


// Создание схемы при первом открытии
bool FtsTextIndex::createSchema(void)
{
    QSqlQuery q(db);

    if(!q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS fts_meta(k TEXT PRIMARY KEY, v TEXT)")))
        return false;

    // recid не индексируется (UNINDEXED): поиск по нему
    // через MATCH не нужен, выборка идет по grams
    if(!q.exec(QStringLiteral("CREATE VIRTUAL TABLE IF NOT EXISTS fts_text USING fts5(recid UNINDEXED, grams, tokenize='unicode61')")))
        return false;

    return true;
}


// Чтение всей мета-таблицы в map
QMap<QString, QString> FtsTextIndex::readMeta(void)
{
    QMap<QString, QString> meta;

    QSqlQuery q(db);
    q.setForwardOnly(true);

    if(!q.exec(QStringLiteral("SELECT k, v FROM fts_meta")))
        return meta;

    while(q.next())
        meta[q.value(0).toString()]=q.value(1).toString();

    return meta;
}


// Запись мета-таблицы целиком
bool FtsTextIndex::writeMeta(const QMap<QString, QString> &meta)
{
    QSqlQuery q(db);

    if(!q.exec(QStringLiteral("DELETE FROM fts_meta")))
        return false;

    q.prepare(QStringLiteral("INSERT INTO fts_meta(k, v) VALUES(?, ?)"));

    QMapIterator<QString, QString> it(meta);

    while(it.hasNext())
    {
        it.next();
        q.addBindValue(it.key());
        q.addBindValue(it.value());

        if(!q.exec())
            return false;
    }

    return true;
}


// Свежесть индекса против текущей базы
bool FtsTextIndex::isFresh(const QString &tetradir,
                           qint64 xmlSize,
                           qint64 xmlMtime,
                           int recordCount,
                           int cryptCount,
                           QString *reason)
{
    if(!ensureOpen())
    {
        if(reason!=nullptr)
            *reason=tr("Cannot open index file");

        return false;
    }

    QMap<QString, QString> meta=readMeta();

    if(meta.value(QStringLiteral("schema"))!=QStringLiteral("1"))
    {
        if(reason!=nullptr)
            *reason=tr("Index schema mismatch");

        return false;
    }

    if(meta.value(QStringLiteral("base_path"))!=tetradir)
    {
        if(reason!=nullptr)
            *reason=tr("Another knowledge base");

        return false;
    }

    if(meta.value(QStringLiteral("xml_size"))!=QString::number(xmlSize) ||
       meta.value(QStringLiteral("xml_mtime"))!=QString::number(xmlMtime))
    {
        if(reason!=nullptr)
            *reason=tr("Knowledge base changed");

        return false;
    }

    if(meta.value(QStringLiteral("record_count"))!=QString::number(recordCount))
    {
        if(reason!=nullptr)
            *reason=tr("Record count changed");

        return false;
    }

    if(meta.value(QStringLiteral("crypt_count"))!=QString::number(cryptCount))
    {
        if(reason!=nullptr)
            *reason=tr("Encrypted records changed");

        return false;
    }

    return true;
}


// Обход ветки для пересборки: шифрованные ветки и записи
// пропускаются всегда, их plaintext не должен лежать рядом с бинарником
void FtsTextIndex::rebuildBranch(const TreeItem *curritem,
                                 int &done,
                                 int total,
                                 std::function<bool(int,int)> &progress,
                                 bool &cancelled)
{
    if(curritem==nullptr || cancelled)
        return;

    if(curritem->getField(QStringLiteral("crypt"))==QStringLiteral("1"))
        return;

    if(curritem->recordtableGetRowCount() > 0)
    {
        const RecordTableData *recordTable=curritem->recordtableGetTableData();

        QSqlQuery q(db);
        q.prepare(QStringLiteral("INSERT INTO fts_text(recid, grams) VALUES(?, ?)"));

        int batch=0;

        for(int i=0; i<static_cast<int>(recordTable->size()); i++)
        {
            if(cancelled)
                return;

            // Шифрованные записи не индексируются никогда
            if(recordTable->getField(QStringLiteral("crypt"), i)==QStringLiteral("1"))
                continue;

            const QString recId=recordTable->getField(QStringLiteral("id"), i);

            QString grams=gramsOf(plainFromHtml(recordTable->getText(i)));

            if(!grams.isEmpty())
            {
                q.addBindValue(recId);
                q.addBindValue(grams);
                q.exec();

                if(++batch>=4096)
                {
                    batch=0;
                    db.commit();
                    db.transaction();
                }
            }

            ++done;

            if((done % 256)==0 && !progress(done, total))
                cancelled=true;
        }
    }

    for(int i=0; i<curritem->childCount(); i++)
    {
        rebuildBranch(curritem->child(i), done, total, progress, cancelled);

        if(cancelled)
            return;
    }
}


// Полная пересборка по модели дерева
bool FtsTextIndex::rebuildFull(KnowTreeModel *model,
                               std::function<bool(int,int)> progress)
{
    if(model==nullptr || !ensureOpen())
        return false;

    const TreeItem *rootItem=model->getRootItem();

    if(rootItem==nullptr)
        return false;

    const int total=model->getAllRecordCount();

    QSqlQuery q(db);

    if(!q.exec(QStringLiteral("DELETE FROM fts_text")))
        return false;

    db.transaction();

    rebuildRunning=true;
    touchedDuringRebuild=false;

    int done=0;
    bool cancelled=false;

    rebuildBranch(rootItem, done, total, progress, cancelled);

    rebuildRunning=false;

    if(cancelled)
    {
        db.rollback();
        return false;
    }

    db.commit();

    // Мета пишется только при полной сборке: недособранный
    // индекс считается несвежим и предложится заново
    QString tetradir=AppContext::instance().appConfig()->storageSection()->get_tetradir();
    QFileInfo xmlInfo(QDir(tetradir).absoluteFilePath(QStringLiteral("mytetra.xml")));

    QMap<QString, QString> meta;
    meta[QStringLiteral("schema")]=QStringLiteral("1");
    meta[QStringLiteral("base_path")]=tetradir;
    meta[QStringLiteral("xml_size")]=QString::number(xmlInfo.size());
    meta[QStringLiteral("xml_mtime")]=QString::number(xmlInfo.lastModified().toMSecsSinceEpoch());

    // Правки в ходе сборки могли потеряться: мета грязная,
    // следующий поиск с текстом пересоберет индекс заново
    if(touchedDuringRebuild)
        meta[QStringLiteral("record_count")]=QStringLiteral("-1");
    else
        meta[QStringLiteral("record_count")]=QString::number(total);

    meta[QStringLiteral("crypt_count")]=QString::number(cryptRecordCount(model));

    writeMeta(meta);

    progress(total, total);

    return true;
}


// Число шифрованных записей в базе. Обход только по флагам,
// без чтения файлов текстов. Зеркалит rebuildBranch: шифрованная
// ветка скипается целиком вместе с поддеревом, поэтому ее записи
// считаются через итог модели, а не пофлагово
int FtsTextIndex::cryptRecordCount(KnowTreeModel *model)
{
    if(model==nullptr || model->getRootItem()==nullptr)
        return 0;

    int count=0;

    // Стек вместо рекурсии: глубокие деревья не переполняют стек вызова
    QList<const TreeItem *> stack;
    stack.append(model->getRootItem());

    while(!stack.isEmpty())
    {
        const TreeItem *item=stack.takeLast();

        // Шифрованная ветка: все ее записи неиндексируемы,
        // вглубь не идем так же как rebuildBranch
        if(item->getField(QStringLiteral("crypt"))==QStringLiteral("1"))
        {
            count+=model->getRecordCountForItem(item);
            continue;
        }

        if(item->recordtableGetRowCount() > 0)
        {
            const RecordTableData *recordTable=item->recordtableGetTableData();

            for(int i=0; i<static_cast<int>(recordTable->size()); i++)
                if(recordTable->getField(QStringLiteral("crypt"), i)==QStringLiteral("1"))
                    ++count;
        }

        for(int i=0; i<item->childCount(); i++)
            stack.append(item->child(i));
    }

    return count;
}


// Обновить одну запись. Пустой текст удаляет строку
void FtsTextIndex::upsert(const QString &recordId, const QString &htmlText)
{
    if(rebuildRunning)
        touchedDuringRebuild=true;

    if(recordId.isEmpty() || !ensureOpen())
        return;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM fts_text WHERE recid=?"));
    q.addBindValue(recordId);
    q.exec();

    QString grams=gramsOf(plainFromHtml(htmlText));

    if(grams.isEmpty())
        return;

    q.prepare(QStringLiteral("INSERT INTO fts_text(recid, grams) VALUES(?, ?)"));
    q.addBindValue(recordId);
    q.addBindValue(grams);
    q.exec();
}


// Удалить одну запись из индекса
void FtsTextIndex::remove(const QString &recordId)
{
    if(rebuildRunning)
        touchedDuringRebuild=true;

    if(recordId.isEmpty() || !ensureOpen())
        return;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM fts_text WHERE recid=?"));
    q.addBindValue(recordId);
    q.exec();
}


// Id записей-кандидатов по словам запроса. matchAll=false это ИЛИ.
// В граммах только буквы/цифры, кавычек быть не может:
// MATCH собирается без экранирования
QSet<QString> FtsTextIndex::search(const QStringList &words, bool matchAll, bool *ok)
{
    QSet<QString> result;

    if(ok!=nullptr)
        *ok=false;

    if(!ensureOpen())
        return result;

    QStringList parts;

    foreach(const QString &word, words)
    {
        QString grams=wordGrams(word);

        if(grams.isEmpty())
            return result;

        parts << QStringLiteral("\"") + grams + QStringLiteral("\"");
    }

    if(parts.isEmpty())
        return result;

    QString match=matchAll ? parts.join(QStringLiteral(" ")) : parts.join(QStringLiteral(" OR "));

    QSqlQuery q(db);
    q.setForwardOnly(true);
    q.prepare(QStringLiteral("SELECT recid FROM fts_text WHERE fts_text MATCH ?"));
    q.addBindValue(match);

    if(!q.exec())
        return result;

    while(q.next())
        result.insert(q.value(0).toString());

    if(ok!=nullptr)
        *ok=true;

    return result;
}


// Число проиндексированных записей, для диагностики
int FtsTextIndex::indexedCount(void)
{
    if(!ensureOpen())
        return -1;

    QSqlQuery q(db);

    if(!q.exec(QStringLiteral("SELECT COUNT(*) FROM fts_text")) || !q.next())
        return -1;

    return q.value(0).toInt();
}
