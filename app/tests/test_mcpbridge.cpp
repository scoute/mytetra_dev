// McpGuiBridge на QTest (см. 523).
// Порт mcpbridgeprobe: сессии, гейты, баннер. Без модели и MainWindow:
// isReadOnly/mcpAllowed/clearEditor - лямбды (чистый DI).
#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>

#include "views/mainWindow/McpGuiBridge.h"

class TestMcpBridge : public QObject
{
    Q_OBJECT

private slots:

    void helloAndHistory(void);
    void byeAndUnknown(void);
    void gates(void);
    void tickCleansDeadPeers(void);

private:

    static QJsonObject parse(const QByteArray &reply);
};

QJsonObject TestMcpBridge::parse(const QByteArray &reply)
{
    return QJsonDocument::fromJson(reply).object();
}

void TestMcpBridge::helloAndHistory(void)
{
    McpGuiBridge bridge(nullptr);
    bridge.isReadOnly=[]() { return false; };
    bridge.mcpAllowed=[]() { return true; };

    int changed=0, histChanged=0, started=0;
    connect(&bridge, &McpGuiBridge::sessionChanged, [&](){ ++changed; });
    connect(&bridge, &McpGuiBridge::historyChanged, [&](){ ++histChanged; });
    connect(&bridge, &McpGuiBridge::sessionStarted, [&](){ ++started; });

    // Hello: пир + сессия
    QJsonObject r=parse(bridge.handleMcpMessage("mcpHello {\"pid\": \"999999\", \"mode\": \"ro\"}"));
    QCOMPARE(r.value("ok").toBool(), true);
    QVERIFY2(bridge.hasPeers(), "peer must be registered");
    QCOMPARE(changed, 1);
    QCOMPARE(started, 1);
    QVERIFY2(bridge.bannerText().contains("MCP"), "banner must mention MCP");

    // History: запись + сигнал
    r=parse(bridge.handleMcpMessage("mcpHistory {\"kind\": \"t\", \"tool\": \"x\", \"summary\": \"s\"}"));
    QCOMPARE(r.value("ok").toBool(), true);
    QCOMPARE(bridge.history().size(), 1);
    QVERIFY2(histChanged>=1, "history signal must fire");
}

void TestMcpBridge::byeAndUnknown(void)
{
    McpGuiBridge bridge(nullptr);
    bridge.isReadOnly=[]() { return false; };
    bridge.mcpAllowed=[]() { return true; };

    QJsonObject r=parse(bridge.handleMcpMessage("mcpHello {\"pid\": \"999999\", \"mode\": \"ro\"}"));
    QCOMPARE(r.value("ok").toBool(), true);

    // Bye: пир ушел
    r=parse(bridge.handleMcpMessage("mcpBye {\"pid\": \"999999\"}"));
    QCOMPARE(r.value("ok").toBool(), true);
    QVERIFY2(!bridge.hasPeers(), "no peers must remain");

    // Неизвестная команда
    r=parse(bridge.handleMcpMessage("mcpNope {}"));
    QCOMPARE(r.value("ok").toBool(), false);
}

void TestMcpBridge::gates(void)
{
    McpGuiBridge bridge(nullptr);
    bridge.isReadOnly=[]() { return false; };
    bridge.mcpAllowed=[]() { return true; };
    bool cleared=false;
    bridge.clearEditor=[&]() { cleared=true; };

    // Гейт mcpAllowed
    bridge.mcpAllowed=[]() { return false; };
    QJsonObject r=parse(bridge.handleMcpMessage("mcpHello {\"pid\": \"1\"}"));
    QCOMPARE(r.value("ok").toBool(), false);
    bridge.mcpAllowed=[]() { return true; };

    // Гейт readOnly: до модели не доходит, clearEditor не дергается
    bridge.isReadOnly=[]() { return true; };
    r=parse(bridge.handleMcpMessage("mcpCreateBranch {\"parent_id\": \"\", \"id\": \"x\", \"name\": \"n\"}"));
    QCOMPARE(r.value("ok").toBool(), false);
    QVERIFY2(r.value("error").toString().contains("read-only"),
             "error must mention read-only");
    QVERIFY2(!cleared, "editor must not be cleared");
}

void TestMcpBridge::tickCleansDeadPeers(void)
{
    McpGuiBridge bridge(nullptr);
    bridge.isReadOnly=[]() { return false; };
    bridge.mcpAllowed=[]() { return true; };

    int changed=0;
    connect(&bridge, &McpGuiBridge::sessionChanged, [&](){ ++changed; });

    // checkPeers: несуществующий pid вычищается
    bridge.handleMcpMessage("mcpHello {\"pid\": \"999998\"}");
    int before=changed;
    bridge.checkPeers();
    QVERIFY2(!bridge.hasPeers(), "dead peer must be cleaned");
    QVERIFY2(changed>before, "cleanup signal must fire");
}

QTEST_MAIN(TestMcpBridge)

#include "test_mcpbridge.moc"
