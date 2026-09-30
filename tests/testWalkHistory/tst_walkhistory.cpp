#include <QtTest>

#include <unistd.h>

#include <QApplication>
#include <QFileInfo>

// Заголовки глобальных объектов приложения
#include "libraries/FixedParameters.h"
#include "libraries/GlobalParameters.h"
#include "libraries/WalkHistory.h"
#include "libraries/ActionLogger.h"
#include "libraries/TrashMonitoring.h"
#include "libraries/ShortcutManager.h"
#include "libraries/PeriodicCheckBase.h"
#include "libraries/PeriodicSyncro.h"
#include "libraries/InternalClipboard.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppFiles.h"
#include "models/dataBaseConfig/DataBaseConfig.h"


// Глобальные объекты приложения. В приложении они определены
// в app/src/main.cpp, который в тесты не линкуется (там функция main),
// поэтому определяются здесь один в один как в main.cpp.
FixedParameters fixedParameters;
GlobalParameters globalParameters;
AppConfig mytetraConfig;
AppFiles mytetraFiles;
DataBaseConfig dataBaseConfig;
TrashMonitoring trashMonitoring;
WalkHistory walkHistory;
ActionLogger actionLogger;
ShortcutManager shortcutManager;
PeriodicCheckBase periodicCheckBase;
PeriodicSyncro periodicSyncro;
InternalClipboard *internalClipboard = nullptr;
QObject *pMainWindow = nullptr;


// Тестовый класс для проверки истории посещения записей.
// Проверяются прямые посещения, навигация назад-вперед (как ее
// вызывает MainWindow: add с текущим id и режимом PREVIOUS/NEXT,
// затем getId для записи перехода), флаг сброса, лимит истории
// и хранение позиций курсора и прокрутки.

class TestWalkHistory : public QObject
{
    Q_OBJECT

private slots:

    // Проверка изоляции окружения: рабочая директория обязана совпадать
    // с директорией тестового бинаря (там лежит тестовый conf.ini).
    // Иначе тест трогал бы данные пользователя.
    void initTestCase()
    {
        QString binaryDir = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
        QCOMPARE(globalParameters.getWorkDirectory(), binaryDir);
        QVERIFY(mytetraConfig.is_init());
    }

    // Проверка пустой истории после очистки
    void emptyAfterClear()
    {
        WalkHistory history;
        history.clear();

        QCOMPARE(history.getId(), QString());
        QCOMPARE(history.getCursorPosition("no-such-id"), 0);
        QCOMPARE(history.getScrollBarPosition("no-such-id"), 0);
    }

    // Проверка игнорирования пустого идентификатора
    void emptyIdIgnored()
    {
        WalkHistory history;
        history.clear();

        history.add("", 10, 20);
        QCOMPARE(history.getId(), QString());
    }

    // Проверка одиночного посещения и запоминания позиций
    void singleVisit()
    {
        WalkHistory history;
        history.clear();

        history.add("note-a", 10, 20);
        QCOMPARE(history.getId(), QString("note-a"));
        QCOMPARE(history.getCursorPosition("note-a"), 10);
        QCOMPARE(history.getScrollBarPosition("note-a"), 20);
    }

    // Проверка обновления позиций при повторном посещении той же записи.
    // Повторяющийся идентификатор в историю не дублируется.
    void revisitUpdatesPositions()
    {
        WalkHistory history;
        history.clear();

        history.add("note-a", 1, 2);
        history.add("note-b", 3, 4);
        QCOMPARE(history.getId(), QString("note-b"));

        // Возврат на note-a: новый визит, позиции обновляются
        history.add("note-a", 100, 200);
        QCOMPARE(history.getId(), QString("note-a"));
        QCOMPARE(history.getCursorPosition("note-a"), 100);
        QCOMPARE(history.getScrollBarPosition("note-a"), 200);
    }

    // Проверка флага сброса: пока флаг поднят, визиты не запоминаются
    void dropFlag()
    {
        WalkHistory history;
        history.clear();

        history.setDrop(true);
        history.add("note-x", 1, 1);
        QCOMPARE(history.getId(), QString());

        history.setDrop(false);
        history.add("note-x", 1, 1);
        QCOMPARE(history.getId(), QString("note-x"));
    }

    // Проверка удаления данных о позициях записи
    void removeHistoryData()
    {
        WalkHistory history;
        history.clear();

        history.add("note-a", 7, 8);
        QCOMPARE(history.getCursorPosition("note-a"), 7);

        history.removeHistoryData("note-a");
        QCOMPARE(history.getCursorPosition("note-a"), 0);
        QCOMPARE(history.getScrollBarPosition("note-a"), 0);
    }

    // Проверка полного сценария навигации как в MainWindow:
    // визиты a -> b -> c, затем Назад, Назад, Вперед, Вперед, Вперед
    // и новый визит. Ожидаемый ряд: c, b, a, b, c, c, d.
    void fullNavigationScenario()
    {
        WalkHistory history;
        history.clear();

        history.add("a", 1, 1);
        history.add("b", 2, 2);
        history.add("c", 3, 3);
        QCOMPARE(history.getId(), QString("c"));

        // Назад с записи c: переход на b
        history.add("c", 30, 30, WALK_HISTORY_GO_PREVIOUS);
        QCOMPARE(history.getId(), QString("b"));

        // Назад с записи b: переход на a
        history.add("b", 20, 20, WALK_HISTORY_GO_PREVIOUS);
        QCOMPARE(history.getId(), QString("a"));

        // Вперед с записи a: переход на b
        history.add("a", 10, 10, WALK_HISTORY_GO_NEXT);
        QCOMPARE(history.getId(), QString("b"));

        // Вперед с записи b: переход на c
        history.add("b", 21, 21, WALK_HISTORY_GO_NEXT);
        QCOMPARE(history.getId(), QString("c"));

        // Вперед в конце истории: остаемся на c
        history.add("c", 31, 31, WALK_HISTORY_GO_NEXT);
        QCOMPARE(history.getId(), QString("c"));

        // Новый визит d: указатель на последней записи
        history.add("d", 4, 4);
        QCOMPARE(history.getId(), QString("d"));

        // Позиции сохранились для всех записей
        QCOMPARE(history.getCursorPosition("a"), 10);
        QCOMPARE(history.getCursorPosition("d"), 4);
    }

    // Проверка обрезки переполненной истории: лимит WALK_HISTORY_MAX,
    // указатель остается на последней записи, навигация работает
    void overflowTrim()
    {
        WalkHistory history;
        history.clear();

        const int count = WALK_HISTORY_MAX + 10;
        for (int i = 0; i < count; ++i)
        {
            history.add(QString("id-%1").arg(i), i, i);
        }

        QString lastId = QString("id-%1").arg(count - 1);
        QCOMPARE(history.getId(), lastId);
        QCOMPARE(history.getCursorPosition(lastId), count - 1);

        // Шаг назад от последней записи ведет на предпоследнюю
        history.add(lastId, 0, 0, WALK_HISTORY_GO_PREVIOUS);
        QCOMPARE(history.getId(), QString("id-%1").arg(count - 2));
    }

    // Проверка повтора последнего идентификатора: в историю не дублируется
    void repeatLastIdIgnored()
    {
        WalkHistory history;
        history.clear();

        history.add("note-a", 1, 2);
        history.add("note-a", 3, 4);
        QCOMPARE(history.getId(), QString("note-a"));

        // Позиции обновились, а дубликата в истории нет:
        // шаг назад с единственной записи никуда не ведет
        QCOMPARE(history.getCursorPosition("note-a"), 3);
        history.add("note-a", 0, 0, WALK_HISTORY_GO_PREVIOUS);
        QCOMPARE(history.getId(), QString("note-a"));
    }

    // Проверка режимов навигации на пустой истории:
    // идентификатор просто запоминается
    void navigationModesOnEmptyHistory()
    {
        WalkHistory history;
        history.clear();

        history.add("note-a", 1, 1, WALK_HISTORY_GO_PREVIOUS);
        QCOMPARE(history.getId(), QString("note-a"));

        history.clear();
        history.add("note-b", 2, 2, WALK_HISTORY_GO_NEXT);
        QCOMPARE(history.getId(), QString("note-b"));
    }

    // Проверка движения вперед в конце истории: указатель не сдвигается
    void nextAtEndStaysPut()
    {
        WalkHistory history;
        history.clear();

        history.add("a", 1, 1);
        history.add("b", 2, 2);

        history.add("b", 0, 0, WALK_HISTORY_GO_NEXT);
        QCOMPARE(history.getId(), QString("b"));
    }

    // Проверка добавления новой записи в режиме навигации назад,
    // когда указатель в конце истории
    void appendNewIdWhileNavigating()
    {
        WalkHistory history;
        history.clear();

        history.add("a", 1, 1);
        history.add("b", 2, 2);

        history.add("c", 3, 3, WALK_HISTORY_GO_PREVIOUS);
        QCOMPARE(history.getId(), QString("b"));
    }

    // Проверка обрезки истории при активной метке возврата:
    // указатель корректируется вместе с историей
    void trimWithActiveLeaveMark()
    {
        WalkHistory history;
        history.clear();

        history.add("a", 1, 1);
        history.add("b", 2, 2);
        history.add("c", 3, 3);

        // Шаг назад поднимает метку начала движения по истории
        history.add("c", 0, 0, WALK_HISTORY_GO_PREVIOUS);
        QCOMPARE(history.getId(), QString("b"));

        const int count = WALK_HISTORY_MAX + 10;
        for (int i = 0; i < count; ++i)
        {
            history.add(QString("id-%1").arg(i), i, i);
        }

        QString lastId = QString("id-%1").arg(count - 1);
        QCOMPARE(history.getId(), lastId);
    }

    // Проверка обрезки истории во время движения назад: указатель
    // стоит в начале, метка возврата положительна и корректируется
    // вместе с историей. Режим PREVIOUS метку не сбрасывает.
    void trimWhileNavigatingBackward()
    {
        WalkHistory history;
        history.clear();

        history.add("a", 1, 1);
        history.add("b", 2, 2);
        history.add("c", 3, 3);

        // Шаг назад: указатель в середине, метка возврата поднята
        history.add("c", 0, 0, WALK_HISTORY_GO_PREVIOUS);
        QCOMPARE(history.getId(), QString("b"));

        // Серия новых записей в режиме навигации назад доводит
        // историю до лимита, указатель упирается в начало
        const int count = WALK_HISTORY_MAX;
        for (int i = 0; i < count; ++i)
        {
            history.add(QString("n-%1").arg(i), i, i, WALK_HISTORY_GO_PREVIOUS);
        }

        // Два самых старых элемента вытеснены обрезкой,
        // указатель стоит на первом оставшемся
        QCOMPARE(history.getId(), QString("c"));
    }
};


// Сброс данных покрытия перед _exit(). _exit() не вызывает atexit-обработчики,
// поэтому без явного сброса gcov теряет данные прогона. libgcov линкуется явно
// через mytetra_harness.pri. Без флагов покрытия в объектах приложения вызов
// безопасен: сбрасывать нечего, функция просто ничего не пишет.
#ifdef MYTETRA_HAS_GCOV_DUMP
extern "C" void __gcov_dump(void);
#endif


// Собственная функция main вместо QTEST_MAIN.
// Повторяет начало main() приложения: запоминается имя бинаря
// (по нему ищется conf.ini), инициализируются глобальные параметры
// и конфиг. Платформа offscreen включается принудительно.
// Выход через _exit(): деструктор глобального AppConfig делает sync()
// уже после смерти QApplication и падает в Qt-кодеках.
int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    QApplication app(argc, argv);

    QString mainProgramFile = QString::fromLocal8Bit(argv[0]);
    globalParameters.setMainProgramFile(mainProgramFile);

    globalParameters.init();
    mytetraConfig.init();

    TestWalkHistory test;
    int code = QTest::qExec(&test, argc, argv);

#ifdef MYTETRA_HAS_GCOV_DUMP
    __gcov_dump();
#endif

    _exit(code);
    return code;
}

#include "tst_walkhistory.moc"
