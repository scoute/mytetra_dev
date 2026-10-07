#ifndef _HISTORYPANEL_H_
#define _HISTORYPANEL_H_

#include <QWidget>
#include <QMap>

// Панель истории посещений заметок в доке справа, как панель тегов.
// Данные - журнал VisitHistory (факты посещений со временем), панель
// показывает агрегат: визиты и последнее время для каждой заметки
// плюс живые счетчики картинок, файлов и размер каталога записи.
// Все колонки сортируются кликом по заголовку, числа и даты как числа.
// Одинарный клик выделяет, прыжок к заметке двойным кликом чтобы
// не сбивать текущую заметку случайным кликом.
// По умолчанию скрыта, переключается из меню Tools и горячей клавишей

class QLineEdit;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;
class QShowEvent;
class QTimer;
class TreeItem;


class HistoryPanel : public QWidget
{
    Q_OBJECT

public:

    HistoryPanel(QWidget *parent=nullptr);
    virtual ~HistoryPanel(void);

public slots:

    // Пересборка таблицы из журнала. Публичный для обновления извне
    void refreshHistory(void);

protected:

    // Таблица пересобирается при каждом показе: журнал мог пополниться
    void showEvent(QShowEvent *event) override;

private slots:

    void onNoteDoubleClicked(int row, int column);
    void onHistoryContextMenu(const QPoint &pos);
    void onClearHistory(void);
    void onForgetNote(void);
    void onVisitLogged(const QString &id);
    void onTreeMetadataSaved(void);

    // Пользователь подвигал границу колонки: ширина запоминается
    // чтобы пересборка ее не сбрасывала
    void onSectionResized(int logicalIndex, int oldSize, int newSize);

    // Прыжок к заметке через готовый механизм позиционирования
    void goToNote(const QString &id);

private:

    // Относительное время для колонки: "только что", "5 мин назад"...
    static QString formatVisitTime(qint64 msecs);

    // Суммарный размер файлов каталога записи в байтах
    static qint64 recordDirSize(const QString &recordDir);

    void setupUi(void);
    void assembly(void);
    void setupSignals(void);

    // Строка фильтра, таблица истории и кнопка очистки
    QLineEdit *filterEdit;
    QTableWidget *historyTable;
    QPushButton *clearButton;

    // Подписка на сохранение метаданных дерева делается один раз
    // и лениво: в конструкторе treeScreen может еще не существовать
    bool treeMetadataConnected;

    // Дебаунс живого обновления: визиты сыплются пачками при навигации
    QTimer *refreshDebounce;

    // Строка контекстного меню для пункта "забыть"
    QString contextNoteId;

    // Ширины колонок, заданные пользователем вручную.
    // Пусто значит автоширина по содержимому с потолком
    QMap<int, int> userColumnWidths;

    // Свои программные ресайзы не запоминать как пользовательские
    bool resizingProgrammatically;
};

#endif /* _HISTORYPANEL_H_ */
