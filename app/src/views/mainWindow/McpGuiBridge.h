#ifndef MCPGUIBRIDGE_H
#define MCPGUIBRIDGE_H

#include <functional>

#include <QObject>
#include <QMap>
#include <QList>
#include <QSet>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

#include "views/mcpHistory/McpHistoryDialog.h"

// Мост "MCP-сервер -> живой GUI". Третье классовое выделение (P3):
// handleMcpMessage/handleMcpCommand (~800 строк) уезжают из
// MainWindow-god в отдельный QObject с DI.
// Зависимости внутрь:
// - KnowTreeModel*: живая модель (указатель стабилен: reload in-place,
//   смена базы - тот же объект через initFromXML). Передается в ctor
// - isReadOnly: mytetraConfig.isGlobalReadOnly (меняется рантаймом)
// - mcpAllowed: mytetraConfig.get_mcpAllow (переключается настройками)
// - clearEditor: сброс MetaEditor при move_note (иначе висячий указатель
//   в таблицу-источник). MainWindow передает лямбду с find_object
// Состояние сессии (пиры, режимы, история) живет здесь, а не в окне.
// Обратно в окно - только сигналы: баннер, диалог истории, показ
// диалога при первой сессии. MainWindow лишь рендерит

class KnowTreeModel;

class McpGuiBridge : public QObject
{
    Q_OBJECT

public:

    explicit McpGuiBridge(KnowTreeModel *model, QObject *parent=nullptr);
    ~McpGuiBridge(void) override;

    void setModel(KnowTreeModel *model);

    // DI-хуки. MainWindow ставит в конструкторе, пробы - лямбды
    std::function<bool()> isReadOnly;
    std::function<bool()> mcpAllowed;
    std::function<void()> clearEditor;

    // Синхронный ответ на mcp-команду для сокетного протокола
    // запрос-ответ: компактный JSON {"ok":true,...} или {"ok":false,...}
    QByteArray handleMcpMessage(const QString &message);

    // Применение одной mcp-команды к живой модели с возвратом итога.
    // Используется респондером и легаси-путём messageHandler
    bool handleMcpCommand(const QString &command,
                          const QJsonObject &args,
                          QJsonObject &result,
                          QString &error);

    // Парсер аргументов команды (был MainWindow static, нужен и
    // легаси-пути messageHandler - теперь общий через мост)
    static QJsonObject parseMcpCommandArgs(const QString &message);

    // Состояние для рендера баннера/диалога в MainWindow
    bool hasPeers(void) const;
    QString bannerText(void);

    const QList<McpHistoryEntry> &history(void) const;
    void clearHistory(void);

    // Тик таймера: выкинуть мертвые пиры (проверка имени бинаря
    // против recycled pid). При чистке - сигнал sessionChanged
    void checkPeers(void);

signals:

    // Пиры/история поменялись: MainWindow перерисовывает баннер
    // (updateMcpBanner читает hasPeers/bannerText)
    void sessionChanged(void);

    // История пополнилась/почищена: MainWindow обновляет диалог
    // (updateMcpHistoryDialog читает history())
    void historyChanged(void);

    // Первый пир сессии: MainWindow показывает диалог истории
    // (updateMcpHistoryDialog + show, как было в mcpHello)
    void sessionStarted(void);

private:

    // Внешняя MCP-правка: баннер и так висит всю сессию, здесь
    // только сигнал на пересчёт (текст с режимами, видимость)
    void flashMcpWrite(void);

    // Запись в журнал сессии с обрезкой до 5000 (как было)
    void addMcpHistoryEntry(const QString &kind,
                            const QString &tool,
                            const QString &summary);

    // Fail-closed гейты поверх хуков (пробы без конфига - отказ, не падение)
    bool readOnly(void) const;
    bool mcpAllowedCheck(void) const;

    KnowTreeModel *m_model=nullptr;

    QMap<QString, qint64> m_mcpPeers;
    QMap<QString, QString> m_mcpPeerModes;
    QList<McpHistoryEntry> m_mcpHistory;
};

#endif // MCPGUIBRIDGE_H
