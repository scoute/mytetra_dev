#ifndef APPCONTEXT_H
#define APPCONTEXT_H

#include <QObject>

// Центральная точка доступа к объектам ядра.
// P1: вместо 14 файловых глобалов из main.cpp с extern
// по всему проекту. Объекты по-прежнему создаются в main()
// (порядок инициализации не меняется), AppContext только
// хранит типизированные указатели и отдает их новому коду.
// Старые extern-объявления остаются как тонкие шимы для
// совместимости и уходят по мере перевода вызовов.

class FixedParameters;
class GlobalParameters;
class AppConfig;
class AppFiles;
class DataBaseConfig;
class TrashMonitoring;
class WalkHistory;
class VisitHistory;
class ActionLogger;
class ShortcutManager;
class PeriodicCheckBase;
class PeriodicSyncro;
class InternalClipboard;
class MainWindow;
class Favorites;
class BacklinkIndex;
class FtsTextIndex;
class EditorShowTextDispatcher;
class CryptKeyHolder;

class AppContext : public QObject
{
    Q_OBJECT

public:

    // Синглтон ядра. Создается лениво в куче (бессмертный),
    // main() отдает его под Qt-владение setParent(&app):
    // приложение удалит ядро вместе с детьми при выходе
    static AppContext &instance(void);

    // Привязка к объектам из main(). Вызывается один раз
    // после создания QApplication, до MainWindow
    void init(FixedParameters *fixedParameters,
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
              CryptKeyHolder *cryptKeyHolder);

    bool isInitialized(void) const;

    // Ранняя привязка главного окна из конструктора MainWindow.
    // Нужна чтобы find_object работал уже во время setupUI
    // (полный init() вызывается позже, когда win уже существует)
    void setMainWindow(MainWindow *mainWindow);

    // Привязка буфера после создания (ранний init() видит nullptr,
    // полный указатель появляется только вместе с QApplication)
    void setInternalClipboard(InternalClipboard *internalClipboard);

    // Типизированные доступы. Null пока init() не вызван
    FixedParameters *fixedParameters(void) const;
    GlobalParameters *globalParameters(void) const;
    AppConfig *appConfig(void) const;
    AppFiles *appFiles(void) const;
    DataBaseConfig *dataBaseConfig(void) const;
    TrashMonitoring *trashMonitoring(void) const;
    WalkHistory *walkHistory(void) const;
    VisitHistory *visitHistory(void) const;
    ActionLogger *actionLogger(void) const;
    ShortcutManager *shortcutManager(void) const;
    PeriodicCheckBase *periodicCheckBase(void) const;
    PeriodicSyncro *periodicSyncro(void) const;
    InternalClipboard *internalClipboard(void) const;
    MainWindow *mainWindow(void) const;
    CryptKeyHolder *cryptKeyHolder(void) const;

    Favorites *favorites(void) const;
    BacklinkIndex *backlinkIndex(void) const;
    FtsTextIndex *ftsTextIndex(void) const;
    EditorShowTextDispatcher *dispatcher(void) const;

private:

    explicit AppContext(QObject *parent=nullptr);
    ~AppContext(void) override;

    bool m_initialized=false;

    FixedParameters *m_fixedParameters=nullptr;
    GlobalParameters *m_globalParameters=nullptr;
    AppConfig *m_appConfig=nullptr;
    AppFiles *m_appFiles=nullptr;
    DataBaseConfig *m_dataBaseConfig=nullptr;
    TrashMonitoring *m_trashMonitoring=nullptr;
    WalkHistory *m_walkHistory=nullptr;
    VisitHistory *m_visitHistory=nullptr;
    ActionLogger *m_actionLogger=nullptr;
    ShortcutManager *m_shortcutManager=nullptr;
    PeriodicCheckBase *m_periodicCheckBase=nullptr;
    PeriodicSyncro *m_periodicSyncro=nullptr;
    InternalClipboard *m_internalClipboard=nullptr;
    MainWindow *m_mainWindow=nullptr;
    CryptKeyHolder *m_cryptKeyHolder=nullptr;

    Favorites *m_favorites=nullptr;
    BacklinkIndex *m_backlinkIndex=nullptr;
    FtsTextIndex *m_ftsTextIndex=nullptr;
    EditorShowTextDispatcher *m_dispatcher=nullptr;
};

#endif // APPCONTEXT_H
