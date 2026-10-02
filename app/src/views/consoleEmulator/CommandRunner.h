#ifndef COMMANDRUNNER_H
#define	COMMANDRUNNER_H

#include <QObject>
#include <QProcess>


class ConsoleEmulator;
class QTextCodec;

class CommandRunner : public QObject
{
    Q_OBJECT

public:
    CommandRunner(QObject *parent=nullptr);
    virtual ~CommandRunner();

    static QString getOsFamily();

    void setCommand(QString cmd);
    void run(bool visible=true);
    int runSimple();
    QString runSimpleAndGetOutput();
    bool isRun();

    // Тихий синхронный запуск: вывод перехватывается и отбрасывается,
    // консоль не создаётся, возвращается код выхода.
    // Для служебных вызовов (git журнала обмена), чей вывод пользователю
    // видеть не нужно и нельзя (иначе внутренности git сыплются в консоль)
    int runSimpleQuiet();

    void setWindowTitle(QString title);
    void setMessageText(QString text);

signals:

    // Оповещение других объектов MyTetra о том что работа выполняемой команды закончилась
    // Должно вызываться при закрытии окна консоли
    void finishWork();

private slots:

    void onManualCloseProcess(void);
    void onProcessError(QProcess::ProcessError error);

    void onReadyReadStandardOutput();
    void onProcessFinish(int exitCode, QProcess::ExitStatus exitStatus);

private:

    QString getCommandForProcessExecute();
    void printOutput() const;

    void createProcessAndConsole(void);
    void removeProcessAndConsole(void);


    QString m_command;
    QString m_shell;
    QString m_windowTitle;
    QString m_messageText;

    QProcess *m_process = nullptr;
    QTextCodec *m_outputCodec = nullptr;

    ConsoleEmulator *m_console = nullptr;

    bool m_isError = false;

    bool m_isRun = false;

};

#endif	/* COMMANDRUNNER_H */

