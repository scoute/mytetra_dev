#ifndef MCP_SERVER_H
#define MCP_SERVER_H

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

#endif // MCP_SERVER_H
