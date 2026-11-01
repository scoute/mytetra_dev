#ifndef PERIODICCHECKBASE_H
#define PERIODICCHECKBASE_H

#include "TimerMonitoring.h"
#include "models/appConfig/AppConfigSync.h"
#include "libraries/AppContext.h"

// Класс, который запускает периодическую проверку того,
// что сторонняя программа изменила файл дерева на диске.
// Функционал класа работает только в случае, если в настройках
// был установлен флажок
// "Периодическая проверка дерева записей на изменение сторонними программами"
// получаемый через AppContext::instance().appConfig()->syncSection()->getEnablePeriodicCheckBase()


class EditorShowTextDispatcher;

class PeriodicCheckBase : public TimerMonitoring
{
    Q_OBJECT

public:
    // P3: диспетчер внедряется из main() (был ::instance())
    void setShowTextDispatcher(EditorShowTextDispatcher *dispatcher);

    void init();

signals:

    void doUpdateDetachedWindows();

protected:

    bool isStartEnabled();
    void timerEvent(QTimerEvent *event);

    // P3: внедренный диспетчер (DI вместо синглтона)
    EditorShowTextDispatcher *m_dispatcher=nullptr;

};

#endif	/* PERIODICCHECKBASE_H */

