#include "AppContext.h"
#include "libraries/Favorites.h"
#include "libraries/BacklinkIndex.h"
#include "libraries/FtsTextIndex.h"
#include "libraries/wyedit/EditorShowTextDispatcher.h"


AppContext &AppContext::instance(void)
{
    // Куча вместо статического объекта: main() отдает ядро под
    // Qt-владение (setParent(&app)), а родителя удаляет детей
    // через delete. Delete статического хранилища - это free()
    // не из кучи и аборт при каждом выходе (см. fix/493).
    // Указатель умирает тривиально, двойного разрушения нет
    static AppContext *inst=new AppContext;

    return *inst;
}


AppContext::AppContext(QObject *parent) : QObject(parent)
{
    // P3: тройка бывших синглтонов живет весь рантайм под этим объектом.
    // Создаются при первом instance() в main() (после QApplication).
    // Диспетчер - без родителя: он сразу уезжает в свой QThread,
    // а moveToThread запрещен для объектов с родителем
    // (см. EditorShowTextDispatcher::EditorShowTextDispatcher).
    // Удаляется вручную в деструкторе, как раньше удалялся
    // бессмертный Meyers-static при выходе
    m_favorites=new Favorites(this);
    m_backlinkIndex=new BacklinkIndex(this);
    m_ftsTextIndex=new FtsTextIndex(this);
    m_dispatcher=new EditorShowTextDispatcher(nullptr);
}


AppContext::~AppContext(void)
{
    delete m_dispatcher;
    m_dispatcher=nullptr;
}


void AppContext::init(FixedParameters *fixedParameters,
                      GlobalParameters *globalParameters,
                      AppConfig *appConfig,
                      AppFiles *appFiles,
                      DataBaseConfig *dataBaseConfig,
                      TrashMonitoring *trashMonitoring,
                      WalkHistory *walkHistory,
                      VisitHistory *visitHistory,
                      ActionLogger *actionLogger,
                      ShortcutManager *shortcutManager,
                      PeriodicCheckBase *periodicCheckBase,
                      PeriodicSyncro *periodicSyncro,
                      InternalClipboard *internalClipboard,
                      MainWindow *mainWindow,
                      CryptKeyHolder *cryptKeyHolder)
{
    m_fixedParameters=fixedParameters;
    m_globalParameters=globalParameters;
    m_appConfig=appConfig;
    m_appFiles=appFiles;
    m_dataBaseConfig=dataBaseConfig;
    m_trashMonitoring=trashMonitoring;
    m_walkHistory=walkHistory;
    m_visitHistory=visitHistory;
    m_actionLogger=actionLogger;
    m_shortcutManager=shortcutManager;
    m_periodicCheckBase=periodicCheckBase;
    m_periodicSyncro=periodicSyncro;
    m_internalClipboard=internalClipboard;
    m_mainWindow=mainWindow;
    m_cryptKeyHolder=cryptKeyHolder;

    m_initialized=true;
}


bool AppContext::isInitialized(void) const
{
    return m_initialized;
}


void AppContext::setMainWindow(MainWindow *mainWindow)
{
    m_mainWindow=mainWindow;
}


void AppContext::setInternalClipboard(InternalClipboard *internalClipboard)
{
    m_internalClipboard=internalClipboard;
}


FixedParameters *AppContext::fixedParameters(void) const
{
    return m_fixedParameters;
}


GlobalParameters *AppContext::globalParameters(void) const
{
    return m_globalParameters;
}


AppConfig *AppContext::appConfig(void) const
{
    return m_appConfig;
}


AppFiles *AppContext::appFiles(void) const
{
    return m_appFiles;
}


DataBaseConfig *AppContext::dataBaseConfig(void) const
{
    return m_dataBaseConfig;
}


TrashMonitoring *AppContext::trashMonitoring(void) const
{
    return m_trashMonitoring;
}


WalkHistory *AppContext::walkHistory(void) const
{
    return m_walkHistory;
}


VisitHistory *AppContext::visitHistory(void) const
{
    return m_visitHistory;
}


ActionLogger *AppContext::actionLogger(void) const
{
    return m_actionLogger;
}


ShortcutManager *AppContext::shortcutManager(void) const
{
    return m_shortcutManager;
}


PeriodicCheckBase *AppContext::periodicCheckBase(void) const
{
    return m_periodicCheckBase;
}


PeriodicSyncro *AppContext::periodicSyncro(void) const
{
    return m_periodicSyncro;
}


InternalClipboard *AppContext::internalClipboard(void) const
{
    return m_internalClipboard;
}


MainWindow *AppContext::mainWindow(void) const
{
    return m_mainWindow;
}


CryptKeyHolder *AppContext::cryptKeyHolder(void) const
{
    return m_cryptKeyHolder;
}


Favorites *AppContext::favorites(void) const
{
    return m_favorites;
}


BacklinkIndex *AppContext::backlinkIndex(void) const
{
    return m_backlinkIndex;
}


FtsTextIndex *AppContext::ftsTextIndex(void) const
{
    return m_ftsTextIndex;
}


EditorShowTextDispatcher *AppContext::dispatcher(void) const
{
    return m_dispatcher;
}
