#ifndef CHANGEVIEWDIALOG_H
#define CHANGEVIEWDIALOG_H

#include <QColor>
#include <QDialog>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>

#include "libraries/BranchDiffEngine.h"

class QPushButton;
class QTableWidget;
class QTableWidgetItem;
class QTextEdit;
class QComboBox;

// Диалог «Что изменилось»: список изменений публикации с чекбоксами.
//
// Показывает разность между базовой точкой подписки и актуальной версией
// публикации. Пользователь выбирает режим импорта (фильтр над списком
// изменений) и/или помечает конкретные изменения вручную. По умолчанию
// выбраны неопасные изменения (новые записи и правки); удаления не выбраны,
// помечаются вручную и требуют подтверждения при применении.
//
// Диалог не трогает базу: он лишь возвращает выбранные индексы и выбранное
// действие (применить выбранное / отметить просмотренным). Применение
// выполняет TreeScreen через SubscriptionImportEngine.

class ChangeViewDialog : public QDialog
{
    Q_OBJECT

public:
    // Действие, выбранное пользователем
    enum class Action
    {
        None,           // Диалог закрыт без действия
        ApplySelected,  // Применить выбранные изменения
        MarkViewed      // Отметить всё просмотренным без импорта
    };

    // Режим импорта — фильтр над списком изменений
    enum class ImportMode
    {
        Custom,             // Выборочно: чекбоксами вручную
        NewOnly,            // Только новые записи
        AllExceptDelete,    // Всё применимое, кроме удалений
        AllIncludeDelete    // Всё применимое, включая удаления
    };

    ChangeViewDialog(const QString &publicationTitle,
                     const QString &ownerName,
                     const QList<DiffChange> &changes,
                     QWidget *parent=nullptr,
                     const QString &publicationDir=QString(),
                     const QMap<QString, QPair<QString, QString>> &recordTexts
                         =QMap<QString, QPair<QString, QString>>(),
                     const QMap<QString, QPair<QString, QString>> &recordBaseDirs
                         =QMap<QString, QPair<QString, QString>>());

    // Выбранное действие (валидно после exec())
    Action action(void) const;

    // Индексы выбранных изменений в исходном списке changes
    QList<int> selectedChangeIndices(void) const;

    // Режим импорта, выбранный пользователем
    ImportMode importMode(void) const;

    // Строковое имя режима (для журнала)
    QString importModeName(void) const;

private slots:
    void onSelectionChanged(void);
    void onItemChanged(QTableWidgetItem *item);
    void onModeChanged(int index);
    void onApplyClicked(void);
    void onMarkViewedClicked(void);
    void onDiffWindowClicked(void);
    void onCellDoubleClicked(int row, int column);

private:
    void setupUi(void);
    void fillTable(void);

    // Заполнение вкладки истории из changelog.json публикации
    void fillHistory(void);

    // Применить выбранный режим к чекбоксам применимых изменений
    void applyModeChecks(ImportMode mode);

    // Можно ли применить изменение к личной БД
    static bool isApplicable(const DiffChange &change);

    // Опасное ли изменение (удаление)
    static bool isDangerous(const DiffChange &change);

    // Метка типа изменения
    static QString typeLabel(const DiffChange &change);

    // Цвет строки по типу изменения: новое — зелёный, изменённое — синий,
    // удаление — красный
    static QColor rowColor(const DiffChange &change);

    // Краткое описание изменения
    static QString changeSummary(const DiffChange &change);

    // Подробное описание выбранного изменения (HTML).
    // Для RecordUpdate с изменением текста включает построчный diff
    // из кеша (локальная копия -> версия владельца)
    QString changeDetails(int changeIndex) const;

    QString publicationTitle;
    QString ownerName;
    QList<DiffChange> changes;
    QString publicationDir;

    QMap<QString, QPair<QString, QString>> recordTexts;

    // Базовые каталоги для рендера «Было / Стало»: sharedRecordId ->
    // (каталог локальной копии, каталог записи публикации)
    QMap<QString, QPair<QString, QString>> recordBaseDirs;

    // Кеш построчных diff по индексу изменения (считается в fillTable,
    // показывается в деталях, колонке-индикаторе и отдельном окне)
    QMap<int, QString> textDiffCache;

    // Открыть построчный diff строки в отдельном окне (если он есть)
    void openDiffWindow(int row);

    Action selectedAction=Action::None;
    QList<int> selectedIndices;

    // Защита от рекурсии при программной смене режима/чекбоксов
    bool syncingChecks=false;

    QTableWidget *changesTable;
    QTextEdit *detailsView;
    QTextEdit *historyView;
    QComboBox *modeSelector;
    QPushButton *diffWindowButton;
    QPushButton *applyButton;
    QPushButton *markViewedButton;
};

#endif // CHANGEVIEWDIALOG_H