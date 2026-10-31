#ifndef MCP_TOOLS_PRIVATE_H
#include <functional>
#define MCP_TOOLS_PRIVATE_H

// P3: общие хелперы McpTools между TU (были file-static в общем файле)

class KnowTreeModel;
class TreeItem;

TreeItem *modelRoot(KnowTreeModel *model);

class Record;

void forEachRecord(TreeItem *item,
                     std::function<void(Record*, TreeItem*)> visit);
#endif // MCP_TOOLS_PRIVATE_H
