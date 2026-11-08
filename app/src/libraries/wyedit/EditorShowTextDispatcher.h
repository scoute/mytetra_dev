#ifndef EDITORSHOWTEXTDISPATCHER_H
#define EDITORSHOWTEXTDISPATCHER_H

#include <QObject>

#include "EditorShowText.h"

//! \brief
//! Объект для управления открепляемыми окнами
//! Используется для создания окон, обновления содержимого,
//! запоминание списка окон и их размещения

class QThread;

class EditorShowTextDispatcher : public QObject
{
    Q_OBJECT
public:

    // P3: синглтон убран, владелец - AppContext (DI).
    // Создается один раз в AppContext, живет весь рантайм
    explicit EditorShowTextDispatcher(QObject *parent = nullptr);
    ~EditorShowTextDispatcher() override;

    void createWindow(const QString &noteId, int x=-1, int y=-1, int w=-1, int h=-1, int vScroll=0);

    // Перечитать текст окна из файла записи (сейв главного окна,
    // синхронизация, разблокировка шифра). Окно с несохраненными
    // правками не трогается, инициатор сейва себя не перечитывает.
    // Курсор и прокрутка переживают перечитывание
    void updateWindow(const QString &noteId, const QObject *initiator=nullptr);
    void updateAllWindows();

    // После сейва открепленного окна: исходящие ссылки, статистика
    // и главное окно подтягиваются (как MainWindow::saveTextarea).
    // Главное окно на той же записи перечитывается только если
    // не правилось: иначе его сейв позже перезапишет файл
    void refreshAfterDetachedSave(const QString &noteId);

    // Каталоги записей переименованы на диске (конвертер имен):
    // перевести все окна на новые пути из живой таблицы.
    // Без перезапуска (иначе show-in-folder и сейвы бьют мимо)
    void refreshAllWorkDirectories(void);

    // Переприменить read-only всех окон (переключение глобального
    // режима чтения). Формула та же что у главного окна
    void refreshAllEditability(void);

    // Перекрасить бумагу всех открепляемых окон под текущую тему.
    // Вызывается из тех же мест что applyNoteColors основного редактора:
    // окна живут дольше смены темы и сами ее не отслеживают
    void applyNoteColorsToAll(void);

    void closeWindow(const QString &noteId);
    void closeAllWindows();
    void closeAllWindowsForExit();

    bool isWindowPresent( const QString &noteId );

    void saveOpenWindows();
    void restoreOpenWindows();

    void switchBehavior(const QString &mode);
    void restoreBehavior();

signals:

public slots:

    void closeWindowByIdVector(const QVector<QString> &ids);
    void closeWindowByIdList(const QStringList &ids);
    void closeWindowByIdSet(const QSet< QString > &ids);

    void closeWindowForNonExistentRecords();

    void onCloseWindow(const QString &noteId);

protected:

    // Диспетчер живет в GUI-потоке (fix/541): окна и модель трогаются
    // только из главного потока, отдельного QThread нет (mThread==nullptr)
    QThread *mThread;

    // Список открытых окошек, ключ - это ID открытой записи
    QHash<QString, QPointer<EditorShowText> > mWindowsList;

    // Текущее поведение сворачивания/разворачивания окон
    // Возможные значения "single" и "together"
    QString mBehavior;

};

#endif // EDITORSHOWTEXTDISPATCHER_H
