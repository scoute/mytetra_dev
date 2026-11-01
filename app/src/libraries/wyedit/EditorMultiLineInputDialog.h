#ifndef EDITORMULTILINEINPUTDIALOG_H
#define	EDITORMULTILINEINPUTDIALOG_H

#include <QDialog>
#include <QTextDocument>
#include <QTextOption>

class QWidget;
class QDialogButtonBox;
class QLineEdit;
class QCheckBox;
class QLabel;
class QKeyEvent;
class HtmlSourceEdit;

class EditorMultiLineInputDialog : public QDialog
{
    Q_OBJECT

public:
    EditorMultiLineInputDialog(QWidget *parent=nullptr);
    virtual ~EditorMultiLineInputDialog();

    void setText(QString text);
    QString getText();
    bool isModified(); // Выяснение, был ли изменен текст, показанный в диалоге

    void setWordWrapMode(QTextOption::WrapMode mode);
    void setSizeCoefficient(float f);

public slots:

    void setupShortcuts(void);

protected:
    // Esc при видимой панели поиска только прячет ее (не отменяет диалог)
    void keyPressEvent(QKeyEvent *event) override;

private slots:

    void showFindPanel(void);
    void showReplacePanel(void);
    void hideFindPanel(void);
    void onFindNext(void);
    void onFindPrevious(void);
    void onReplaceOne(void);
    void onReplaceAll(void);
    void onFindTextChanged(const QString &text);

private:

    float sizeCoefficient;
    HtmlSourceEdit *textArea;
    QDialogButtonBox *buttonBox;

    QWidget *findPanel;
    QLineEdit *findEdit;
    QLineEdit *replaceEdit;
    QCheckBox *caseCheck;
    QLabel *matchLabel;

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    void updateSize();

    QTextDocument::FindFlags currentFlags(void) const;
};

#endif // EDITORMULTILINEINPUTDIALOG_H
