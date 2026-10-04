#include "Clipper.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QRegularExpression>
#include <QDateTime>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextImageFormat>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QDir>

#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "libraries/GlobalParameters.h"
#include "models/appConfig/AppConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "models/recordTable/RecordTableData.h"
#include "views/tree/TreeScreen.h"
#include "views/tree/KnowTreeView.h"
#include "views/mainWindow/MainWindow.h"
#include "controllers/recordTable/RecordTableController.h"

extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;


// Ограничения фонового скачивания картинок: обычная вставка спрашивает
// подтверждение у пользователя, а клиппер работает из скрытого окна,
// поэтому спрашиваь некого и действуют лимиты из настроек
static const int defaultClipMaxImages=20;
static const qint64 defaultClipMaxImageSizeBytes=5*1024*1024;
static const int clipDownloadTimeoutMs=15000;

// Актуальные лимиты. По умолчанию встроенные значения, боевые
// подтягиваются из конфига в начале каждого клипа
static int clipMaxImages=defaultClipMaxImages;
static qint64 clipMaxImageBytes=defaultClipMaxImageSizeBytes;


void Clipper::reloadLimits(void)
{
    // Ручная правка conf.ini может дать мусор, отрицательные
    // и нулевые значения отбрасываются в пользу встроенных
    int maxImages=mytetraConfig.get_clipperMaxImages();
    if(maxImages>0)
        clipMaxImages=maxImages;
    else
        clipMaxImages=defaultClipMaxImages;

    int maxSizeMb=mytetraConfig.get_clipperMaxImageSizeMb();
    if(maxSizeMb>0)
        clipMaxImageBytes=static_cast<qint64>(maxSizeMb)*1024*1024;
    else
        clipMaxImageBytes=defaultClipMaxImageSizeBytes;
}


void Clipper::setMaxImages(int count)
{
    if(count>0)
        clipMaxImages=count;
}


void Clipper::setMaxImageSizeBytes(qint64 bytes)
{
    if(bytes>0)
        clipMaxImageBytes=bytes;
}


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


bool Clipper::isInnerImageName(const QString &name)
{
    static QRegularExpression innerPattern("^image\\d{10}[a-z0-9]+\\.png$");

    return innerPattern.match(name).hasMatch();
}


QImage Clipper::imageFromDataUrl(const QString &url)
{
    // Формат data:[<mime>][;base64],<данные>
    int commaPos=url.indexOf(",");
    if(commaPos==-1)
        return QImage();

    QString meta=url.mid(5, commaPos-5);
    QString data=url.mid(commaPos+1);

    QByteArray bytes;
    if(meta.contains(";base64"))
        bytes=QByteArray::fromBase64(data.toLatin1());
    else
        bytes=QByteArray::fromPercentEncoding(data.toLatin1());

    if(bytes.isEmpty() || bytes.size()>clipMaxImageBytes)
        return QImage();

    QImage image;
    if(!image.loadFromData(bytes))
        return QImage();

    return image;
}


QByteArray Clipper::downloadBytes(const QUrl &url)
{
    QNetworkAccessManager manager;

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    QNetworkReply *reply=manager.get(request);

    // Ожидание ответа с таймаутом, иначе фоновая задача может висеть вечно
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(clipDownloadTimeoutMs);
    loop.exec();

    QByteArray result;
    if(timer.isActive() && reply->error()==QNetworkReply::NoError)
    {
        result=reply->readAll();
        if(result.size()>clipMaxImageBytes)
        {
            qWarning() << "Clipper: image" << url.toString() << "exceeds size limit, skipped";
            result.clear();
        }
    }
    else
        qWarning() << "Clipper: can not download image" << url.toString() << reply->errorString();

    timer.stop();
    reply->deleteLater();

    return result;
}


QString Clipper::processImages(const QString &html, QMap<QString, QImage> &images, int *skipped)
{
    if(skipped!=nullptr)
        *skipped=0;

    // Документ нужен для честного поиска картинок, как в ImageFormatter:
    // руками по HTML теги искать нельзя, разметка бывает любая
    QTextDocument textDocument;
    QTextCursor textCursor(&textDocument);
    textCursor.insertHtml(html);

    QTextBlock textBlock=textDocument.begin();
    while(textBlock.isValid())
    {
        bool resetBlock=false;

        QTextBlock::iterator it;
        for(it=textBlock.begin(); !(it.atEnd()); ++it)
        {
            QTextFragment fragment=it.fragment();
            if(!fragment.isValid() || !fragment.charFormat().isImageFormat())
                continue;

            QString imageName=fragment.charFormat().toImageFormat().name();

            // Внутреннее имя значит картинка уже наша, трогать нечего
            if(isInnerImageName(imageName))
                continue;

            // Лимит на число картинок в одном клипе
            if(images.size()>=clipMaxImages)
            {
                qWarning() << "Clipper: too many images, rest left as external references";
                if(skipped!=nullptr)
                    (*skipped)++;
                break;
            }

            QImage image;

            if(imageName.startsWith("data:"))
            {
                // Картинка прямо в HTML
                image=imageFromDataUrl(imageName);
            }
            else
            {
                QUrl imageUrl(imageName);
                QString scheme=imageUrl.scheme().toLower();

                if(scheme=="http" || scheme=="https")
                {
                    // Внешняя картинка скачивается, как при обычной вставке,
                    // только без вопроса (клиппер фоновый, спрашивать некого)
                    QByteArray bytes=downloadBytes(imageUrl);
                    if(!bytes.isEmpty())
                        image.loadFromData(bytes);
                }
                else
                {
                    // Локальный файл подхватывается из ресурсов документа,
                    // сам документ file: ссылки резолвит при вставке HTML
                    QVariant resource=textDocument.resource(QTextDocument::ImageResource,
                                                            QUrl(imageName));
                    image=resource.value<QImage>();
                }
            }

            if(image.isNull())
            {
                qWarning() << "Clipper: image left as external reference:" << imageName;
                if(skipped!=nullptr)
                    (*skipped)++;
                continue;
            }

            // Внутреннее имя и замена ссылки в документе
            QString internalImageName=getUniqueImageName();
            images[internalImageName]=image;

            textDocument.addResource(QTextDocument::ImageResource,
                                     QUrl(internalImageName),
                                     QVariant(image));

            unsigned int position=fragment.position();
            textCursor.setPosition(position);
            textCursor.deleteChar();
            textCursor.insertImage(internalImageName);

            // Документ изменился, перебор начинается сначала
            resetBlock=true;
            break;
        }

        if(resetBlock)
            textBlock=textDocument.begin();
        else
            textBlock=textBlock.next();
    }

    return textDocument.toHtml();
}


bool Clipper::saveImageFiles(const QMap<QString, QImage> &images, const QString &recordDir)
{
    bool result=true;

    QMapIterator<QString, QImage> i(images);
    while(i.hasNext())
    {
        i.next();

        QString fileName=recordDir+"/"+i.key();
        if(!i.value().save(fileName, "PNG"))
        {
            qWarning() << "Clipper: can not save image" << fileName;
            result=false;
        }
    }

    return result;
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
    // Актуальные лимиты картинок из настроек
    reloadLimits();

    const QMimeData *mime=QApplication::clipboard()->mimeData();
    if(mime==nullptr)
        return false;

    QString plainText=mime->text();

    // Пустой буфер клипать не во что. Отдельно лежащая в буфере картинка
    // (скриншот, копия картинки) это тоже содержимое для клипа
    bool hasImageAlone=mime->hasImage() && !mime->hasHtml() && plainText.trimmed().isEmpty();
    if(plainText.trimmed().isEmpty() && !mime->hasHtml() && !hasImageAlone)
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

    // Идентификатор и каталог задаются заранее, иначе сгенерированные
    // внутри вставки значения останутся на копии объекта, и будет
    // неизвестно, куда сохранять файлы картинок
    record.setField("id",   getUniqueId());
    record.setField("dir",  getUniqueId());
    record.setField("file", "text.html");

    // Картинки выносятся в файлы каталога записи под внутренними именами,
    // как при обычной вставке из браузера через редактор
    QMap<QString, QImage> images;
    int skippedImages=0;
    QString noteHtml;
    if(hasImageAlone)
    {
        // В буфере только картинка без текста и HTML
        QImage image=qvariant_cast<QImage>(mime->imageData());
        if(!image.isNull())
        {
            QString internalImageName=getUniqueImageName();
            images[internalImageName]=image;
            noteHtml="<img src=\""+internalImageName+"\" />";
        }
    }
    else if(mime->hasHtml())
        noteHtml=processImages(mime->html(), images, &skippedImages);
    else
        noteHtml=buildNoteHtml(mime);

    record.setText(noteHtml);
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

    // Картинки сохраняются в каталог только что созданной записи.
    // Каталог появляется в момент вставки, раньше писать некуда
    if(!images.isEmpty())
    {
        QString recordDir=mytetraConfig.get_tetradir()+"/base/"+record.getField("dir");
        saveImageFiles(images, recordDir);
    }

    // Обновление вида таблицы (прямая вставка в данные сигналов не дает)
    // и счетчика записей на ветке
    find_object<RecordTableController>("recordTableController")->setTableData(table);
    treeScreen->updateSelectedBranch();

    // О пропущенных картинках пользователь узнает сразу, а не когда
    // недосчитается их спустя полгода. Лимиты меняются в настройках
    if(skippedImages>0)
    {
        qWarning() << "Clipper: skipped" << skippedImages << "image(s), see limits in Tools -> Preferences -> Misc";

        MainWindow *mainWindow=find_object<MainWindow>("mainwindow");
        if(mainWindow!=nullptr)
            mainWindow->showTrayMessage(QCoreApplication::translate("Clipper", "Clipper"),
                                        QCoreApplication::translate("Clipper", "Skipped %1 image(s). Change limits in Tools - Preferences - Misc.").arg(skippedImages));
    }

    qDebug() << "Clipper: note clipped to branch" << branchId << "pos" << pos;

    return true;
}
