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

// Win-хоткей живет в ClipperHotkey.cpp (fix/545): дубль здесь удален (ODR-риск)


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
