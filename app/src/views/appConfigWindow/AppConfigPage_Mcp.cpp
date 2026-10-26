#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>
#include <QToolButton>
#include <QCommonStyle>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QDebug>

#include "AppConfigPage_Mcp.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/mcp/McpServer.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;


AppConfigPage_Mcp::AppConfigPage_Mcp(QWidget *parent) : ConfigPage(parent)
{
  setupUi();
  setupSignals();
  assembly();
}


AppConfigPage_Mcp::~AppConfigPage_Mcp()
{

}


void AppConfigPage_Mcp::setupUi(void)
{
  qDebug() << "Create mcp config page";

  // Галочка разрешения: без нее сервер отказывается стартовать,
  // а GUI игнорирует внешние mcp-команды
  mcpAllowCheck=new QCheckBox(this);
  mcpAllowCheck->setText(tr("Allow MCP access"));
  mcpAllowCheck->setChecked(mytetraConfig.get_mcpAllow());

  // Знак вопроса: тултип короткий, диалог по клику подробный
  mcpHelpButton=new QToolButton(this);
  QCommonStyle styleHelp;
  mcpHelpButton->setIcon(styleHelp.standardIcon(QStyle::SP_MessageBoxQuestion));
  mcpHelpButton->setToolTip(tr("What is MCP and how to connect it?"));

  // Рамка с примерами подключения клиентов
  clientBox=new QGroupBox(this);
  clientBox->setTitle(tr("Client setup (copy to client config)"));

  opencodeLabel=new QLabel(this);
  opencodeLabel->setText(tr("OpenCode (opencode.json):"));

  opencodeText=new QPlainTextEdit(this);
  opencodeText->setReadOnly(true);

  opencodeCopy=new QPushButton(this);
  opencodeCopy->setText(tr("Copy"));

  claudeLabel=new QLabel(this);
  claudeLabel->setText(tr("Claude Code (.mcp.json):"));

  claudeText=new QPlainTextEdit(this);
  claudeText->setReadOnly(true);

  claudeCopy=new QPushButton(this);
  claudeCopy->setText(tr("Copy"));

  retranslateExamples();

  // Список тулов из единого каталога: имя жирным, описание как есть.
  // Категории отвечают режимам --mcp-ro / --mcp-rw / --mcp-rw-with-delete
  toolsBox=new QGroupBox(this);

  roHeader=new QLabel(this);
  roBody=new QLabel(toolsSectionText(QStringLiteral("ro")), this);
  roBody->setWordWrap(true);
  roBody->setTextInteractionFlags(Qt::TextSelectableByMouse);

  rwHeader=new QLabel(this);
  rwBody=new QLabel(toolsSectionText(QStringLiteral("rw")), this);
  rwBody->setWordWrap(true);
  rwBody->setTextInteractionFlags(Qt::TextSelectableByMouse);

  deleteHeader=new QLabel(this);
  deleteBody=new QLabel(toolsSectionText(QStringLiteral("delete")), this);
  deleteBody->setWordWrap(true);
  deleteBody->setTextInteractionFlags(Qt::TextSelectableByMouse);

  retranslateTools();
}


void AppConfigPage_Mcp::setupSignals(void)
{
  connect(mcpHelpButton, &QToolButton::clicked, this, &AppConfigPage_Mcp::onHelpButton);
  connect(opencodeCopy, &QPushButton::clicked, this, &AppConfigPage_Mcp::onCopyOpencode);
  connect(claudeCopy, &QPushButton::clicked, this, &AppConfigPage_Mcp::onCopyClaude);
}


void AppConfigPage_Mcp::assembly(void)
{
  // Строка галочки со знаком вопроса
  QHBoxLayout *allowLayout=new QHBoxLayout();
  allowLayout->addWidget(mcpAllowCheck);
  allowLayout->addWidget(mcpHelpButton);
  allowLayout->addStretch();

  // Строки копирования прижимаются вправо
  QHBoxLayout *opencodeCopyLayout=new QHBoxLayout();
  opencodeCopyLayout->addStretch();
  opencodeCopyLayout->addWidget(opencodeCopy);

  QHBoxLayout *claudeCopyLayout=new QHBoxLayout();
  claudeCopyLayout->addStretch();
  claudeCopyLayout->addWidget(claudeCopy);

  // Примеры в рамке подключения
  QVBoxLayout *clientLayout=new QVBoxLayout();
  clientLayout->addWidget(opencodeLabel);
  clientLayout->addWidget(opencodeText);
  clientLayout->addLayout(opencodeCopyLayout);
  clientLayout->addWidget(claudeLabel);
  clientLayout->addWidget(claudeText);
  clientLayout->addLayout(claudeCopyLayout);
  clientBox->setLayout(clientLayout);

  // Секции каталога тулов
  QVBoxLayout *toolsLayout=new QVBoxLayout();
  toolsLayout->addWidget(roHeader);
  toolsLayout->addWidget(roBody);
  toolsLayout->addWidget(rwHeader);
  toolsLayout->addWidget(rwBody);
  toolsLayout->addWidget(deleteHeader);
  toolsLayout->addWidget(deleteBody);
  toolsBox->setLayout(toolsLayout);

  // Основной слой страницы
  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addLayout(allowLayout);
  centralLayout->addWidget(clientBox);
  centralLayout->addWidget(toolsBox);
  centralLayout->addStretch();

  setLayout(centralLayout);
}


// Текст секции каталога: по строке на тул. Вызывается и при
// построении, и при смене языка (заголовки со счетчиками)
QString AppConfigPage_Mcp::toolsSectionText(const QString &category)
{
  QStringList lines;

  foreach(const McpToolInfo &tool, mcpToolCatalog())
    if(tool.category==category)
      lines << QStringLiteral("<b>") + tool.name.toHtmlEscaped() +
               QStringLiteral("</b> &mdash; ") + tool.description.toHtmlEscaped();

  return lines.join(QStringLiteral("<br/>"));
}


int AppConfigPage_Mcp::toolsSectionCount(const QString &category)
{
  int count=0;

  foreach(const McpToolInfo &tool, mcpToolCatalog())
    if(tool.category==category)
      ++count;

  return count;
}


void AppConfigPage_Mcp::retranslateTools(void)
{
  toolsBox->setTitle(tr("Available tools"));

  roHeader->setText(tr("Read-only (ro) — %1 tools").arg(toolsSectionCount(QStringLiteral("ro"))));
  rwHeader->setText(tr("Read-write (rw) — %1 tools").arg(toolsSectionCount(QStringLiteral("rw"))));
  deleteHeader->setText(tr("Delete — %1 tools").arg(toolsSectionCount(QStringLiteral("delete"))));

  roBody->setText(toolsSectionText(QStringLiteral("ro")));
  rwBody->setText(toolsSectionText(QStringLiteral("rw")));
  deleteBody->setText(toolsSectionText(QStringLiteral("delete")));
}


// Путь к бинарнику в JSON: обратные слэши ломают JSON, меняются на прямые
static QString mcpBinaryPath(void)
{
  QString path=globalParameters.getMainProgramFile();

  if(path.isEmpty())
    path="/path/to/mytetra";

  path.replace("\\", "/");

  return path;
}


QString AppConfigPage_Mcp::opencodeExample(void)
{
  return "{\n"
         "  \"$schema\": \"https://opencode.ai/config.json\",\n"
         "  \"mcp\": {\n"
         "    \"mytetra\": {\n"
         "      \"type\": \"local\",\n"
         "      \"command\": [\""+mcpBinaryPath()+"\", \"--mcp-ro\"],\n"
         "      \"enabled\": true\n"
         "    }\n"
         "  }\n"
         "}";
}


QString AppConfigPage_Mcp::claudeExample(void)
{
  return "{\n"
         "  \"mcpServers\": {\n"
         "    \"mytetra\": {\n"
         "      \"command\": \""+mcpBinaryPath()+"\",\n"
         "      \"args\": [\"--mcp-ro\"]\n"
         "    }\n"
         "  }\n"
         "}";
}


void AppConfigPage_Mcp::retranslateExamples(void)
{
  opencodeText->setPlainText(opencodeExample());
  claudeText->setPlainText(claudeExample());
}


void AppConfigPage_Mcp::onHelpButton(void)
{
  QString text=tr("MCP lets an AI client (opencode, Claude Code) read and search SyncTetra notes.\n"
                  "\n"
                  "Enable the checkbox and press Apply. The client starts the server itself over stdio, no port is used. Paths in the examples below already point to this SyncTetra.\n"
                  "\n"
                  "Server modes:\n"
                  "--mcp-ro: read-only (recommended)\n"
                  "--mcp-rw: create and update notes, no delete\n"
                  "--mcp-rw-with-delete: full write, deletes go to trash\n"
                  "\n"
                  "While the AI writes, SyncTetra shows a banner. Keep read-only unless writes are needed.\n"
                  "\n"
                  "Claude Code terminal way:\n"
                  "claude mcp add --transport stdio synctetra -- BINARY --mcp-ro");

  // Путь в подсказке терминала тоже реальный, а не плейсхолдер
  text.replace("BINARY", mcpBinaryPath());

  QMessageBox msgBox;
  msgBox.setWindowTitle(tr("MCP help"));
  msgBox.setText(text);
  msgBox.exec();
}


void AppConfigPage_Mcp::onCopyOpencode(void)
{
  QApplication::clipboard()->setText(opencodeText->toPlainText());
}


void AppConfigPage_Mcp::onCopyClaude(void)
{
  QApplication::clipboard()->setText(claudeText->toPlainText());
}


void AppConfigPage_Mcp::retranslateUi(void)
{
  mcpAllowCheck->setText(tr("Allow MCP access"));
  mcpHelpButton->setToolTip(tr("What is MCP and how to connect it?"));

  clientBox->setTitle(tr("Client setup (copy to client config)"));
  opencodeLabel->setText(tr("OpenCode (opencode.json):"));
  opencodeCopy->setText(tr("Copy"));
  claudeLabel->setText(tr("Claude Code (.mcp.json):"));
  claudeCopy->setText(tr("Copy"));

  retranslateExamples();
  retranslateTools();
}


// Перезапуск не нужен: сервер читает галочку при каждом старте,
// GUI проверяет ее в момент команды
int AppConfigPage_Mcp::applyChanges(void)
{
  qDebug() << "Apply changes mcp";

  if(mytetraConfig.get_mcpAllow()!=mcpAllowCheck->isChecked())
    mytetraConfig.set_mcpAllow(mcpAllowCheck->isChecked());

  return 0;
}
