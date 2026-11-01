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
#include "libraries/wyedit/EditorConfig.h"
#include "models/appConfig/AppConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "views/tree/TreeScreen.h"
#include "models/appConfig/AppConfigClipper.h"

// Лимиты картинок из настроек. Клиппер фоновый (окно может быть скрыто),
// поэтому вместо вопроса как при обычной вставке действуют лимиты,
// а о пропусках сообщается сразу трей-уведомлением



static const int defaultClipMaxImages=20;


static const qint64 defaultClipMaxImageSizeBytes=5*1024*1024;


int clipMaxImages=defaultClipMaxImages;


qint64 clipMaxImageBytes=defaultClipMaxImageSizeBytes;

// Расширение новой картинки в клипере: настройка формата редактора,
// картинки с прозрачностью всегда PNG (у JPG нет альфа-канала).
// Конфига может не быть в тестах — тогда png


// Расширение новой картинки в клипере: настройка формата редактора,
// картинки с прозрачностью всегда PNG (у JPG нет альфа-канала).
// Конфига может не быть в тестах — тогда png
QString clipImageExtension(const QImage &image)
{
    EditorConfig *editorConfig=find_object<EditorConfig>("editorconfig");

    if(editorConfig==nullptr)
        return QStringLiteral("png");

    return editorConfig->imageExtensionFor(image);
}

// Ужатие картинки клипера до лимита длинной стороны из настройки.
// Без конфига возвращается исходник


// Ужатие картинки клипера до лимита длинной стороны из настройки.
// Без конфига возвращается исходник
QImage clipLimitImageSize(const QImage &image)
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
    int maxImages=AppContext::instance().appConfig()->clipperSection()->get_clipperMaxImages();
    if(maxImages>0)
        clipMaxImages=maxImages;
    else
        clipMaxImages=defaultClipMaxImages;

    int maxSizeMb=AppContext::instance().appConfig()->clipperSection()->get_clipperMaxImageSizeMb();
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


// Суффикс браузера в шапке окна (" - Google Chrome" и т.п.).
// Дефис, en dash, em dash и пара браузеров кириллицей — файл UTF-8,
// поэтому литералами, а не hex-последовательностями
const QRegularExpression &browserCaptionSuffix(void)
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

    TreeScreen *treeScreen=AppContext::instance().globalParameters()->getTreeScreen();
    if(!treeScreen)
        return nullptr;
    return treeScreen->knowTreeModel;
}


void Clipper::saveTree(void) const
{
    if(testModel)
        return;

    TreeScreen *treeScreen=AppContext::instance().globalParameters()->getTreeScreen();
    if(treeScreen)
    {
        treeScreen->saveKnowTree();

        // Пачка новых записей: кеш статистики в пересчет
        treeScreen->knowTreeModel->invalidateStats();
    }
}
