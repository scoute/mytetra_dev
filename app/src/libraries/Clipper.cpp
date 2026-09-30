#include "Clipper.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QRegularExpression>
#include <QDateTime>

#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "libraries/GlobalParameters.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "models/recordTable/RecordTableData.h"
#include "views/tree/TreeScreen.h"
#include "views/tree/KnowTreeView.h"
#include "views/mainWindow/MainWindow.h"
#include "controllers/recordTable/RecordTableController.h"

extern GlobalParameters globalParameters;


Clipper::Clipper(void)
{
}


QString Clipper::clipboardBranchName(void)
{
    return "Clipboard";
}


bool Clipper::looksLikeUrl(const QString &value)
{
    QString trimmed=value.trimmed();

    return trimmed.startsWith("http://") || trimmed.startsWith("https://");
}


QString Clipper::extractUrl(const QString &text)
{
    // Ссылка это http(s) и все непробельные символы до пробела
    static QRegularExpression urlPattern("https?://\\S+");

    QRegularExpressionMatch match=urlPattern.match(text);
    if(!match.hasMatch())
        return QString();

    QString url=match.captured(0);

    // Отрезается хвост из символов, которые обычно примыкают
    // к ссылке в тексте, но частью ссылки не являются
    while(!url.isEmpty() && QString(".,;:!?)]}'\"").contains(url.right(1)))
        url.chop(1);

    return url;
}


QString Clipper::makeNoteName(const QString &plainText)
{
    // Имя заметки это первая непустая строка текста
    QStringList lines=plainText.split("\n");
    foreach(QString line, lines)
    {
        QString trimmed=line.trimmed();
        if(!trimmed.isEmpty())
        {
            // Длинная строка обрезается, чтобы имя оставалось читаемым
            const int maxNameLength=80;
            if(trimmed.length()>maxNameLength)
                trimmed=trimmed.left(maxNameLength).trimmed()+"...";

            return trimmed;
        }
    }

    // Текста нет (например, в буфере только картинка или HTML),
    // имя собирается из даты и времени клипа
    return "Clipboard "+QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss");
}


QString Clipper::buildNoteHtml(const QMimeData *mime)
{
    if(mime==nullptr)
        return QString();

    // Готовый HTML из браузера или редактора кладется как есть
    if(mime->hasHtml())
        return mime->html();

    // Обычный текст экранируется и разбивается на строки
    return mime->text().toHtmlEscaped().replace("\n", "<br/>");
}


QString Clipper::ensureClipboardBranch(KnowTreeModel *model)
{
    // Рекурсивный поиск ветки с нужным именем по всему дереву,
    // так как пользователь мог переместить ветку вглубь
    QString targetName=clipboardBranchName();

    QList<TreeItem *> stack;
    stack.append(model->getItem(QModelIndex()));
    while(!stack.isEmpty())
    {
        TreeItem *item=stack.takeLast();
        if(item==nullptr)
            continue;

        if(item->getField("name")==targetName)
            return item->getField("id");

        for(int i=0; i<item->childCount(); ++i)
            stack.append(item->child(i));
    }

    // Ветка не найдена, создается ветка верхнего уровня
    QString id=getUniqueId();

    QMap<QString, QString> branchFields;
    branchFields["id"]=id;
    branchFields["name"]=targetName;

    model->addNewChildBranch(QModelIndex(), branchFields);

    return id;
}


bool Clipper::clipFromClipboard(const QString &urlHint)
{
    const QMimeData *mime=QApplication::clipboard()->mimeData();
    if(mime==nullptr)
        return false;

    QString plainText=mime->text();

    // Пустой буфер клипать не во что
    if(plainText.trimmed().isEmpty() && !mime->hasHtml())
    {
        qWarning() << "Clipper: clipboard is empty, nothing to clip";
        return false;
    }

    KnowTreeModel *model=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());
    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

    // Ветка для вырезок, создается при первом клипе
    QString branchId=ensureClipboardBranch(model);
    treeScreen->saveKnowTree();

    TreeItem *item=model->getItemById(branchId);
    if(item==nullptr)
    {
        qWarning() << "Clipper: can not find clipboard branch after creation";
        return false;
    }

    // В зашифрованную ветку без введенного пароля вставить нельзя,
    // метод вставки роняет программу критической ошибкой
    if(item->getField("crypt")=="1" && globalParameters.getCryptKey().length()==0)
    {
        qWarning() << "Clipper: clipboard branch is crypted and password is not entered";
        return false;
    }

    // Несохраненные правки текущей записи сбрасываются в файл,
    // иначе переключение ветки их потеряет
    find_object<MainWindow>("mainwindow")->saveTextarea();

    // Курсор ставится на ветку вырезок, таблица записей переключается
    treeScreen->setCursorToId(branchId);

    // Сборка записи из содержимого буфера
    Record record;
    record.switchToFat();
    record.setText( buildNoteHtml(mime) );
    record.setField("name",   makeNoteName(plainText));
    record.setField("author", "");

    QString url;
    if(looksLikeUrl(urlHint))
        url=urlHint.trimmed();
    else
        url=extractUrl(plainText);
    record.setField("url", url);

    record.setField("tags", "");

    // Вставка записи в конец таблицы ветки вырезок
    RecordTableData *table=item->recordtableGetTableData();
    int pos=table->insertNewRecord(GlobalParameters::AddNewRecordBehavior::ADD_TO_END,
                                   0,
                                   record);
    if(pos<0)
    {
        qWarning() << "Clipper: can not insert record to clipboard branch";
        return false;
    }

    treeScreen->saveKnowTree();

    // Обновление вида таблицы (прямая вставка в данные сигналов не дает)
    // и счетчика записей на ветке
    find_object<RecordTableController>("recordTableController")->setTableData(table);
    treeScreen->updateSelectedBranch();

    qDebug() << "Clipper: note clipped to branch" << branchId << "pos" << pos;

    return true;
}
