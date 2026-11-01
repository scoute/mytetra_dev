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


Clipper::Clipper(QObject *parent) : QObject(parent)
{

}


Clipper::~Clipper()
{
    stop();
}
