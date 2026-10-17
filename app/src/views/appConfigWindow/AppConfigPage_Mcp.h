#ifndef APPCONFIGPAGE_MCP_H
#define APPCONFIGPAGE_MCP_H

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QCheckBox;
class QToolButton;
class QLabel;
class QPlainTextEdit;
class QPushButton;

// Страница MCP: разрешение доступа внешнему AI-клиенту
// и копируемые примеры конфигов opencode / Claude Code
class AppConfigPage_Mcp : public ConfigPage
{
    Q_OBJECT

public:
    AppConfigPage_Mcp(QWidget *parent = nullptr);
    virtual ~AppConfigPage_Mcp();

    void retranslateUi(void);
    int applyChanges(void);

private slots:
    void onHelpButton(void);
    void onCopyOpencode(void);
    void onCopyClaude(void);

protected:
    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    // Примеры с путем к бинарнику для вставки в конфиг клиента
    QString opencodeExample(void);
    QString claudeExample(void);

    void retranslateExamples(void);

    // Секция каталога одной категории в HTML, счетчик тулов,
    // перевод заголовков секций
    QString toolsSectionText(const QString &category);
    int toolsSectionCount(const QString &category);
    void retranslateTools(void);

    // Разрешение запуска MCP-сервера внешним клиентом, по умолчанию выключено
    QCheckBox *mcpAllowCheck;

    // Знак вопроса с подсказками про режимы и безопасность
    QToolButton *mcpHelpButton;

    // Рамка с примерами подключения клиентов
    QGroupBox *clientBox;
    QLabel *opencodeLabel;
    QPlainTextEdit *opencodeText;
    QPushButton *opencodeCopy;
    QLabel *claudeLabel;
    QPlainTextEdit *claudeText;
    QPushButton *claudeCopy;

    // Рамка со списком тулов по категориям. Тексты описаний
    // английские, как их видит агент; заголовки переводятся
    QGroupBox *toolsBox;
    QLabel *roHeader;
    QLabel *roBody;
    QLabel *rwHeader;
    QLabel *rwBody;
    QLabel *deleteHeader;
    QLabel *deleteBody;
};

#endif // APPCONFIGPAGE_MCP_H
