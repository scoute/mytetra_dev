#ifndef _CHANGEHISTORYDIALOG_H_
#define _CHANGEHISTORYDIALOG_H_

#include <QDialog>

#include "libraries/ChangeHistory.h"

class QListWidget;
class QTextBrowser;
class QPushButton;
class QLabel;

// История изменений заметки: список версий (дата и тема),
// раскрашенный diff версии против текущего текста,
// откат возвращает текст версии вызывателю

class ChangeHistoryDialog : public QDialog
{
    Q_OBJECT

public:

    explicit ChangeHistoryDialog(const QString &tetradir,
                                 const QString &recordId,
                                 const QString &recordName,
                                 const QString &currentHtml,
                                 QWidget *parent=nullptr);

    // Текст выбранной версии, валиден при Accepted
    QString rollbackContent(void) const;

    // Есть ли хоть одна версия
    bool hasHistory(void) const;

private slots:

    void onVersionSelected(int row);
    void onRollback(void);

private:

    // Пересчитать diff для текущей строки
    void updateDiff(void);

    QString tetradir;
    QString recordId;
    QString recordName;
    QString currentHtml;

    QList<ChangeHistory::Version> versionList;
    QString selectedContent;

    QListWidget *versionsList;
    QTextBrowser *diffView;
    QLabel *statusLabel;
    QPushButton *rollbackButton;
    QPushButton *closeButton;
};

#endif /* _CHANGEHISTORYDIALOG_H_ */
