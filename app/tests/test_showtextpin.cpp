// Скрепка "поверх всех окон" в открепляемом окне на QTest (см. 561).
// Без базы: проверяется сам флаг окна при переключении кнопки.
#include <QtTest>
#include <QApplication>
#include <QToolButton>

#include "libraries/wyedit/EditorShowText.h"

class TestShowTextPin : public QObject
{
    Q_OBJECT

private slots:

    void pinTogglesFlag(void);
};

void TestShowTextPin::pinTogglesFlag(void)
{
    EditorShowText win;

    QToolButton *pin=win.findChild<QToolButton *>();
    QVERIFY2(pin!=nullptr, "pin button must exist");
    QVERIFY2(pin->isCheckable(), "pin button must be checkable");

    // По умолчанию скрепка выключена
    QVERIFY2(!pin->isChecked(), "pin must be off by default");
    QVERIFY2(!(win.windowFlags() & Qt::WindowStaysOnTopHint),
             "window must not stay on top by default");

    // Включение скрепки ставит флаг окна
    pin->setChecked(true);
    QVERIFY2(win.windowFlags() & Qt::WindowStaysOnTopHint,
             "window must stay on top when pinned");

    // Выключение скрепки снимает флаг
    pin->setChecked(false);
    QVERIFY2(!(win.windowFlags() & Qt::WindowStaysOnTopHint),
             "window must not stay on top after unpin");
}

QTEST_MAIN(TestShowTextPin)

#include "test_showtextpin.moc"
