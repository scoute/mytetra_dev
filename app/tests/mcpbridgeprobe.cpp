// McpGuiBridge стенд: сессии, гейты, баннер. Без модели и MainWindow:
// isReadOnly/mcpAllowed/clearEditor - лямбды (чистый DI).
#include <QApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#include "views/mainWindow/McpGuiBridge.h"

static int fails=0;
static void check(bool ok, const char *name)
{
    qDebug() << (ok ? "PASS" : "FAIL") << name;
    if(!ok)
        ++fails;
}

static QJsonObject parse(const QByteArray &reply)
{
    return QJsonDocument::fromJson(reply).object();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    McpGuiBridge bridge(nullptr);
    bridge.isReadOnly=[]() { return false; };
    bridge.mcpAllowed=[]() { return true; };
    bool cleared=false;
    bridge.clearEditor=[&]() { cleared=true; };

    int changed=0, histChanged=0, started=0;
    QObject::connect(&bridge, &McpGuiBridge::sessionChanged, [&](){ ++changed; });
    QObject::connect(&bridge, &McpGuiBridge::historyChanged, [&](){ ++histChanged; });
    QObject::connect(&bridge, &McpGuiBridge::sessionStarted, [&](){ ++started; });

    // Hello: пир + сессия
    QJsonObject r=parse(bridge.handleMcpMessage("mcpHello {\"pid\": \"999999\", \"mode\": \"ro\"}"));
    check(r.value("ok").toBool()==true, "HELLO-OK");
    check(bridge.hasPeers(), "HELLO-PEERS");
    check(changed==1, "HELLO-SESSION-SIGNAL");
    check(started==1, "HELLO-STARTED-SIGNAL");
    check(bridge.bannerText().contains("MCP"), "BANNER-TEXT");

    // History: запись + сигнал
    r=parse(bridge.handleMcpMessage("mcpHistory {\"kind\": \"t\", \"tool\": \"x\", \"summary\": \"s\"}"));
    check(r.value("ok").toBool()==true, "HIST-OK");
    check(bridge.history().size()==1, "HIST-SIZE");
    check(histChanged>=1, "HIST-SIGNAL");

    // Bye: пир ушел
    r=parse(bridge.handleMcpMessage("mcpBye {\"pid\": \"999999\"}"));
    check(r.value("ok").toBool()==true, "BYE-OK");
    check(!bridge.hasPeers(), "BYE-NOPEERS");

    // Неизвестная команда
    r=parse(bridge.handleMcpMessage("mcpNope {}"));
    check(r.value("ok").toBool()==false, "UNKNOWN-FAIL");

    // Гейт mcpAllowed
    bridge.mcpAllowed=[]() { return false; };
    r=parse(bridge.handleMcpMessage("mcpHello {\"pid\": \"1\"}"));
    check(r.value("ok").toBool()==false, "DISABLED-FAIL");
    bridge.mcpAllowed=[]() { return true; };

    // Гейт readOnly: до модели не доходит, clearEditor не дергается
    bridge.isReadOnly=[]() { return true; };
    cleared=false;
    r=parse(bridge.handleMcpMessage("mcpCreateBranch {\"parent_id\": \"\", \"id\": \"x\", \"name\": \"n\"}"));
    check(r.value("ok").toBool()==false, "READONLY-FAIL");
    check(r.value("error").toString().contains("read-only"), "READONLY-TEXT");
    check(!cleared, "READONLY-NOEDIT");
    bridge.isReadOnly=[]() { return false; };

    // checkPeers: несуществующий pid вычищается
    bridge.handleMcpMessage("mcpHello {\"pid\": \"999998\"}");
    int before=changed;
    bridge.checkPeers();
    check(!bridge.hasPeers(), "TICK-CLEAN");
    check(changed>before, "TICK-SIGNAL");

    qDebug() << (fails==0 ? "MCPBRIDGEPROBE-OK" : "MCPBRIDGEPROBE-BAD");
    return fails;
}
