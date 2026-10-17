#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include <QList>
#include <QString>

// MCP-сервер (Model Context Protocol) поверх stdio.
// Запуск: mytetra --mcp-ro | --mcp-rw | --mcp-rw-with-delete
// (--mcp оставлен для совместимости и означает read-only).
// Чтение всегда, запись кроме ro, удаление только в старшем режиме.
// Второй процесс рядом с запущенной MyTetra безопасен: запись идет
// через живой GUI если он есть, иначе напрямую.
// Протокол: JSON-RPC 2.0, по одному сообщению на строку (NDJSON).
// Возвращает код выхода: 0 - штатно (EOF), 2 - нет конфига,
// 3 - база отсутствует или не читается
int runMcpServer(int argc, char **argv);


// Каталог тулов для страницы настроек: имя, категория ro/rw/delete,
// описание. Единый источник: категории из toolAllowed, описания
// из того же buildToolsList, что видит агент. Категории отвечают
// режимам --mcp-ro / --mcp-rw / --mcp-rw-with-delete
struct McpToolInfo
{
  QString name;
  QString category;
  QString description;
};

QList<McpToolInfo> mcpToolCatalog(void);

#endif // MCP_SERVER_H
