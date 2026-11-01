#include "AppContext.h"


AppContext &AppContext::instance(void)
{
    static AppContext inst;

    return inst;
}


AppContext::AppContext(QObject *parent) : QObject(parent)
{

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


void AppContext::registerSingletons(Favorites *favorites,
                                    BacklinkIndex *backlinkIndex,
                                    EditorShowTextDispatcher *dispatcher)
{
    m_favorites=favorites;
    m_backlinkIndex=backlinkIndex;
    m_dispatcher=dispatcher;
}


bool AppContext::isInitialized(void) const
{
    return m_initialized;
}


void AppContext::setMainWindow(MainWindow *mainWindow)
{
    m_mainWindow=mainWindow;
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


EditorShowTextDispatcher *AppContext::dispatcher(void) const
{
    return m_dispatcher;
}
