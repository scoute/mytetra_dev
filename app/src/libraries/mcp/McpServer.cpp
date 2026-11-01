// MCP-сервер MyTetra: JSON-RPC 2.0 поверх stdio (NDJSON).
// Только чтение базы. Второй процесс рядом с запущенной MyTetra
// безопасен: писатель один (GUI), читатель видит сохраненное
// состояние, структура подтягивается по mtime mytetra.xml

#include <QCoreApplication>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QFileInfo>
#include <QDateTime>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QLocalSocket>
#include <QDataStream>
#include <QLibrary>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#else
#include <unistd.h>
#endif
#include <cstdio>

#include "McpServer.h"
#include "McpTools.h"
#include "main.h"
#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "libraries/RandomInitter.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "models/appConfig/AppConfig.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"

// Версии протокола MCP которые понимает сервер.
// Запрошенная клиентом поддерживаемая версия эхомится назад,
// незнакомая сводится к самой старой стабильной

#include "McpServerPrivate.h"
#include "libraries/AppContext.h"
#include "models/appConfig/AppConfigStorage.h"
#include "models/appConfig/AppConfigServices.h"

// Pre-валидация базы до initFromXML: в headless-режиме диалоги
// XmlTree::load и criticalError формата роняют процесс,
// поэтому файл проверяется вручную с чистым выходом в stderr
bool validateDatabase(const QString &xmlPath)
{
  QFile xmlFile(xmlPath);

  if(!xmlFile.open(QIODevice::ReadOnly))
  {
    fprintf(stderr, "mcp: can not open database file %s\n", xmlPath.toLocal8Bit().data());
    return false;
  }

  QByteArray xmlData=xmlFile.readAll();
  xmlFile.close();

  QDomDocument doc;
  QString errorText;
  int errorLine=0;
  int errorColumn=0;

  if(!doc.setContent(xmlData, true, &errorText, &errorLine, &errorColumn))
  {
    fprintf(stderr, "mcp: can not parse database file %s: %s at %d:%d\n",
            xmlPath.toLocal8Bit().data(), errorText.toLocal8Bit().data(),
            errorLine, errorColumn);
    return false;
  }

  QDomElement format=doc.documentElement().firstChildElement("format");
  int version=format.attribute("version", "0").toInt();
  int subversion=format.attribute("subversion", "0").toInt();

  // Та же проверка что KnowTreeModel::checkFormat
  if(version>CURRENT_FORMAT_VERSION || subversion>CURRENT_FORMAT_SUBVERSION)
  {
    fprintf(stderr, "mcp: unsupported database format %d.%d\n", version, subversion);
    return false;
  }

  return true;
}

// Каталог тулов для страницы настроек. Категории отвечают режимам:
// ro виден в --mcp-ro, rw добавляется в --mcp-rw,
// delete только в --mcp-rw-with-delete

// Каталог тулов для страницы настроек. Категории отвечают режимам:
// ro виден в --mcp-ro, rw добавляется в --mcp-rw,
// delete только в --mcp-rw-with-delete
QList<McpToolInfo> mcpToolCatalog(void)
{
  QList<McpToolInfo> catalog;

  const QJsonArray tools=buildToolsList(McpMode::ReadWriteDelete);

  foreach(const QJsonValue &value, tools)
  {
    const QJsonObject tool=value.toObject();
    const QString name=tool.value("name").toString();

    McpToolInfo info;
    info.name=name;
    info.description=tool.value("description").toString();

    if(toolAllowed(McpMode::ReadOnly, name))
      info.category=QStringLiteral("ro");
    else if(toolAllowed(McpMode::ReadWrite, name))
      info.category=QStringLiteral("rw");
    else
      info.category=QStringLiteral("delete");

    catalog.append(info);
  }

  return catalog;
}

int runMcpServer(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  AppContext::instance().globalParameters()->setMainProgramFile(QString::fromLocal8Bit(argv[0]));

  // ГПСЧ первым: trash-бэкап в AppContext::instance().appConfig()->init() уже зовёт rand()
  // для имён копий, а сдвиг pid разводит одновременно стартовавшие
  // серверы по разным сериям (иначе одинаковые имена и id)
  RandomInitter::init(static_cast<long>(QCoreApplication::applicationPid()));

  // Headless boot: initHeadless never opens InstallDialog,
  // missing conf.ini is a clean exit instead of a crash
  if(!AppContext::instance().globalParameters()->initHeadless())
  {
    fprintf(stderr, "mcp: conf.ini not found, can not locate MyTetra data\n");
    return 2;
  }

  AppContext::instance().appConfig()->init();

  // Доступ закрыт галочкой на странице MCP: сервер не стартует.
  // Код 4 чтобы отличать от отсутствия conf.ini (2) и битой базы (3)
  if(!AppContext::instance().appConfig()->servicesSection()->get_mcpAllow())
  {
    fprintf(stderr, "mcp: access is disabled, allow it on the MCP settings page\n");
    return 4;
  }

  AppContext::instance().dataBaseConfig()->init();

  QString xmlPath=AppContext::instance().appConfig()->storageSection()->get_tetradir()+"/mytetra.xml";

  // Какая база обслужвается: при расхождении цифр с GUI первым делом
  // сверить этот путь с tetradir в conf.ini запущенной программы
  fprintf(stderr, "mcp: base %s\n", AppContext::instance().appConfig()->storageSection()->get_tetradir().toUtf8().constData());

  if(!validateDatabase(xmlPath))
    return 3;

  McpMode mode=mcpModeFromArgs(argc, argv);

  // P3: индекс и диспетчер внедряются из контекста (были ::instance()).
  // AppContext::init уже вызван в main() до раннего возврата сюда
  KnowTreeModel model(AppContext::instance().backlinkIndex(),
                      AppContext::instance().dispatcher());
  model.initFromXML(xmlPath);

  McpTools tools(&model, AppContext::instance().backlinkIndex());
  McpSession session(model, xmlPath, tools,
                     AppContext::instance().backlinkIndex(),
                     QString::fromLocal8Bit(argv[0]), mode);

  // GUI-баннер: я жив. Парный Bye — в конце цикла
  notifyMcpPresence(QString::fromLocal8Bit(argv[0]), mode, "mcpHello");

  QFile stdIn;
  stdIn.open(stdin, QIODevice::ReadOnly);

  QFile stdOut;
  stdOut.open(stdout, QIODevice::WriteOnly);

  // Только ответы протокола уходят в stdout, диагностика - в stderr
  while(true)
  {
    QByteArray line=stdIn.readLine();

    if(line.isNull())
      break; // EOF: клиент закрыл канал

    line=line.trimmed();

    if(line.isEmpty())
      continue;

    QByteArray response;

    if(session.handleMessage(line, response))
    {
      stdOut.write(response+"\n");
      stdOut.flush();
    }
  }

  // Чистый выход: GUI гасит баннер когда уйдут все пиры
  notifyMcpPresence(QString::fromLocal8Bit(argv[0]), mode, "mcpBye");

  return 0;
}
