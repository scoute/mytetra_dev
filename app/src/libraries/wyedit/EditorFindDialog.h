#ifndef _EDITORFINDDIALOG_H_
#define	_EDITORFINDDIALOG_H_

#include <QWidget>
#include <QDialog>
#include <QTextDocument>


class QCheckBox;
class QLineEdit;
class QPushButton;
class QLabel;


class EditorFindDialog : public QDialog
{
 Q_OBJECT

public:
 EditorFindDialog(QWidget *parent=nullptr);

 // Текущий текст поиска и флаги, собранные из чекбоксов.
 // Нужны редактору чтобы обновить подсветку при показе диалога
 QString searchText(void) const;
 QTextDocument::FindFlags searchFlags(void) const;

 // Установить текст и флаги извне (для моста из глобального поиска)
 void setSearchText(const QString &text);
 void setSearchFlags(QTextDocument::FindFlags flags);

 // Показать счетчик вида "2 of 5". Пустая строка гасит надпись
 void setMatchCounter(const QString &text);
 
signals:
 void find_text(const QString &text, QTextDocument::FindFlags flags);
 void find_previous(void);
 void find_next(void);

 // Изменился текст или опции: подсветку надо обновить, курсор не двигать
 void highlight_text(const QString &text, QTextDocument::FindFlags flags);

 // Диалог скрыт: подсветку надо снять
 void find_dialog_hidden(void);

 // Кнопка поиска по базе: запрос уходит наружу вместе с текстом
 void find_in_base(const QString &text);

private slots:
 void find_clicked(void);
 void prev_clicked(void);
 void next_clicked(void);
 void inbase_clicked(void);
 void enable_find_button(const QString &text);
 void emit_highlight(void);
 
private:
 QLineEdit *lineEdit;
 QCheckBox *mathCase;
 QCheckBox *wholeWords;
 QCheckBox *searchBackward;
 QPushButton *findButton;
 QPushButton *prevButton; // Перейти к предыдущему совпадению
 QPushButton *nextButton; // Перейти к следующему совпадению
 QPushButton *inbaseButton; // Искать этот запрос по всей базе
 QLabel *matchCounter; // Счетчик вида "2 of 5"

 // Флаги поиска, собранные из состояния чекбоксов
 QTextDocument::FindFlags collectFlags(void) const;
 
 void setup_ui(void);
 void setup_signals(void);
 void assembly(void);

 void hideEvent(QHideEvent *event);
 void showEvent(QShowEvent *event);
};

#endif	/* _EDITORFINDDIALOG_H_ */

