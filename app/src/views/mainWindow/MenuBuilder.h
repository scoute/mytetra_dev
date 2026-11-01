#ifndef MENUBUILDER_H
#define MENUBUILDER_H

#include <QObject>
#include <QMap>

// Построитель меню главного окна. Второе классовое выделение из
// MainWindow-god (P3): вместо ~30 QAction и 5 QMenu мемберов плюс
// 500 строк конструкторов - отдельный QObject с DI.
// Зависимости:
// - QMenuBar передается в конструктор (билдер сам вешает меню)
// - тогглы доков (tags/history/backlinks) и десктоп/мобайл режим
//   передаются параметрами buildToolsMenu (это продукты MainWindow,
//   билдер их не ищет)
// - конфиг/шорткаты/темы читаются через те же extern, что раньше
//   читало MainWindow (поведение 1:1, следующий шаг - инъекция)
// Связь обратно - сигналы (билдер не знает про MainWindow):
// - триггеры пунктов, у которых были прямые connect в MainWindow,
//   уходят сигналами; файловые/тулзовые/хелповые экшены отдаются
//   геттерами и коннектятся в MainWindow::setupSignals как раньше
// Поведенческие методы (синки по экранам, тогглы, applyReadOnly,
// runDirectPreferences) остаются в MainWindow и ходят через геттеры

class QMenu;
class QAction;
class QMenuBar;

class MenuBuilder : public QObject
{
    Q_OBJECT

public:

    enum class MenuId { File, Tools, Themes, View, Help };

    enum class ActionId {
        FileDatabases, FileExportTree, FileImportTree, FileImportTelegram,
        FilePrint, FilePrintPreview, FileExportPdf, FileQuit,
        ToolsFindInBase, ToolsImagesGallery, ToolsFilesGallery, ToolsActionLog,
        ToolsClipFromClipboard, ToolsPreferences,
        DirectMain, DirectAppearance, DirectCrypt, DirectSyncro,
        DirectRecordTable, DirectAttach, DirectKeyboard, DirectHistory, DirectMisc,
        HelpAboutMyTetra, HelpAboutQt, HelpTechnicalInfo,
        ThemeConfigure,
        ViewHeader, ViewFavorites, ViewTreeFilter, ViewTools, ViewHtmlSource, ViewReadOnly,
        ViewSecretColor, ViewPaperColor
    };

    explicit MenuBuilder(QMenuBar *menuBar, QObject *parent=nullptr);
    ~MenuBuilder(void) override;

    // Построить все пять меню разом. Параметры - продукты MainWindow:
    // toggle-экшены доков (иначе билдеру пришлось бы знать про доки),
    // isDesktop решает Preferences против подменю (как было)
    void buildAll(QAction *tagsToggle, QAction *historyToggle,
                  QAction *backlinksToggle, bool isDesktop);

    QMenu *menu(MenuId id) const;
    QAction *action(ActionId id) const;

    // Переперевести меню и пункты (вызывается из retranslateUi)
    void retranslate(void);

signals:

    // Выбрана тема из группы (QAction::data - имя темы).
    // MainWindow связывает с onThemeMenuTriggered
    void themeChosen(QAction *action);

    // Пункт "Configure theme...". MainWindow -> onThemeConfigure
    void themeConfigureRequested(void);

    // Тогглы меню Вид. MainWindow -> onView*Toggled
    void viewHeaderToggled(bool checked);
    void viewFavoritesToggled(bool checked);
    void viewTreeFilterToggled(bool checked);
    void viewToolsToggled(bool checked);
    void viewHtmlSourceToggled(bool checked);
    void viewReadOnlyToggled(bool checked);

    // "Change secret color..."/"Change note background color..."
    void secretColorRequested(void);
    void paperColorRequested(void);

    // Меню Вид открывается: MainWindow перечитывает пометки
    // (syncViewMenu живет там - ему нужен editorScreen)
    void viewMenuShown(void);

private:

    void buildFileMenu(void);
    void buildToolsMenu(QAction *tagsToggle, QAction *historyToggle,
                        QAction *backlinksToggle, bool isDesktop);
    void buildPreferencesSubMenu(QMenu *menu);
    void buildHelpMenu(void);
    void buildThemesMenu(void);
    void buildViewMenu(void);

    // Пометка текущей темы (только конфиг - живет здесь).
    // Вызывается при построении и при каждом открытии меню
    void syncThemeMenu(void);

    QMenuBar *m_menuBar=nullptr;

    QMap<MenuId, QMenu *> m_menus;
    QMap<ActionId, QAction *> m_actions;

    // Подменю Preferences (только мобайл): заголовок переводится отдельно
    QMenu *m_preferencesSubMenu=nullptr;
};

#endif // MENUBUILDER_H
