// QueryFullProcessImageNameW требует _WIN32_WINNT >= 0x0600.
 // Определять до любых Qt-заголоков: Qt тянет windows.h первым,
 // и поздний _WIN32_WINNT уже ничего не даёт (тот же приём в main.cpp)
#if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x0600)
#undef _WIN32_WINNT
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QKeySequence>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QSslSocket>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
#include <QTimer>

#include <cstring>

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#endif

#include "Clipper.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "models/appConfig/AppConfig.h"
#include "models/recordTable/Record.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "views/tree/TreeScreen.h"
#include "views/recordTable/RecordTableScreen.h"
#include "views/clipBranchDialog/ClipBranchDialog.h"
#include "controllers/recordTable/RecordTableController.h"
#include "models/appConfig/AppConfigStorage.h"
#include "models/appConfig/AppConfigServices.h"
#include "models/appConfig/AppConfigRecordTable.h"
#include "models/appConfig/AppConfigClipper.h"

// Лимиты картинок из настроек. Клиппер фоновый (окно может быть скрыто),
// поэтому вместо вопроса как при обычной вставке действуют лимиты,
// а о пропусках сообщается сразу трей-уведомлением

#include "ClipperPrivate.h"

void Clipper::start(void)
{
    stop();

    if(!AppContext::instance().appConfig()->clipperSection()->get_clipperenable())
    {
        qDebug() << "Clipper: disabled in config";
        return;
    }

    const QString sequence=AppContext::instance().appConfig()->clipperSection()->get_clipperhotkey();
    if(sequence.isEmpty())
    {
        qDebug() << "Clipper: empty hotkey, disabled";
        return;
    }

    if(!grabHotkey(sequence))
        qDebug() << "Clipper: global hotkey unavailable:" << backendStatus();
    else
    {
        hotkeyActive=true;
        hotkeyActiveSequence=sequence;
        qDebug() << "Clipper: global hotkey active:" << sequence;
    }
}

void Clipper::stop(void)
{
    ungrabHotkey();
    hotkeyActive=false;
    hotkeyActiveSequence.clear();
}

void Clipper::rereadSettings(void)
{
    start();
}

bool Clipper::isHotkeyAvailable(void) const
{
    return hotkeyActive;
}

QString Clipper::backendStatus(void) const
{
    // Состояние TLS всегда в строке статуса: без бэкенда докачка
    // картинок по https молча не работает и это иначе не увидеть
    const QString sslInfo=QSslSocket::supportsSsl()
        ? QStringLiteral("SSL: ")+QSslSocket::sslLibraryVersionString()
        : tr("SSL unavailable");

    if(hotkeyActive)
#if defined(Q_OS_WIN)
        return tr("Windows hotkey active: %1").arg(hotkeyActiveSequence)
               + QStringLiteral(" ")+sslInfo;
#else
        return tr("X11 hotkey active: %1").arg(hotkeyActiveSequence)
               + QStringLiteral(" ")+sslInfo;
#endif

#if defined(Q_OS_LINUX)
    if(QGuiApplication::platformName()!="xcb")
        return tr("Global hotkey works only on X11 (current: %1). Use the Clip Now button.")
               .arg(QGuiApplication::platformName())
               + QStringLiteral(" ")+sslInfo;
#endif

    if(!AppContext::instance().appConfig()->clipperSection()->get_clipperenable())
        return tr("Clipper disabled in settings.")
               + QStringLiteral(" ")+sslInfo;

    return tr("Global hotkey unavailable.")
           + QStringLiteral(" ")+sslInfo;
}

QString Clipper::makeTitle(const QString &plainText)
{
    const QStringList lines=plainText.split('\n', Qt::SkipEmptyParts);
    for(const QString &rawLine : lines)
    {
        // Значок картинки без текста (U+FFFC от QTextDocument) именем
        // быть не должен — иначе получаются «пустые» заметки
        QString line=rawLine.trimmed().simplified();
        line.remove(QChar::ObjectReplacementCharacter);
        line=line.trimmed();
        if(line.isEmpty())
            continue;

        if(line.length()<=80)
            return line;
        return line.left(77)+"...";
    }

    return tr("Clipped note ")+QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
}

QByteArray Clipper::contentHash(const QString &normalizedHtml)
{
    // Внутренние имена картинок случайны при каждом клипе — заменить их
    // фиксированным токеном, иначе хеши одного и того же содержимого
    // не сойдутся ни у новой, ни у уже лежащей записи
    QString canonical=normalizedHtml;
    static QRegularExpression internalNameRe("image\\d{10}[a-z0-9]+\\.(png|jpg)");
    canonical.replace(internalNameRe, "image.png");

    return QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha256);
}

QString Clipper::recordIdForHash(const QByteArray &hash)
{
    // Первые 5 байт хеша -> 10 цифр, следующие 10 байт -> 10 символов 0-9a-z.
    // Формат как у getUniqueId(), существующий код работает с id как
    // с непрозрачной строкой
    static const char *alphabet="0123456789abcdefghijklmnopqrstuvwxyz";

    QByteArray bytes=hash;
    while(bytes.size()<15)
        bytes+=hash; // Теоретически короткие хеши — зациклить

    quint64 digits=0;
    for(int i=0; i<5; ++i)
        digits=(digits<<8) | static_cast<quint8>(bytes[i]);
    QString id=QString::number(digits % 10000000000ULL).rightJustified(10, '0');

    for(int i=0; i<10; ++i)
        id+=alphabet[static_cast<quint8>(bytes[5+i]) % 36];

    return id;
}

void Clipper::clipNow(void)
{
    clipNowWithUrl(QString());
}

void Clipper::clipNowWithUrl(const QString &urlHint)
{
    qDebug() << "Clipper: clip requested";

    // Глобальный режим только чтения: записи не создаются
    if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
    {
        const QString message=tr("Read-only mode is enabled.");
        notify(tr("Web Clipper"), message);
        emit clipFinished(false, message);
        return;
    }

    // Актуальные лимиты картинок из настроек
    reloadLimits();

    m_urlHint=urlHint;

    ClipData clipData;
    if(!collectFromClipboard(clipData))
    {
        notify(tr("Web Clipper"), clipData.errorMessage);
        emit clipFinished(false, clipData.errorMessage);
        return;
    }

    if(finishClip(clipData))
        emit clipFinished(true, clipData.errorMessage);
    else
        emit clipFinished(false, clipData.errorMessage);
}

// Общий хвост вставки готового ClipData: ветка, дедуп, запись, трей.
// Итоговое сообщение складывается в clipData.errorMessage

// Общий хвост вставки готового ClipData: ветка, дедуп, запись, трей.
// Итоговое сообщение складывается в clipData.errorMessage
bool Clipper::finishClip(ClipData &clipData)
{
    // Хеш и id считаются до всего: глобальный дедуп ветки не требует
    const QByteArray newHash=contentHash(clipData.html);
    const QString recordId=recordIdForHash(newHash);

    // Дубль отсекается до опроса: зачем выбирать куда,
    // если вставлять нечего. Здесь только глобальный быстрый путь
    // (запись с id хеша уже есть в дереве); медленное сравнение
    // текстов свежих записей ветки останется после ее выбора
    if(KnowTreeModel *model=treeModel())
    {
        if(!recordId.isEmpty() && model->isRecordIdExists(recordId))
        {
            const QString message=AppContext::instance().appConfig()->clipperSection()->get_clipperopendup()
                ? tr("Already clipped, opened existing note.")
                : tr("Already clipped, skipped.");
            notify(tr("Web Clipper"), message);
            if(AppContext::instance().appConfig()->clipperSection()->get_clipperopendup())
                openExistingRecord(recordId);
            clipData.errorMessage=message;
            return false;
        }
    }

    // Режим "спрашивать каждый раз": модальная плитка веток поверх
    // браузера. Отмена — тихий выход без записи и без мусора в базе
    QString askId;
    if(AppContext::instance().appConfig()->clipperSection()->get_clipperbranchmode()==2)
    {
        askId=ClipBranchDialog::askBranch();
        if(askId.isEmpty())
        {
            clipData.errorMessage=tr("Cancelled, nothing saved.");
            return false;
        }
    }

    TreeItem *branchItem=nullptr;
    QString branchError;
    if(!ensureBranch(branchItem, &branchError, askId))
    {
        notify(tr("Web Clipper"), branchError);
        clipData.errorMessage=branchError;
        return false;
    }

    QString matchedId;
    if(isDuplicate(branchItem, recordId, newHash, &matchedId))
    {
        const QString message=AppContext::instance().appConfig()->clipperSection()->get_clipperopendup()
            ? tr("Already clipped, opened existing note.")
            : tr("Already clipped, skipped.");
        notify(tr("Web Clipper"), message);
        if(AppContext::instance().appConfig()->clipperSection()->get_clipperopendup())
            openExistingRecord(matchedId.isEmpty() ? recordId : matchedId);
        clipData.errorMessage=message;
        return false;
    }

    QString storeError;
    if(!storeRecord(branchItem, recordId, clipData.title, clipData.url, clipData.html, clipData.images, &storeError))
    {
        notify(tr("Web Clipper"), storeError);
        clipData.errorMessage=storeError;
        return false;
    }

    // Ветка в недавние: плитка опроса показывает их вместо полного списка
    rememberRecentBranch(branchItem->getField(QStringLiteral("id")));

    QString message=tr("Saved to %1: %2").arg(branchItem->getPathAsNameWithDelimeter(QStringLiteral(" / ")),
                                                     clipData.title);

    // О пропущенных картинках сообщается сразу, а не когда их недосчитаются.
    // Лимиты меняются в Tools - Preferences - Misc
    if(clipData.skippedImages>0)
    {
        message+=tr(" (skipped %1 image(s), see limits in settings)").arg(clipData.skippedImages);
        qWarning() << "Clipper: skipped" << clipData.skippedImages << "image(s)";
    }

    // Новая запись вставлена мимо модели: вид о ней не узнает сам.
    // Если открыта та же ветка — обновить список и засветить запись
    if(TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen"))
    {
        TreeItem *currentItem=(treeScreen->knowTreeModel!=nullptr)
            ? treeScreen->knowTreeModel->getItem(treeScreen->getCurrentItemIndex())
            : nullptr;

        if(currentItem==branchItem)
        {
            if(RecordTableController *rtc=find_object<RecordTableController>("recordTableController"))
                rtc->setTableData(branchItem->recordtableGetTableData());

            if(RecordTableScreen *rtScreen=find_object<RecordTableScreen>("recordTableScreen"))
                rtScreen->setSelectionToId(recordId);
        }
    }

    notify(tr("Web Clipper"), message);
    clipData.errorMessage=message;
    return true;
}

void Clipper::notify(const QString &title, const QString &text)
{
    // Через invokeMethod, чтобы не линковать MainWindow (тесты, moc)
    QObject *mainWindow=find_object<QObject>("mainwindow");
    if(mainWindow)
        QMetaObject::invokeMethod(mainWindow, "showTrayMessage",
                                  Q_ARG(QString, title),
                                  Q_ARG(QString, text));
    else
        qDebug() << "Clipper notify:" << title << text;
}

#if defined(Q_OS_WIN)
// Чтение сырого Windows HTML Format напрямую: Qt отдает только фрагмент,
// а адрес страницы (SourceURL) живет в заголовке формата

// Чтение сырого Windows HTML Format напрямую: Qt отдает только фрагмент,
// а адрес страницы (SourceURL) живет в заголовке формата
QByteArray Clipper::readNativeHtmlFormat(void)
{
    static UINT cfHtmlFormat=0;

    if(cfHtmlFormat==0)
        cfHtmlFormat=RegisterClipboardFormatA("HTML Format");

    if(cfHtmlFormat==0)
        return QByteArray();

    if(!OpenClipboard(nullptr))
        return QByteArray();

    QByteArray raw;

    if(HANDLE handle=GetClipboardData(cfHtmlFormat))
    {
        if(const char *data=static_cast<const char *>(GlobalLock(handle)))
        {
            const SIZE_T bytes=GlobalSize(handle);

            if(bytes>0 && bytes<=INT_MAX)
                raw=QByteArray(data, static_cast<int>(bytes));

            GlobalUnlock(handle);
        }
    }

    CloseClipboard();

    return raw;
}

// Заголовок окна по хендлу для хоткея без выделения
QString Clipper::windowCaption(void *hwnd)
{
    wchar_t buffer[512]={0};

    if(!IsWindow(static_cast<HWND>(hwnd)))
        return QString();

    const int length=GetWindowTextW(static_cast<HWND>(hwnd), buffer, 512);

    if(length<=0)
        return QString();

    return QString::fromWCharArray(buffer, length);
}
#endif // defined(Q_OS_WIN)

// Сбор материала из буфера обмена тем же движком, что ручная вставка:
// HTML нормализуется через QTextDocument, внешние картинки скачиваются
// и получают внутренние имена image<id>.png

// Сбор материала из буфера обмена тем же движком, что ручная вставка:
// HTML нормализуется через QTextDocument, внешние картинки скачиваются
// и получают внутренние имена image<id>.png
bool Clipper::collectFromClipboard(ClipData &clipData)
{
    const QClipboard *clipboard=QApplication::clipboard();
    if(!clipboard)
    {
        clipData.errorMessage=tr("No clipboard available.");
        return false;
    }

    const QMimeData *mimeData=clipboard->mimeData();
    if(!mimeData)
    {
        clipData.errorMessage=tr("Clipboard is empty.");
        return false;
    }

    QString html;
    QMap<QString, QImage> directImages;

    if(mimeData->hasHtml())
    {
        html=mimeData->html();
    }
    else if(mimeData->hasImage())
    {
        const QImage image=qvariant_cast<QImage>(mimeData->imageData());
        if(image.isNull())
        {
            clipData.errorMessage=tr("Clipboard image is not readable.");
            return false;
        }
        const QString internalName=getUniqueImageName(clipImageExtension(image));
        directImages.insert(internalName, clipLimitImageSize(image));
        html=QString("<img src=\"%1\" />").arg(internalName);
    }
    else if(mimeData->hasText())
    {
        const QStringList paragraphs=mimeData->text().split('\n');
        for(const QString &paragraph : paragraphs)
            html+=QString("<p>%1</p>").arg(paragraph.toHtmlEscaped());
    }
    else
    {
        clipData.errorMessage=tr("Clipboard has no text, HTML or image.");
        return false;
    }

    // Нормализация через QTextDocument — как при ручной вставке
    // (onDownloadImagesSuccessfull делает то же самое через временный документ)
    QTextDocument document;
    document.setHtml(html);

    if(!processDocument(document, clipData, directImages.size()))
        return false;

    // Прямая картинка из буфера — тоже в набор
    for(auto it=directImages.constBegin(); it!=directImages.constEnd(); ++it)
        clipData.images.insert(it.key(), it.value());

    // Адрес и название страницы-источника (как CintaNotes).
    // Qt отдает только фрагмент HTML, заголовок с SourceURL отрезается,
    // поэтому на Windows формат читается напрямую через WinAPI
    QString pageUrl;
    QString pageTitle=extractHtmlTitle(html);

#if defined(Q_OS_WIN)
    if(mimeData->hasHtml())
    {
        const QByteArray rawHtml=readNativeHtmlFormat();

        if(!rawHtml.isEmpty())
        {
            pageUrl=parseHtmlFormatSourceUrl(rawHtml);

            const QString nativeTitle=extractHtmlTitle(QString::fromUtf8(rawHtml));

            if(!nativeTitle.isEmpty())
                pageTitle=nativeTitle;
        }
    }
#endif

    QString plainText=document.toPlainText();

    // Название: <title> (во фрагменте его почти никогда нет),
    // затем шапка окна браузера, затем первая строка.
    // Шапка — из момента хоткея, а если ее нет (прямой вызов без хоткея:
    // --control, кнопка проверки) — свежая. Потребляется здесь же,
    // чтобы прямой забор не подхватил чужой заголовок
    if(pageTitle.isEmpty() && m_hotkeyCaption.isEmpty())
        m_hotkeyCaption=foregroundWindowCaption();
    const QString browserTitle=browserTitleFromCaption(m_hotkeyCaption);
    m_hotkeyCaption.clear();

    if(!pageTitle.isEmpty())
        clipData.title=pageTitle;
    else if(!browserTitle.isEmpty())
        clipData.title=browserTitle;
    else
        clipData.title=makeTitle(plainText);

    qDebug() << "Clipper: collect url" << (pageUrl.isEmpty() ? QStringLiteral("-") : pageUrl)
             << "titleSrc" << (!pageTitle.isEmpty() ? "page" : (!browserTitle.isEmpty() ? "window" : "text"))
             << "htmlBytes" << clipData.html.size();

    // Ссылка на источник: явная из --url, затем SourceURL страницы,
    // затем одинокий URL в буфере. Первая ссылка из текста не берётся:
    // в скопированной странице обычно чужой URL, а не адрес самой страницы
    clipData.url=resolveUrl(m_urlHint, pageUrl, plainText);

    clipData.valid=true;
    return true;
}

bool Clipper::processDocument(QTextDocument &document, ClipData &clipData, int alreadyHave)
{
    // Замена внешних картинок на внутренние с докачкой (логика ImageFormatter:
    // имя вида image<10 цифр><символы>.png считается уже внутренним)
    QRegularExpression internalRe("^image\\d{10}[a-z0-9]+\\.(png|jpg)$");
    QMap<QString, QString> refToInternal;
    QMap<QString, QImage> fetchedImages;

    QTextCursor cursor(&document);
    bool restart=true;
    int guard=0;
    while(restart && guard<10000)
    {
        guard++;
        restart=false;

        QTextBlock block=document.begin();
        while(block.isValid() && !restart)
        {
            QTextBlock::iterator it;
            for(it=block.begin(); !(it.atEnd()) && !restart; ++it)
            {
                QTextFragment fragment=it.fragment();
                if(!fragment.isValid())
                    continue;

                if(!fragment.charFormat().isImageFormat())
                    continue;

                const QString refName=fragment.charFormat().toImageFormat().name();
                if(internalRe.match(refName).hasMatch())
                    continue;

                // Лимит числа картинок в одном клипе. Остальные остаются
                // внешними ссылками, о пропуске сообщается в итоге
                if(fetchedImages.size()+alreadyHave>=clipMaxImages)
                {
                    qWarning() << "Clipper: too many images, rest left as external references";
                    clipData.skippedImages++;
                    refToInternal.insert(refName, refName);
                    continue;
                }

                QString internalName;
                if(refToInternal.contains(refName))
                {
                    internalName=refToInternal.value(refName);
                }
                else if(refName.startsWith("data:"))
                {
                    // Картинка прямо в HTML
                    QImage dataImage=imageFromDataUrl(refName);
                    if(dataImage.isNull())
                    {
                        qDebug() << "Clipper: image not loaded, reference kept as is:" << refName;
                        refToInternal.insert(refName, refName);
                        clipData.skippedImages++;
                        continue;
                    }

                    internalName=getUniqueImageName(clipImageExtension(dataImage));
                    refToInternal.insert(refName, internalName);
                    fetchedImages.insert(internalName, clipLimitImageSize(dataImage));
                }
                else
                {
                    // Докачка: http(s) по сети, file:// с диска
                    bool fetchOk=false;
                    QByteArray bytes=fetchUrl(refName, &fetchOk);

                    QImage image;
                    if(fetchOk)
                        image.loadFromData(bytes);

                    if(image.isNull())
                    {
                        qDebug() << "Clipper: image not loaded, reference kept as is:" << refName;
                        refToInternal.insert(refName, refName);
                        clipData.skippedImages++;
                        continue;
                    }

                    internalName=getUniqueImageName(clipImageExtension(image));
                    refToInternal.insert(refName, internalName);
                    fetchedImages.insert(internalName, clipLimitImageSize(image));
                }

                if(internalName==refName)
                    continue;

                const unsigned int position=fragment.position();
                cursor.setPosition(position);
                cursor.deleteChar();
                cursor.insertImage(internalName);
                restart=true;
                break; // Документ изменен, итератор инвалидирован:
                       // дальше только полный перезапуск обхода
            }

            if(!restart)
                block=block.next();
        }
    }

    // Готовый HTML и докачанные картинки. Прямую картинку из буфера
    // добавляет вызывающий collectFromClipboard
    clipData.html=document.toHtml();
    clipData.images=fetchedImages;

    return true;
}

// Скачивание по URL: http(s) через QNetworkAccessManager с таймаутом,
// file:// — локальный файл по явному действию пользователя (клиппер
// запускается вручную; вставка своей картинки — штатный сценарий).
// Без модальных диалогов (фоновая работа)

// Скачивание по URL: http(s) через QNetworkAccessManager с таймаутом,
// file:// — локальный файл по явному действию пользователя (клиппер
// запускается вручную; вставка своей картинки — штатный сценарий).
// Без модальных диалогов (фоновая работа)
QByteArray Clipper::fetchUrl(const QString &url, bool *ok)
{
    if(ok)
        *ok=false;

    if(url.startsWith("file://"))
    {
        QFile file(QUrl(url).toLocalFile());
        if(!file.open(QIODevice::ReadOnly))
            return QByteArray();

        // Тот же лимит, что для сетевых картинок: локальный файл
        // не должен раздувать память заметки. Размер проверяется
        // до readAll(), чтобы не тянуть гигабайты в память
        if(file.size()>clipMaxImageBytes)
            return QByteArray();

        const QByteArray data=file.readAll();

        if(data.size()>clipMaxImageBytes)
            return QByteArray();

        if(ok)
            *ok=!data.isEmpty();

        return data;
    }

    if(!url.startsWith("http://") && !url.startsWith("https://"))
        return QByteArray();

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(url)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply=manager.get(request);

    // NoPeerCertificate при возобновлении TLS-сессии — не ошибка цепочки:
    // сертификат уже проверялся в полном handshake. Остальные ошибки
    // фатальны, отключать проверку целиком нельзя
    QObject::connect(&manager, &QNetworkAccessManager::sslErrors,
                     [](QNetworkReply *r, const QList<QSslError> &errors)
                     {
                         // Явно разрешенный небезопасный режим из настроек
                         if(AppContext::instance().appConfig()->servicesSection()->get_tlsIgnoreInvalidCertificates())
                         {
                             QStringList details;

                             foreach(const QSslError &error, errors)
                                 details.append(error.errorString());

                             qWarning() << "Clipper: TLS errors ignored by setting:"
                                        << details.join("; ");

                             r->ignoreSslErrors();
                             return;
                         }

                         bool onlyMissing=true;

                         foreach(const QSslError &error, errors)
                             if(error.error()!=QSslError::NoPeerCertificate)
                                 onlyMissing=false;

                         if(onlyMissing)
                         {
                             r->ignoreSslErrors(errors);
                             return;
                         }

                         QStringList details;

                         foreach(const QSslError &error, errors)
                             details.append(error.errorString());

                         qWarning() << "Clipper: TLS certificate verification failed:"
                                    << details.join("; ");

                         r->abort();
                     });

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(15000);
    loop.exec();

    QByteArray data;
    if(timer.isActive() && reply->error()==QNetworkReply::NoError)
    {
        data=reply->readAll();

        if(data.size()>clipMaxImageBytes)
        {
            qDebug() << "Clipper: image exceeds size limit:" << url;
            data.clear();
            if(ok)
                *ok=false;
        }
        else if(ok)
            *ok=!data.isEmpty();
    }
    else
        qDebug() << "Clipper: download failed:" << url << reply->errorString();

    reply->deleteLater();
    return data;
}

// Ветка unsorted_notes: по запомненному id, иначе по имени среди корневых,
// иначе создание + запоминание id (переживает переименования)

// Ветка unsorted_notes: по запомненному id, иначе по имени среди корневых,
// иначе создание + запоминание id (переживает переименования)
bool Clipper::ensureBranch(    TreeItem* &branchItem, QString *errorMessage,
                              const QString &forcedId)
{
    branchItem=nullptr;

    KnowTreeModel *model=treeModel();
    if(!model)
    {
        if(errorMessage)
            *errorMessage=tr("Knowledge tree is not ready.");
        return false;
    }

    // Разовый выбор из опроса "куда вставить": обязан существовать,
    // взят из живого дерева секунду назад
    if(!forcedId.isEmpty())
    {
        branchItem=model->getItemById(forcedId);
        if(!branchItem)
        {
            if(errorMessage)
                *errorMessage=tr("Selected branch is gone.");
            return false;
        }
        return true;
    }

    const QString savedId=AppContext::instance().appConfig()->clipperSection()->get_clipperbranchid();
    if(!savedId.isEmpty())
    {
        TreeItem *savedItem=model->getItemById(savedId);
        if(savedItem)
        {
            branchItem=savedItem;
            return true;
        }
    }

    TreeItem *rootItem=model->getItem(QModelIndex());
    if(!rootItem)
    {
        if(errorMessage)
            *errorMessage=tr("Knowledge tree is not ready.");
        return false;
    }

    for(int i=0; i<rootItem->childCount(); ++i)
    {
        if(rootItem->child(i)->getField("name")=="unsorted_notes")
        {
            branchItem=rootItem->child(i);
            AppContext::instance().appConfig()->clipperSection()->set_clipperbranchid(branchItem->getField("id"));
            return true;
        }
    }

    QMap<QString, QString> branchFields;
    branchFields.insert(QStringLiteral("id"), getUniqueId());
    branchFields.insert(QStringLiteral("name"), QStringLiteral("unsorted_notes"));
    model->addNewChildBranch(QModelIndex(), branchFields);

    branchItem=rootItem->child(rootItem->childCount()-1);
    if(!branchItem)
    {
        if(errorMessage)
            *errorMessage=tr("Can not create unsorted_notes branch.");
        return false;
    }

    AppContext::instance().appConfig()->clipperSection()->set_clipperbranchid(branchItem->getField("id"));
    saveTree();
    return true;
}

// Ветка в историю clipperrecent: пары id:count через ";", свежие впереди.
// Один список кормит оба режима плиток: последние — как есть,
// частые — сортировкой по счетчику. Лимит 32 с запасом под оба вида

// Ветка в историю clipperrecent: пары id:count через ";", свежие впереди.
// Один список кормит оба режима плиток: последние — как есть,
// частые — сортировкой по счетчику. Лимит 32 с запасом под оба вида
void Clipper::rememberRecentBranch(const QString &branchId)
{
    if(branchId.isEmpty())
        return;

    QMap<QString, int> counts;
    QStringList order;
    const QStringList pairs=AppContext::instance().appConfig()->clipperSection()->get_clipperrecent()
                             .split(QChar(';'), Qt::SkipEmptyParts);
    for(const QString &pair : pairs)
    {
        const int colon=pair.lastIndexOf(QChar(':'));
        QString id;
        int count=1;
        if(colon>0)
        {
            id=pair.left(colon);
            count=pair.mid(colon+1).toInt();
            if(count<1)
                count=1;
        }
        else
            id=pair;

        if(id.isEmpty() || counts.contains(id))
            continue;

        counts[id]=count;
        order << id;
    }

    counts[branchId]=counts.value(branchId, 0)+1;
    order.removeAll(branchId);
    order.prepend(branchId);
    while(order.size()>32)
        order.removeLast();

    QStringList result;
    for(const QString &id : order)
        result << id+QChar(':')+QString::number(counts.value(id, 1));

    AppContext::instance().appConfig()->clipperSection()->set_clipperrecent(result.join(QChar(';')));
}

// Защита от повторной вставки: быстрый путь — запись с id из хеша уже
// есть в дереве; медленный — сравнение хешей текстов свежих 2000 записей
// ветки (ловит дубли, вставленные до хеш-id). Ограничение сканирования —
// свежие 2000. Найденный id отдается через matchedId для прыжка к оригиналу

// Защита от повторной вставки: быстрый путь — запись с id из хеша уже
// есть в дереве; медленный — сравнение хешей текстов свежих 2000 записей
// ветки (ловит дубли, вставленные до хеш-id). Ограничение сканирования —
// свежие 2000. Найденный id отдается через matchedId для прыжка к оригиналу
bool Clipper::isDuplicate(TreeItem *branchItem, const QString &recordId, const QByteArray &hash,
                          QString *matchedId)
{
    if(!branchItem || hash.isEmpty())
        return false;

    KnowTreeModel *model=treeModel();
    if(model && !recordId.isEmpty() && model->isRecordIdExists(recordId))
    {
        if(matchedId!=nullptr)
            *matchedId=recordId;
        return true;
    }

    RecordTableData *table=branchItem->recordtableGetTableData();
    if(!table)
        return false;

    const int total=table->size();
    const int scanFrom=total>2000 ? total-2000 : 0;
    for(int pos=total-1; pos>=static_cast<int>(scanFrom); --pos)
    {
        Record *record=table->getRecord(pos);
        if(!record)
            continue;

        const QString dir=record->getField("dir");
        QString file=record->getField("file");
        if(file.isEmpty())
            file=QStringLiteral("text.html");
        if(dir.isEmpty())
            continue;

        QFile textFile(AppContext::instance().appConfig()->storageSection()->get_tetradir()+"/base/"+dir+"/"+file);
        if(!textFile.open(QIODevice::ReadOnly))
            continue;

        const QByteArray existingHash=QCryptographicHash::hash(textFile.readAll(),
                                                               QCryptographicHash::Sha256);
        if(existingHash==hash)
        {
            if(matchedId!=nullptr)
                *matchedId=record->getField(QStringLiteral("id"));
            return true;
        }
    }

    return false;
}

// Прыжок к уже вставленной заметке: путь резолвится вживую
// (ветку могли переместить), окно поверх не тянем

// Прыжок к уже вставленной заметке: путь резолвится вживую
// (ветку могли переместить), окно поверх не тянем
void Clipper::openExistingRecord(const QString &recordId)
{
    KnowTreeModel *model=treeModel();
    if(model==nullptr || recordId.isEmpty())
        return;

    const QStringList path=model->getRecordPath(recordId);
    if(path.isEmpty() || !model->isItemValid(path))
        return;

    // Через invokeMethod, чтобы не линковать MainWindow (тесты, moc)
    QObject *mainWindow=find_object<QObject>("mainwindow");
    if(mainWindow)
        QMetaObject::invokeMethod(mainWindow, "setTreeAndRecordtablePositions",
                                  Q_ARG(QStringList, path),
                                  Q_ARG(QString, recordId));
}

// Сохранение записи штатными средствами модели (как ручное создание):
// FAT-запись -> insertNewRecord -> PNG картинок в каталог -> saveKnowTree.
// Id записи — из хеша содержимого: повторный клип того же получит тот же
// id и отсечётся проверкой до вставки (insert всё равно бы заменил id
// при коллизии, так что гонка безопасна)

// Сохранение записи штатными средствами модели (как ручное создание):
// FAT-запись -> insertNewRecord -> PNG картинок в каталог -> saveKnowTree.
// Id записи — из хеша содержимого: повторный клип того же получит тот же
// id и отсечётся проверкой до вставки (insert всё равно бы заменил id
// при коллизии, так что гонка безопасна)
bool Clipper::storeRecord(TreeItem *branchItem, const QString &recordId,
                          const QString &title,
                          const QString &url, const QString &html,
                          const QMap<QString, QImage> &images,
                          QString *errorMessage)
{
    if(!branchItem)
    {
        if(errorMessage)
            *errorMessage=tr("Target branch is not ready.");
        return false;
    }

    RecordTableData *table=branchItem->recordtableGetTableData();
    if(!table)
    {
        if(errorMessage)
            *errorMessage=tr("Target branch has no record table.");
        return false;
    }

    Record record;
    record.switchToFat();
    record.setText(html);
    record.setField("id", recordId);
    record.setField("dir", getUniqueId());
    record.setField("file", "text.html");
    record.setField("name", title);
    record.setField("author", QString());
    record.setField("url", url);
    record.setField("tags", QString());

    const int pos=table->insertNewRecord(GlobalParameters::AddNewRecordBehavior::ADD_TO_END,
                                         0, record);
    if(pos<0)
    {
        if(errorMessage)
            *errorMessage=tr("Can not insert note into unsorted_notes.");
        return false;
    }

    Record *storedRecord=table->getRecord(pos);
    if(!storedRecord)
    {
        if(errorMessage)
            *errorMessage=tr("Can not insert note into unsorted_notes.");
        return false;
    }

    // Картинки — PNG-файлами в каталог записи (как saveTextareaImages)
    const QString recordDir=AppContext::instance().appConfig()->storageSection()->get_tetradir()+"/base/"+storedRecord->getField("dir");
    for(auto it=images.constBegin(); it!=images.constEnd(); ++it)
    {
        if(!it.value().save(recordDir+"/"+it.key(), "PNG"))
            qDebug() << "Clipper: can not save image" << it.key();
    }

    saveTree();

    return true;
}

// --- Глобальный хоткей (X11 через dlopen, без новых зависимостей сборки) ---

