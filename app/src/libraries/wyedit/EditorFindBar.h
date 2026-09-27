#ifndef _EDITORFINDBAR_H_
#define _EDITORFINDBAR_H_

#include <QWidget>
#include <QTextDocument>


class QCheckBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QKeyEvent;


// Встраиваемая полоска поиска в заметке. Живет в layout редактора между
// панелью инструментов и текстом, показывается и прячется по Ctrl+F.
// Замена отдельному окну EditorFindDialog: ни одного нового окна,
// повторный Ctrl+F ищет дальше, Esc прячет полоску
class EditorFindBar : public QWidget
{
 Q_OBJECT

public:
 EditorFindBar(QWidget *parent=nullptr);

 // Текущий текст поиска и флаги, собранные из чекбоксов
 QString searchText(void) const;
 QTextDocument::FindFlags searchFlags(void) const;

 // Установить текст и флаги извне (для моста из глобального поиска)
 void setSearchText(const QString &text);
 void setSearchFlags(QTextDocument::FindFlags flags);

 // Показать счетчик вида "2 of 5". Пустая строка гасит надпись
 void setMatchCounter(const QString &text);

 // Показать полоску: фокус в поле ввода, текст выделяется
 void showBar(void);

 // Спрятать полоску. Редактор по сигналу снимает подсветку
 void hideBar(void);

signals:
 void find_text(const QString &text, QTextDocument::FindFlags flags);
 void find_previous(void);
 void find_next(void);

 // Изменился текст или опции: подсветку надо обновить, курсор не двигать
 void highlight_text(const QString &text, QTextDocument::FindFlags flags);

 // Полоска спрятана: подсветку надо снять
 void find_bar_hidden(void);

  // Кнопка поиска по базе: запрос уходит наружу вместе с текстом
  void find_in_base(const QString &text);

  // Замена текущего совпадения или всех совпадений в заметке.
  // Флаги те же что у поиска: регистр и целые слова
  void replace_one(const QString &text, const QString &replacement, QTextDocument::FindFlags flags);
  void replace_all(const QString &text, const QString &replacement, QTextDocument::FindFlags flags);

protected:
  // Перехват клавиш в полях ввода: Enter в поиске ищет дальше,
  // Shift+Enter ищет назад, Enter в замене заменяет текущее,
  // Esc прячет полоску. QLineEdit сам их не обрабатывает как нам надо
  bool eventFilter(QObject *watched, QEvent *event);

private slots:
  void find_clicked(void);
  void prev_clicked(void);
  void next_clicked(void);
  void replace_clicked(void);
  void replace_all_clicked(void);
  void inbase_clicked(void);
 void close_clicked(void);
 void enable_find_button(const QString &text);
 void emit_highlight(void);

private:
  QLineEdit *lineEdit;
  QCheckBox *mathCase;
  QCheckBox *wholeWords;
  QLineEdit *replaceEdit; // Текст замены, может быть пустым (удаление)
  QPushButton *findButton;
  QPushButton *prevButton; // Перейти к предыдущему совпадению
  QPushButton *nextButton; // Перейти к следующему совпадению
  QPushButton *replaceButton; // Заменить текущее совпадение
  QPushButton *replaceAllButton; // Заменить все совпадения в заметке
 QPushButton *inbaseButton; // Искать этот запрос по всей базе
 QPushButton *closeButton; // Спрятать полоску
 QLabel *matchCounter; // Счетчик вида "2 of 5"

 // Флаги поиска, собранные из состояния чекбоксов.
 // Направление задают стрелки и Enter, кнопки Find всегда вперед
 QTextDocument::FindFlags collectFlags(void) const;

 void setup_ui(void);
 void setup_signals(void);
 void assembly(void);
};

#endif /* _EDITORFINDBAR_H_ */
