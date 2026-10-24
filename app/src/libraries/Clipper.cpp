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
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
#include <QTimer>

#include <cstring>
#if defined(Q_OS_LINUX)
#include <dlfcn.h>
#endif

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
#include "libraries/wyedit/EditorConfig.h"
#include "models/appConfig/AppConfig.h"
#include "models/recordTable/Record.h"
#include "models/recordTable/RecordTableData.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "views/tree/TreeScreen.h"
#include "views/recordTable/RecordTableScreen.h"
#include "views/clipBranchDialog/ClipBranchDialog.h"
#include "controllers/recordTable/RecordTableController.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;


// Лимиты картинок из настроек. Клиппер фоновый (окно может быть скрыто),
// поэтому вместо вопроса как при обычной вставке действуют лимиты,
// а о пропусках сообщается сразу трей-уведомлением
static const int defaultClipMaxImages=20;
static const qint64 defaultClipMaxImageSizeBytes=5*1024*1024;

static int clipMaxImages=defaultClipMaxImages;
static qint64 clipMaxImageBytes=defaultClipMaxImageSizeBytes;


// Расширение новой картинки в клипере: настройка формата редактора,
// картинки с прозрачностью всегда PNG (у JPG нет альфа-канала).
// Конфига может не быть в тестах — тогда png
static QString clipImageExtension(const QImage &image)
{
    EditorConfig *editorConfig=find_object<EditorConfig>("editorconfig");

    if(editorConfig==nullptr)
        return QStringLiteral("png");

    return editorConfig->imageExtensionFor(image);
}


// Ужатие картинки клипера до лимита длинной стороны из настройки.
// Без конфига возвращается исходник
static QImage clipLimitImageSize(const QImage &image)
{
    EditorConfig *editorConfig=find_object<EditorConfig>("editorconfig");

    if(editorConfig==nullptr)
        return image;

    return editorConfig->limitImageSize(image);
}


#if defined(Q_OS_WIN)
namespace {

// Id единственного глобального хоткея в потоке GUI
const int clipperWinHotkeyId=1;

// Разбор "Ctrl+Alt+V" в модификаторы и виртуальный код для RegisterHotKey.
// Только латинские клавиши, как и в хоткей-редакторе настроек
bool parseWinHotkey(const QString &sequence, UINT &modifiers, UINT &vkCode)
{
    modifiers=0;
    vkCode=0;

    const QStringList parts=sequence.split('+', Qt::SkipEmptyParts);
    if(parts.isEmpty())
        return false;

    for(int i=0; i<parts.size()-1; ++i)
    {
        const QString mod=parts[i].trimmed().toLower();
        if(mod=="ctrl")
            modifiers|=MOD_CONTROL;
        else if(mod=="alt")
            modifiers|=MOD_ALT;
        else if(mod=="shift")
            modifiers|=MOD_SHIFT;
        else if(mod=="meta" || mod=="win")
            modifiers|=MOD_WIN;
        else
            return false;
    }

    const QString key=parts.last().trimmed();
    if(key.size()==1)
    {
        const ushort ch=key[0].toUpper().unicode();
        if((ch>='A' && ch<='Z') || (ch>='0' && ch<='9'))
        {
            vkCode=ch;
            return true;
        }
        if(ch==' ')
        {
            vkCode=VK_SPACE;
            return true;
        }
        return false;
    }

    if(key.size()>1 && (key[0]=='F' || key[0]=='f'))
    {
        bool ok=false;
        const int n=key.mid(1).toInt(&ok);
        if(ok && n>=1 && n<=24)
        {
            vkCode=VK_F1+n-1;
            return true;
        }
        return false;
    }

    const QString lower=key.toLower();
    if(lower=="tab")            { vkCode=VK_TAB;    return true; }
    if(lower=="return" ||
       lower=="enter")          { vkCode=VK_RETURN; return true; }
    if(lower=="escape" ||
       lower=="esc")            { vkCode=VK_ESCAPE; return true; }
    if(lower=="backspace")      { vkCode=VK_BACK;   return true; }
    if(lower=="delete")         { vkCode=VK_DELETE; return true; }
    if(lower=="insert")         { vkCode=VK_INSERT; return true; }
    if(lower=="home")           { vkCode=VK_HOME;   return true; }
    if(lower=="end")            { vkCode=VK_END;    return true; }
    if(lower=="pageup")         { vkCode=VK_PRIOR;  return true; }
    if(lower=="pagedown")       { vkCode=VK_NEXT;   return true; }
    if(lower=="left")           { vkCode=VK_LEFT;   return true; }
    if(lower=="up")             { vkCode=VK_UP;     return true; }
    if(lower=="right")          { vkCode=VK_RIGHT;  return true; }
    if(lower=="down")           { vkCode=VK_DOWN;   return true; }
    if(lower=="space")          { vkCode=VK_SPACE;  return true; }

    return false;
}

// Диагностика SendInput: сколько событий реально вставлено.
// Объявление вперёд: определение ниже, после обеих эмуляций
static void logSendInput(const char *what, UINT sent);

// Эмуляция Ctrl+C чтобы забрать текущее выделение (как CintaNotes).
// Копирование чужое выделение не портит, повторная вставка того же
// отсекается дедупом по хешу
void sendCopyKeys(void)
{
    INPUT input[4]={};
    for(int i=0; i<4; ++i)
        input[i].type=INPUT_KEYBOARD;
    input[0].ki.wVk=VK_CONTROL;
    input[1].ki.wVk='C';
    input[2].ki.wVk='C';
    input[2].ki.dwFlags=KEYEVENTF_KEYUP;
    input[3].ki.wVk=VK_CONTROL;
    input[3].ki.dwFlags=KEYEVENTF_KEYUP;
    logSendInput("Ctrl+C", SendInput(4, input, sizeof(INPUT)));
}

void logSendInput(const char *what, UINT sent)
{
    if(sent!=4)
        qDebug() << "Clipper: SendInput" << what << "inserted" << sent
                 << "of 4, error" << GetLastError();
}

} // namespace


// Приёмник WM_HOTKEY из очереди потока GUI (без Q_OBJECT: дёргает слот напрямую)
class ClipperWinFilter : public QAbstractNativeEventFilter
{
public:
    explicit ClipperWinFilter(Clipper *owner) : m_owner(owner) {}

    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override
    {
        Q_UNUSED(result);
        if(eventType=="windows_generic_MSG")
        {
            MSG *msg=static_cast<MSG*>(message);
            if(msg->message==WM_HOTKEY && msg->wParam==clipperWinHotkeyId)
            {
                QMetaObject::invokeMethod(m_owner, "onWinHotkey", Qt::QueuedConnection);
                return false;
            }
        }
        return false;
    }

private:
    Clipper *m_owner;
};
#endif // defined(Q_OS_WIN)


void Clipper::reloadLimits(void)
{
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


bool Clipper::looksLikeUrl(const QString &value)
{
    QString trimmed=value.trimmed();

    return trimmed.startsWith("http://") || trimmed.startsWith("https://");
}


QString Clipper::extractUrl(const QString &text)
{
    static QRegularExpression urlPattern("https?://\\S+");

    QRegularExpressionMatch match=urlPattern.match(text);
    if(!match.hasMatch())
        return QString();

    QString url=match.captured(0);

    while(!url.isEmpty() && QString(".,;:!?)]}'\"").contains(url.right(1)))
        url.chop(1);

    return url;
}


// Адрес страницы из сырого Windows HTML Format (заголовок SourceURL).
// Возвращается как есть, годность проверяет looksLikeUrl у вызывателя
QString Clipper::parseHtmlFormatSourceUrl(const QByteArray &raw)
{
    const QList<QByteArray> lines=raw.split('\n');

    for(QByteArray line : lines)
    {
        line=line.trimmed();

        if(line.size()>10 && line.left(10).compare("SourceURL:", Qt::CaseInsensitive)==0)
            return QString::fromLatin1(line.mid(10).trimmed());
    }

    return QString();
}


QString Clipper::resolveUrl(const QString &urlHint, const QString &sourceUrl,
                              const QString &plainText)
{
    if(looksLikeUrl(urlHint))
        return urlHint.trimmed();

    if(looksLikeUrl(sourceUrl))
        return sourceUrl.trimmed();

    if(looksLikeUrl(plainText.trimmed()))
        return plainText.trimmed();

    return QString();
}


QString Clipper::extractHtmlTitle(const QString &html)
{
    static const QRegularExpression titleRe("<title[^>]*>(.*?)</title>",
                                            QRegularExpression::CaseInsensitiveOption |
                                            QRegularExpression::DotMatchesEverythingOption);

    const QRegularExpressionMatch match=titleRe.match(html);

    if(!match.hasMatch())
        return QString();

    QTextDocument document;
    document.setHtml(match.captured(1));

    QString title=document.toPlainText().trimmed().simplified();

    if(title.length()<=80)
        return title;

    return title.left(77)+"...";
}


// Суффикс браузера в шапке окна (" - Google Chrome" и т.п.).
// Дефис, en dash, em dash и пара браузеров кириллицей — файл UTF-8,
// поэтому литералами, а не hex-последовательностями
static const QRegularExpression &browserCaptionSuffix(void)
{
    static const QRegularExpression browserRe("\\s+[—–-]\\s*"
                                              "(Google Chrome|Chromium|Mozilla Firefox|Firefox|"
                                              "Microsoft Edge|Opera|Vivaldi|Brave|"
                                              "Яндекс Браузер|Yandex)"
                                              "\\s*$",
                                              QRegularExpression::CaseInsensitiveOption);
    return browserRe;
}


// Название самой веб-страницы из заголовка окна: отрезается суффикс
// вида " - Google Chrome". Не браузер — заголовок как есть
QString Clipper::cleanWindowCaption(const QString &caption)
{
    const QString cleaned=caption.trimmed().remove(browserCaptionSuffix()).trimmed();

    if(!cleaned.isEmpty())
        return cleaned;

    return caption.trimmed();
}


// Заголовок окна именно браузера: название страницы без суффикса.
// Пусто если суффикса известного браузера нет (терминал, редактор) —
// тогда честнее первая строка, а не шапка чужого окна
QString Clipper::browserTitleFromCaption(const QString &caption)
{
    const QString trimmed=caption.trimmed();
    if(trimmed.isEmpty())
        return QString();

    const QRegularExpressionMatch match=browserCaptionSuffix().match(trimmed);
    if(!match.hasMatch())
        return QString();

    return trimmed.left(match.capturedStart()).trimmed();
}


QImage Clipper::imageFromDataUrl(const QString &url)
{
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


void Clipper::setTestModel(KnowTreeModel *model)
{
    testModel=model;
}


KnowTreeModel *Clipper::treeModel(void) const
{
    if(testModel)
        return testModel;

    TreeScreen *treeScreen=globalParameters.getTreeScreen();
    if(!treeScreen)
        return nullptr;
    return treeScreen->knowTreeModel;
}


void Clipper::saveTree(void) const
{
    if(testModel)
        return;

    TreeScreen *treeScreen=globalParameters.getTreeScreen();
    if(treeScreen)
    {
        treeScreen->saveKnowTree();

        // Пачка новых записей: кеш статистики в пересчет
        treeScreen->knowTreeModel->invalidateStats();
    }
}




Clipper::Clipper(QObject *parent) : QObject(parent)
{

}


Clipper::~Clipper()
{
    stop();
}


void Clipper::start(void)
{
    stop();

    if(!mytetraConfig.get_clipperenable())
    {
        qDebug() << "Clipper: disabled in config";
        return;
    }

    const QString sequence=mytetraConfig.get_clipperhotkey();
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
    if(hotkeyActive)
#if defined(Q_OS_WIN)
        return tr("Windows hotkey active: %1").arg(hotkeyActiveSequence);
#else
        return tr("X11 hotkey active: %1").arg(hotkeyActiveSequence);
#endif

#if defined(Q_OS_LINUX)
    if(QGuiApplication::platformName()!="xcb")
        return tr("Global hotkey works only on X11 (current: %1). Use the Clip Now button.")
               .arg(QGuiApplication::platformName());
#endif

    if(!mytetraConfig.get_clipperenable())
        return tr("Clipper disabled in settings.");

    return tr("Global hotkey unavailable.");
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
    if(mytetraConfig.get_readOnly())
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


// Заметка из заголовка окна без выделения (хоткей в браузере):
// название страницы, ссылка пустая — взять её неоткуда
void Clipper::clipWindowTitle(const QString &caption)
{
    qDebug() << "Clipper: clip window title";

    if(mytetraConfig.get_readOnly())
    {
        const QString message=tr("Read-only mode is enabled.");
        notify(tr("Web Clipper"), message);
        emit clipFinished(false, message);
        return;
    }

    reloadLimits();

    m_urlHint.clear();

    ClipData clipData;
    clipData.title=makeTitle(caption);
    clipData.url=QString();
    clipData.html=QString("<p>%1</p>").arg(caption.toHtmlEscaped());
    clipData.valid=true;

    if(finishClip(clipData))
        emit clipFinished(true, clipData.errorMessage);
    else
        emit clipFinished(false, clipData.errorMessage);
}


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
            const QString message=mytetraConfig.get_clipperopendup()
                ? tr("Already clipped, opened existing note.")
                : tr("Already clipped, skipped.");
            notify(tr("Web Clipper"), message);
            if(mytetraConfig.get_clipperopendup())
                openExistingRecord(recordId);
            clipData.errorMessage=message;
            return false;
        }
    }

    // Режим "спрашивать каждый раз": модальная плитка веток поверх
    // браузера. Отмена — тихий выход без записи и без мусора в базе
    QString askId;
    if(mytetraConfig.get_clipperbranchmode()==2)
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
        const QString message=mytetraConfig.get_clipperopendup()
            ? tr("Already clipped, opened existing note.")
            : tr("Already clipped, skipped.");
        notify(tr("Web Clipper"), message);
        if(mytetraConfig.get_clipperopendup())
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


// Сведения о процессе для диагностики UIPI: ввод из менее
// привилегированного процесса в более привилегированный режется молча
struct WinProcInfo
{
    QString exe;
    bool elevated=false;
    bool known=false;
};

static WinProcInfo processInfo(DWORD pid)
{
    WinProcInfo info;

    if(pid==0)
        return info;

    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

    if(process==nullptr)
        return info;

    wchar_t path[1024]={0};
    DWORD pathSize=1024;

    if(QueryFullProcessImageNameW(process, 0, path, &pathSize) && pathSize>0)
    {
        info.exe=QString::fromWCharArray(path, pathSize);
        info.exe=info.exe.section(QLatin1Char('\\'), -1).section(QLatin1Char('/'), -1);
    }

    HANDLE token=nullptr;

    if(OpenProcessToken(process, TOKEN_QUERY, &token))
    {
        TOKEN_ELEVATION elevation={0};
        DWORD returned=0;

        if(GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &returned) &&
           returned==sizeof(elevation))
        {
            info.elevated=(elevation.TokenIsElevated!=0);
            info.known=true;
        }

        CloseHandle(token);
    }

    CloseHandle(process);

    return info;
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
                         if(mytetraConfig.get_tlsIgnoreInvalidCertificates())
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

    const QString savedId=mytetraConfig.get_clipperbranchid();
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
            mytetraConfig.set_clipperbranchid(branchItem->getField("id"));
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

    mytetraConfig.set_clipperbranchid(branchItem->getField("id"));
    saveTree();
    return true;
}


// Ветка в историю clipperrecent: пары id:count через ";", свежие впереди.
// Один список кормит оба режима плиток: последние — как есть,
// частые — сортировкой по счетчику. Лимит 32 с запасом под оба вида
void Clipper::rememberRecentBranch(const QString &branchId)
{
    if(branchId.isEmpty())
        return;

    QMap<QString, int> counts;
    QStringList order;
    const QStringList pairs=mytetraConfig.get_clipperrecent()
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

    mytetraConfig.set_clipperrecent(result.join(QChar(';')));
}


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

        QFile textFile(mytetraConfig.get_tetradir()+"/base/"+dir+"/"+file);
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
    const QString recordDir=mytetraConfig.get_tetradir()+"/base/"+storedRecord->getField("dir");
    for(auto it=images.constBegin(); it!=images.constEnd(); ++it)
    {
        if(!it.value().save(recordDir+"/"+it.key(), "PNG"))
            qDebug() << "Clipper: can not save image" << it.key();
    }

    saveTree();

    return true;
}


// --- Глобальный хоткей (X11 через dlopen, без новых зависимостей сборки) ---

#if defined(Q_OS_LINUX)

typedef void *XDisplayPtr;
typedef unsigned long XWindow;
typedef int XKeyCodeInt;

enum
{
    X11KeyPress = 2,
    X11KeyPressMask = 1,
    X11ShiftMask = 1 << 0,
    X11LockMask = 1 << 1,
    X11ControlMask = 1 << 2,
    X11Mod1Mask = 1 << 3,
    X11Mod2Mask = 1 << 4,
    X11Mod4Mask = 1 << 6,
    X11GrabModeAsync = 1
};

struct XKeyEventCompat
{
    int type;
    unsigned long serial;
    int send_event;
    int pad1;
    void *display;
    unsigned long window;
    unsigned long root;
    unsigned long subwindow;
    unsigned long time;
    int x;
    int y;
    int x_root;
    int y_root;
    unsigned int state;
    unsigned int keycode;
    int same_screen;
    int pad2;

    // XNextEvent копирует в буфер весь XEvent (24 long = 192 байта),
    // а не только клавишу (96 байт). Без хвоста первое же событие
    // рвало стек (stack smashing на первом хоткее под X11)
    long tail[12];
};

// Совместимая структура обязана совпадать с XEvent размером:
// иначе XNextEvent снова начнет писать мимо буфера
static_assert(sizeof(XKeyEventCompat)==24*sizeof(long),
              "XKeyEventCompat must match XEvent size");

typedef void *(*XOpenDisplayFunc)(const char *);
typedef int (*XCloseDisplayFunc)(void *);
typedef unsigned long (*XDefaultRootWindowFunc)(void *);
typedef int (*XKeysymToKeycodeFunc)(void *, unsigned long);
typedef unsigned long (*XStringToKeysymFunc)(const char *);
typedef int (*XGrabKeyFunc)(void *, int, unsigned int, unsigned long, int, int, int);
typedef int (*XUngrabKeyFunc)(void *, int, unsigned int, unsigned long);
typedef int (*XSelectInputFunc)(void *, unsigned long, long);
typedef int (*XNextEventFunc)(void *, void *);
typedef int (*XPendingFunc)(void *);
typedef int (*XConnectionNumberFunc)(void *);
typedef int (*XFlushFunc)(void *);

#endif


unsigned int Clipper::qtModsToX11(int qtModifiers)
{
    unsigned int mask=0;
    if(qtModifiers & Qt::ShiftModifier)
        mask|=1 << 0;
    if(qtModifiers & Qt::ControlModifier)
        mask|=1 << 2;
    if(qtModifiers & Qt::AltModifier)
        mask|=1 << 3;
    if(qtModifiers & Qt::MetaModifier)
        mask|=1 << 6;
    return mask;
}


unsigned long Clipper::qtKeyToKeysym(int qtKey)
{
    // Латиница и цифры: keysym совпадают с Unicode
    if((qtKey>=0x20 && qtKey<=0x7e))
        return static_cast<unsigned long>(qtKey);

    switch(qtKey)
    {
        case Qt::Key_Return:
        case Qt::Key_Enter:   return 0xff0d;
        case Qt::Key_Escape:  return 0xff1b;
        case Qt::Key_Tab:     return 0xff09;
        case Qt::Key_Backtab: return 0xff09;
        case Qt::Key_Backspace: return 0xff08;
        case Qt::Key_Delete:  return 0xffff;
        case Qt::Key_Insert:  return 0xff63;
        case Qt::Key_Home:    return 0xff50;
        case Qt::Key_End:     return 0xff57;
        case Qt::Key_PageUp:  return 0xff55;
        case Qt::Key_PageDown:return 0xff56;
        case Qt::Key_Space:   return 0x20;
        default: break;
    }

    if(qtKey>=Qt::Key_F1 && qtKey<=Qt::Key_F35)
        return 0xffbd+(static_cast<unsigned long>(qtKey)-static_cast<unsigned long>(Qt::Key_F1));

    return 0;
}


bool Clipper::grabHotkey(const QString &sequence)
{
#if defined(Q_OS_WIN)
    ungrabHotkey();

    UINT modifiers=0;
    UINT vkCode=0;
    if(!parseWinHotkey(sequence, modifiers, vkCode))
        return false;

    // Хоткей на очередь потока GUI, без окна. Поток один, id один
    if(!RegisterHotKey(nullptr, clipperWinHotkeyId, modifiers, vkCode))
        return false;

    winFilter=new ClipperWinFilter(this);
    QCoreApplication::instance()->installNativeEventFilter(winFilter);
    return true;
#elif !defined(Q_OS_LINUX)
    Q_UNUSED(sequence)
    return false;
#else
    ungrabHotkey();

    if(QGuiApplication::platformName()!="xcb")
        return false;

    x11Lib=dlopen("libX11.so.6", RTLD_NOW);
    if(!x11Lib)
        x11Lib=dlopen("libX11.so", RTLD_NOW);
    if(!x11Lib)
        return false;

    XOpenDisplayFunc pOpenDisplay=(XOpenDisplayFunc)dlsym(x11Lib, "XOpenDisplay");
    XDefaultRootWindowFunc pRootWindow=(XDefaultRootWindowFunc)dlsym(x11Lib, "XDefaultRootWindow");
    XStringToKeysymFunc pStringToKeysym=(XStringToKeysymFunc)dlsym(x11Lib, "XStringToKeysym");
    XKeysymToKeycodeFunc pKeysymToKeycode=(XKeysymToKeycodeFunc)dlsym(x11Lib, "XKeysymToKeycode");
    XGrabKeyFunc pGrabKey=(XGrabKeyFunc)dlsym(x11Lib, "XGrabKey");
    XSelectInputFunc pSelectInput=(XSelectInputFunc)dlsym(x11Lib, "XSelectInput");
    XConnectionNumberFunc pConnectionNumber=(XConnectionNumberFunc)dlsym(x11Lib, "XConnectionNumber");

    if(!pOpenDisplay || !pRootWindow || !pStringToKeysym || !pKeysymToKeycode
       || !pGrabKey || !pSelectInput || !pConnectionNumber)
    {
        dlclose(x11Lib);
        x11Lib=nullptr;
        return false;
    }

    const int keySequence=QKeySequence(sequence)[0];
    if(keySequence==0)
    {
        dlclose(x11Lib);
        x11Lib=nullptr;
        return false;
    }

    const int qtKey=keySequence & ~static_cast<int>(Qt::KeyboardModifierMask);
    const unsigned int modifiers=qtModsToX11(keySequence & static_cast<int>(Qt::KeyboardModifierMask));

    unsigned long keysym=0;
    if(qtKey>=0x20 && qtKey<=0x7e)
    {
        char name[2]={static_cast<char>(qtKey), '\0'};
        keysym=pStringToKeysym(name);
    }
    else
        keysym=qtKeyToKeysym(qtKey);
    if(keysym==0)
    {
        dlclose(x11Lib);
        x11Lib=nullptr;
        return false;
    }

    xDisplay=pOpenDisplay(nullptr);
    if(!xDisplay)
    {
        dlclose(x11Lib);
        x11Lib=nullptr;
        return false;
    }

    const int keycode=pKeysymToKeycode(xDisplay, keysym);
    if(keycode==0)
    {
        XCloseDisplayFunc pCloseDisplay=(XCloseDisplayFunc)dlsym(x11Lib, "XCloseDisplay");
        if(pCloseDisplay)
            pCloseDisplay(xDisplay);
        xDisplay=nullptr;
        dlclose(x11Lib);
        x11Lib=nullptr;
        return false;
    }

    xRootWindow=pRootWindow(xDisplay);
    pSelectInput(xDisplay, xRootWindow, 1L << 0); // KeyPressMask

    // Маски NumLock/CapsLock: захватываем все 4 комбинации
    const unsigned int extraMasks[4]={0, 2, 16, 18};
    for(int i=0; i<4; ++i)
        pGrabKey(xDisplay, keycode, modifiers | extraMasks[i], xRootWindow, 1, 1, 1);

    XFlushFunc pFlush=(XFlushFunc)dlsym(x11Lib, "XFlush");
    if(pFlush)
        pFlush(xDisplay);

    xKeycode=keycode;
    xModifiers=modifiers;

    xNotifier=new QSocketNotifier(pConnectionNumber(xDisplay), QSocketNotifier::Read, this);
    connect(xNotifier, &QSocketNotifier::activated, this, &Clipper::onX11Activity);

    return true;
#endif
}


void Clipper::ungrabHotkey(void)
{
#if defined(Q_OS_WIN)
    if(winFilter)
    {
        if(QCoreApplication::instance())
            QCoreApplication::instance()->removeNativeEventFilter(winFilter);
        delete winFilter;
        winFilter=nullptr;
    }
    UnregisterHotKey(nullptr, clipperWinHotkeyId);
#endif
#if defined(Q_OS_LINUX)
    if(xNotifier)
    {
        delete xNotifier;
        xNotifier=nullptr;
    }

    if(x11Lib && xDisplay)
    {
        XUngrabKeyFunc pUngrabKey=(XUngrabKeyFunc)dlsym(x11Lib, "XUngrabKey");
        XCloseDisplayFunc pCloseDisplay=(XCloseDisplayFunc)dlsym(x11Lib, "XCloseDisplay");
        if(pUngrabKey)
        {
            const unsigned int extraMasks[4]={0, 2, 16, 18};
            for(int i=0; i<4; ++i)
                pUngrabKey(xDisplay, xKeycode, xModifiers | extraMasks[i], xRootWindow);
        }
        if(pCloseDisplay)
            pCloseDisplay(xDisplay);
    }

    if(x11Lib)
    {
        dlclose(x11Lib);
        x11Lib=nullptr;
    }

    xDisplay=nullptr;
    xRootWindow=0;
    xKeycode=0;
    xModifiers=0;
#endif
}


void Clipper::onX11Activity(void)
{
#if defined(Q_OS_LINUX)
    if(!x11Lib || !xDisplay)
        return;

    XPendingFunc pPending=(XPendingFunc)dlsym(x11Lib, "XPending");
    XNextEventFunc pNextEvent=(XNextEventFunc)dlsym(x11Lib, "XNextEvent");
    if(!pPending || !pNextEvent)
        return;

    while(pPending(xDisplay)>0)
    {
        XKeyEventCompat event;
        memset(&event, 0, sizeof(event));
        pNextEvent(xDisplay, &event);

        if(event.type!=X11KeyPress)
            continue;
        if(static_cast<int>(event.keycode)!=xKeycode)
            continue;

        const unsigned int relevantMods=event.state & (1 | 4 | 8 | 64);
        if(relevantMods!=xModifiers)
            continue;

        qDebug() << "Clipper: global hotkey pressed";

        // Выделение само в буфер не попадает: сначала эмулировать Ctrl+C,
        // иначе заберется предыдущее содержимое (протухший буфер).
        // Дальше как в Windows: изменился — забрать, нет — подождать
        m_x11BeforeSig=x11ClipSignature();
        m_hotkeyCaption=x11ActiveWindowCaption(x11Lib, xDisplay);
        sendCopyKeysX11(x11Lib, xDisplay);
        m_x11Tries=0;
        QTimer::singleShot(300, this, &Clipper::clipX11AfterCopy);
    }
#endif
}


// Слепок буфера: форматы+текст+html+флаг картинки. Сырые данные всех
// форматов не годятся: метки времени меняются при каждом чтении
// и "изменение" было бы всегда
QString Clipper::x11ClipSignature(void)
{
#if !defined(Q_OS_LINUX)
    return QString();
#else
    const QMimeData *mime=(QApplication::clipboard()!=nullptr)
        ? QApplication::clipboard()->mimeData() : nullptr;
    if(!mime)
        return QStringLiteral("null");

    return mime->formats().join(QChar(',')) + QChar('|')
         + mime->text() + QChar('|') + mime->html() + QChar('|')
         + (mime->hasImage() ? QChar('I') : QChar('-'));
#endif
}


// Эмуляция Ctrl+C через XTest (Display* и libXtst — opaque void*,
// Xlib-заголовки не тянутся). Нет библиотеки или символов —
// тихо ничего не делаем, дальше отработает забор как есть
void Clipper::sendCopyKeysX11(void *x11Lib, void *xDisplay)
{
#if !defined(Q_OS_LINUX)
    Q_UNUSED(x11Lib)
    Q_UNUSED(xDisplay)
#else
    if(!x11Lib || !xDisplay)
        return;

    static void *xtstLib=nullptr;
    static bool xtstTried=false;
    if(!xtstTried)
    {
        xtstTried=true;
        xtstLib=dlopen("libXtst.so.6", RTLD_NOW);
        if(!xtstLib)
            xtstLib=dlopen("libXtst.so", RTLD_NOW);
    }
    if(!xtstLib)
    {
        qDebug() << "Clipper: libXtst missing, no X11 copy emulation";
        return;
    }

    typedef int (*FakeKeyFunc)(void *, unsigned int, int, unsigned long);
    FakeKeyFunc pFake=(FakeKeyFunc)dlsym(xtstLib, "XTestFakeKeyEvent");
    XStringToKeysymFunc pStringToKeysym=(XStringToKeysymFunc)dlsym(x11Lib, "XStringToKeysym");
    XKeysymToKeycodeFunc pKeysymToKeycode=(XKeysymToKeycodeFunc)dlsym(x11Lib, "XKeysymToKeycode");
    XFlushFunc pFlush=(XFlushFunc)dlsym(x11Lib, "XFlush");
    if(!pFake || !pStringToKeysym || !pKeysymToKeycode || !pFlush)
    {
        qDebug() << "Clipper: XTest symbols missing";
        return;
    }

    const unsigned int ctrlCode=pKeysymToKeycode(xDisplay, pStringToKeysym("Control_L"));
    const unsigned int cCode=pKeysymToKeycode(xDisplay, pStringToKeysym("c"));
    if(ctrlCode==0 || cCode==0)
    {
        qDebug() << "Clipper: no X11 keycodes for Ctrl+C";
        return;
    }

    pFake(xDisplay, ctrlCode, 1, 0);
    pFake(xDisplay, cCode, 1, 0);
    pFake(xDisplay, cCode, 0, 0);
    pFake(xDisplay, ctrlCode, 0, 0);
    pFlush(xDisplay);
    qDebug() << "Clipper: X11 Ctrl+C sent";
#endif
}


// Заголовок активного окна: _NET_WM_NAME (UTF-8), запасной XFetchName.
// Вверх по родителям не выше двух колен (фокус бывает на дочернем)
QString Clipper::x11ActiveWindowCaption(void *x11Lib, void *xDisplay)
{
#if !defined(Q_OS_LINUX)
    Q_UNUSED(x11Lib)
    Q_UNUSED(xDisplay)
    return QString();
#else
    if(!x11Lib || !xDisplay)
        return QString();

    typedef int (*GetInputFocusFunc)(void *, unsigned long *, int *);
    typedef unsigned long (*InternAtomFunc)(void *, const char *, int);
    typedef int (*GetPropertyFunc)(void *, unsigned long, unsigned long, long, long, int,
                                   unsigned long, unsigned long *, int *, unsigned long *,
                                   unsigned long *, unsigned char **);
    typedef int (*FetchNameFunc)(void *, unsigned long, char **);
    typedef int (*QueryTreeFunc)(void *, unsigned long, unsigned long *,
                                 unsigned long *, unsigned long **, unsigned int *);
    typedef int (*FreeFunc)(void *);

    GetInputFocusFunc pGetFocus=(GetInputFocusFunc)dlsym(x11Lib, "XGetInputFocus");
    InternAtomFunc pInternAtom=(InternAtomFunc)dlsym(x11Lib, "XInternAtom");
    GetPropertyFunc pGetProperty=(GetPropertyFunc)dlsym(x11Lib, "XGetWindowProperty");
    FetchNameFunc pFetchName=(FetchNameFunc)dlsym(x11Lib, "XFetchName");
    QueryTreeFunc pQueryTree=(QueryTreeFunc)dlsym(x11Lib, "XQueryTree");
    FreeFunc pFree=(FreeFunc)dlsym(x11Lib, "XFree");
    if(!pGetFocus || !pInternAtom || !pGetProperty || !pFetchName || !pFree)
    {
        qDebug() << "Clipper: X11 title symbols missing";
        return QString();
    }

    const unsigned long utf8Atom=pInternAtom(xDisplay, "UTF8_STRING", 0);
    const unsigned long nameAtom=pInternAtom(xDisplay, "_NET_WM_NAME", 0);

    unsigned long window=0;
    int revert=0;
    pGetFocus(xDisplay, &window, &revert);

    // Фокус бывает на дочернем окне без имени: подняться к родителям
    for(int level=0; level<3 && window!=0 && window!=1; ++level)
    {
        unsigned long actualType=0;
        int actualFormat=0;
        unsigned long itemCount=0;
        unsigned long bytesAfter=0;
        unsigned char *prop=nullptr;

        if(pGetProperty(xDisplay, window, nameAtom, 0, 1024, 0,
                        utf8Atom, &actualType, &actualFormat,
                        &itemCount, &bytesAfter, &prop)==0 &&
           prop!=nullptr && itemCount>0)
        {
            const QString title=QString::fromUtf8(reinterpret_cast<const char *>(prop),
                                                  static_cast<int>(itemCount));
            pFree(prop);
            if(!title.trimmed().isEmpty())
            {
                qDebug() << "Clipper: X11 window title:" << title;
                return title;
            }
        }
        else if(prop!=nullptr)
            pFree(prop);

        if(pQueryTree==nullptr)
            break;

        unsigned long root=0;
        unsigned long parent=0;
        unsigned long *children=nullptr;
        unsigned int childCount=0;
        if(pQueryTree(xDisplay, window, &root, &parent, &children, &childCount)==0)
            break;

        if(children!=nullptr)
            pFree(children);

        if(parent==0 || parent==root)
            break;

        window=parent;
    }

    // Запасной вариант: старый XA_STRING через XFetchName
    if(window!=0 && window!=1)
    {
        char *name=nullptr;
        if(pFetchName(xDisplay, window, &name)!=0 && name!=nullptr)
        {
            const QString title=QString::fromLocal8Bit(name);
            pFree(name);
            if(!title.trimmed().isEmpty())
            {
                qDebug() << "Clipper: X11 window title (legacy):" << title;
                return title;
            }
        }
    }

    return QString();
#endif
}


// Сырая шапка окна под фокусом прямо сейчас. Для потоков без снимка
// из момента хоткея: --control, кнопка проверки, сломанный граббер.
// На X11 со своим соединением, чтобы не зависеть от граббера
QString Clipper::foregroundWindowCaption(void)
{
#if defined(Q_OS_WIN)
    return windowCaption(GetForegroundWindow());
#elif !defined(Q_OS_LINUX)
    return QString();
#else
    static void *fgLib=nullptr;
    static void *fgDisplay=nullptr;
    static bool fgTried=false;

    if(!fgTried)
    {
        fgTried=true;
        fgLib=dlopen("libX11.so.6", RTLD_NOW);
        if(!fgLib)
            fgLib=dlopen("libX11.so", RTLD_NOW);

        if(fgLib!=nullptr)
        {
            typedef void *(*OpenDisplayFunc)(const char *);
            OpenDisplayFunc pOpenDisplay=(OpenDisplayFunc)dlsym(fgLib, "XOpenDisplay");
            if(pOpenDisplay!=nullptr)
                fgDisplay=pOpenDisplay(nullptr);
        }
    }

    return x11ActiveWindowCaption(fgLib, fgDisplay);
#endif
}


void Clipper::clipX11AfterCopy(void)
{
#if defined(Q_OS_LINUX)
    const QString nowSig=x11ClipSignature();
    qDebug() << "Clipper: after hotkey copy changed" << (nowSig!=m_x11BeforeSig);

    if(nowSig!=m_x11BeforeSig)
    {
        clipNow();
        return;
    }

    m_x11Tries=0;
    QTimer::singleShot(250, this, &Clipper::clipX11WaitRetry);
#endif
}


void Clipper::clipX11WaitRetry(void)
{
#if defined(Q_OS_LINUX)
    // Медленная копия: перепослать Ctrl+C и подождать ещё.
    // Всего ~2 секунды. Буфер так и не изменился — выделения не было
    // (медленная копия успела бы): молча пропускаем, чтобы не вставить
    // протухшее содержимое чужого буфера. Шапка окна тоже сбрасывается,
    // чтобы прямой забор не подхватил чужой заголовок
    const QString nowSig=x11ClipSignature();
    qDebug() << "Clipper: wait copy, try" << m_x11Tries
             << "changed" << (nowSig!=m_x11BeforeSig);

    if(nowSig!=m_x11BeforeSig)
    {
        clipNow();
        return;
    }

    if(m_x11Tries<8)
    {
        m_x11Tries++;
        sendCopyKeysX11(x11Lib, xDisplay);
        QTimer::singleShot(250, this, &Clipper::clipX11WaitRetry);
        return;
    }

    m_hotkeyCaption.clear();
    notify(tr("Web Clipper"), tr("No selection to clip."));
#endif
}


#if defined(Q_OS_WIN)
void Clipper::onWinHotkey(void)
{
    qDebug() << "Clipper: global hotkey pressed";

    // Счетчик буфера и окно под фокусом запоминаются до эмуляции:
    // если выделения не было, буфер не изменится и заметка соберется
    // из заголовка окна (название страницы), а не из чужого содержимого
    m_hotkeyClipSeq=GetClipboardSequenceNumber();
    m_hotkeyWindow=GetForegroundWindow();
    m_hotkeyCaption=windowCaption(m_hotkeyWindow);
    m_hotkeyTitle=cleanWindowCaption(m_hotkeyCaption);

    DWORD fgPid=0;
    GetWindowThreadProcessId(static_cast<HWND>(m_hotkeyWindow), &fgPid);
    const WinProcInfo fg=processInfo(fgPid);
    const WinProcInfo self=processInfo(GetCurrentProcessId());
    qDebug() << "Clipper: hotkey window" << m_hotkeyWindow << fgPid << fg.exe
             << "elevated:" << fg.elevated << "self elevated:" << self.elevated;

    // Какой именно контрол в фокусе: Ctrl+A бьёт в него. Если это не
    // контент (адресная строка, хром, диалог), выделять нечего —
    // и это видно сразу, а не гадается по тишине буфера
    DWORD fgTid=GetWindowThreadProcessId(static_cast<HWND>(m_hotkeyWindow), &fgPid);
    HWND focusedChild=nullptr;
    wchar_t childClass[128]={0};

    if(fgTid!=0)
    {
        GUITHREADINFO threadInfo={0};
        threadInfo.cbSize=sizeof(threadInfo);

        if(GetGUIThreadInfo(fgTid, &threadInfo) && threadInfo.hwndFocus!=nullptr)
        {
            focusedChild=threadInfo.hwndFocus;
            RealGetWindowClassW(focusedChild, childClass, 128);
        }
    }

    qDebug() << "Clipper: hotkey focus child" << focusedChild
             << QString::fromWCharArray(childClass);
    // Один хоткей на всё: сначала скопировать выделение в буфер,
    // затем забрать буфер в unsorted_notes
    sendCopyKeys();
    QTimer::singleShot(300, this, &Clipper::clipNowAfterHotkey);
}


void Clipper::clipNowAfterHotkey(void)
{
    // Ненулевой счетчик и тот же номер: буфер не тронут — выделения
    // не было. Дальше два пути: чужое окно — подождать медленную копию,
    // своё окно — молча пропустить (иначе мусорная заметка из себя)
    const DWORD nowSeq=GetClipboardSequenceNumber();
    qDebug() << "Clipper: after hotkey copy seq" << m_hotkeyClipSeq << "->" << nowSeq;

    if(m_hotkeyClipSeq==0 || nowSeq!=m_hotkeyClipSeq)
    {
        clipNow();
        return;
    }

    if(!m_hotkeyTitle.isEmpty() && IsWindow(static_cast<HWND>(m_hotkeyWindow)))
    {
        DWORD fgPid=0;
        GetWindowThreadProcessId(static_cast<HWND>(m_hotkeyWindow), &fgPid);

        if(fgPid==GetCurrentProcessId())
        {
            qDebug() << "Clipper: hotkey in own window without selection, skipped";

            // Шапка чужого окна нам не нужна: следующий прямой забор
            // не должен подхватить протухший заголовок
            m_hotkeyCaption.clear();
            return;
        }
    }

    m_copyWaitTries=0;
    QTimer::singleShot(250, this, &Clipper::clipWaitCopyRetry);
}




void Clipper::clipWaitCopyRetry(void)
{
    // Медленная копия большого выделения: перепослать Ctrl+C и подождать
    // ещё. Всего до ~2 секунд, затем заголовок окна
    const DWORD nowSeq=GetClipboardSequenceNumber();
    qDebug() << "Clipper: wait copy, seq" << m_hotkeyClipSeq << "->" << nowSeq
             << "try" << m_copyWaitTries;

    if(m_hotkeyClipSeq!=0 &&
       nowSeq==m_hotkeyClipSeq &&
       IsWindow(static_cast<HWND>(m_hotkeyWindow)) &&
       m_copyWaitTries<8)
    {
        m_copyWaitTries++;
        sendCopyKeys();
        QTimer::singleShot(250, this, &Clipper::clipWaitCopyRetry);
        return;
    }

    if(m_hotkeyClipSeq==0 ||
       nowSeq==m_hotkeyClipSeq ||
       !IsWindow(static_cast<HWND>(m_hotkeyWindow)))
    {
        if(const QMimeData *mime=QApplication::clipboard()->mimeData())
            qDebug() << "Clipper: fallback clipboard formats" << mime->formats()
                     << "hasText" << mime->hasText() << "len"
                     << (mime->hasText() ? mime->text().size() : -1)
                     << "hasHtml" << mime->hasHtml()
                     << "htmlLen" << (mime->hasHtml() ? mime->html().size() : -1);

        clipWindowTitle(m_hotkeyTitle);
        return;
    }

    clipNow();
}
#endif
