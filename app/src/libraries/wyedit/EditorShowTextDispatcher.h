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

    void updateWindow(const QString &noteId);
    void updateAllWindows();

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
