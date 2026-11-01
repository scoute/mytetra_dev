#include <QModelIndex>
#include <QJsonArray>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QThread>
#include <QCryptographicHash>
#include <QSet>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDomDocument>

#include <functional>
#include <algorithm>

#include "McpTools.h"
#include "main.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"
#include "models/recordTable/Record.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/BacklinkIndex.h"
#include "libraries/helpers/UniqueIdHelper.h"

#include <QFile>
#include "libraries/AppContext.h"

// Контекст сниппета поиска с каждой стороны от совпадения
static const int MCP_SNIPPET_CONTEXT=100;

McpTools::McpTools(KnowTreeModel *model, BacklinkIndex *backlinks) : m_model(model), m_backlinks(backlinks)
{
}
