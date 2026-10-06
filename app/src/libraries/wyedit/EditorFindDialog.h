#ifndef _EDITORFINDDIALOG_H_
#define	_EDITORFINDDIALOG_H_

#include <QWidget>
#include <QDialog>
#include <QTextDocument>


class QCheckBox;
class QLineEdit;
class QPushButton;


class EditorFindDialog : public QDialog
{
 Q_OBJECT

public:
 EditorFindDialog(QWidget *parent=nullptr);
 
signals:
 void find_text(const QString &text, QTextDocument::FindFlags flags);
 void replace_text(const QString &text, const QString &replaceText, QTextDocument::FindFlags flags);
 void replace_all_text(const QString &text, const QString &replaceText, QTextDocument::FindFlags flags);

private slots:
 void find_clicked(void);
 void replace_clicked(void);
 void replace_all_clicked(void);
 void enable_find_button(const QString &text);

private:
 QTextDocument::FindFlags collectFlags(void) const;
 
private:
 QLineEdit *lineEdit;
 QLineEdit *replaceEdit;
 QCheckBox *mathCase;
 QCheckBox *wholeWords;
 QCheckBox *searchBackward;
 QPushButton *findButton;
 QPushButton *replaceButton;
 QPushButton *replaceAllButton;
 
 void setup_ui(void);
 void setup_signals(void);
 void assembly(void);

 void hideEvent(QHideEvent *event);
 void showEvent(QShowEvent *event);
};

#endif	/* _EDITORFINDDIALOG_H_ */

