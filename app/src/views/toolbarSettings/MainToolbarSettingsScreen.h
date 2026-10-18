#ifndef MAINTOOLBARSETTINGSSCREEN_H
#define MAINTOOLBARSETTINGSSCREEN_H

#include <QDialog>
#include <QMap>
#include <QStringList>

class QWidget;
class QDialogButtonBox;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QListWidget;
class QTreeView;
class QStackedWidget;
class QStandardItemModel;

// Диалог настройки порядка и видимости кнопок основных тулбаров:
// дерево веток, список записей, вторая линия списка записей.
// Устройство повторяет диалог панелей редактора: доступные слева,
// задействованные справа, перемещение стрелками, разделители,
// всё применяется живьём без перезапуска.
// Внешние кнопки второй линии (переключатели панелей) и скрытые
// shortcut-only кнопки сюда не выносятся: первые всегда в конце,
// вторые регистрируются всегда
class MainToolbarSettingsScreen : public QDialog
{
    Q_OBJECT
public:
    MainToolbarSettingsScreen(QWidget *parent = nullptr);
    ~MainToolbarSettingsScreen();

    enum Panel { TreePanel = 0, NotesPanel = 1, NotesExtraPanel = 2 };

protected slots:
    void onPanelChange(int row);
    void onMoveAvailableToUsed(void);
    void onMoveUsedToAvailable(void);
    void onMoveSelectedUp(void);
    void onMoveSelectedDown(void);
    void onInfoClick(void);
    void applyChanges(void);

protected:
    void setupUI(void);
    void setupSignals(void);
    void assembly(void);

    // Загрузка моделей выбранной панели из отложенных списков
    void loadPanel(int panel);

    // Текущий задействованный список выбранной панели из модели
    QStringList usedCommands(void) const;

    // Задействованный список указанной панели из её модели
    QStringList usedCommandsFor(int panel) const;

    // Вид задействованных команд выбранной панели
    class QTreeView *currentUsedView(void) const;

    // Видимость команды в текущем режиме интерфейса (десктоп/мобила)
    bool isCommandVisibleForMode(int panel, const QString &command) const;

    // Спрятать в обоих списках строки, неприменимые к текущему режиму
    void applyModeVisibility(int panel);

    // Видимые имя и иконка команды для списков
    QString commandText(int panel, const QString &command) const;
    QIcon commandIcon(int panel, const QString &command) const;

    // Действие-источник иконки и текста
    class QAction *commandAction(int panel, const QString &command) const;

    // Модель задействованных команд выбранной панели
    QStandardItemModel *usedModel(void) const;

    QListWidget *selectPanelListWidget;

    QLabel *availableLabel;
    QLabel *usedLabel;
    QLabel *exchangeLabel;

    QPushButton *toUsedPushButton;
    QPushButton *toAvailablePushButton;
    QPushButton *usedUpPushButton;
    QPushButton *usedDownPushButton;
    QPushButton *infoPushButton;

    QDialogButtonBox *dialogButtonBox;

    QStandardItemModel *availableModel;
    QStandardItemModel *treeUsedModel;
    QStandardItemModel *notesUsedModel;
    QStandardItemModel *notesExtraUsedModel;
    QStackedWidget *usedStackedWidget;

    QTreeView *availableView;
    QTreeView *treeUsedView;
    QTreeView *notesUsedView;
    QTreeView *notesExtraUsedView;

    QVBoxLayout *screenLayout;
    QVBoxLayout *availableLayout;
    QVBoxLayout *buttonsLayout;
    QVBoxLayout *usedLayout;
    QHBoxLayout *exchangeLayout;

    // Отложенные задействованные списки (пишутся в конфиг по OK)
    QMap<int, QStringList> pendingUsed;

    // Списки на момент открытия (для проверки изменений)
    QMap<int, QStringList> loadedUsed;
};

#endif // MAINTOOLBARSETTINGSSCREEN_H
