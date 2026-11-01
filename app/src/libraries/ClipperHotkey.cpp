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
#include "libraries/AppContext.h"
#include "models/appConfig/AppConfigStorage.h"
#include "models/appConfig/AppConfigServices.h"
#include "models/appConfig/AppConfigRecordTable.h"
#include "models/appConfig/AppConfigClipper.h"

// Лимиты картинок из настроек. Клиппер фоновый (окно может быть скрыто),
// поэтому вместо вопроса как при обычной вставке действуют лимиты,
// а о пропусках сообщается сразу трей-уведомлением

#include "ClipperPrivate.h"

#if defined(Q_OS_WIN)
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
#endif // defined(Q_OS_WIN): WinProcInfo, processInfo

// Заголовок окна по хендлу для хоткея без выделения

#if defined(Q_OS_LINUX)
#include <dlfcn.h>
#endif

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
    // ещё. Всего до ~2 секунд. Буфер так и не изменился — выделения
    // не было: молча пропускаем как в Linux, а не лепим заметку
    // из одного заголовка. Шапка окна сбрасывается, чтобы прямой
    // забор не подхватил чужой заголовок
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

        m_hotkeyCaption.clear();
        notify(tr("Web Clipper"), tr("No selection to clip."));
        return;
    }

    clipNow();
}
#endif

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
