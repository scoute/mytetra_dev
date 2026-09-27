#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QKeyEvent>

#include "Editor.h"
#include "EditorConfig.h"
#include "EditorFindBar.h"


EditorFindBar::EditorFindBar(QWidget *parent) : QWidget(parent)
{
  setup_ui();
  setup_signals();
  assembly();

  // Поле ввода само сообщает о спецклавишах через фильтр событий
  lineEdit->installEventFilter(this);
}


void EditorFindBar::setup_ui(void)
{
  lineEdit=new QLineEdit();
  lineEdit->setMinimumWidth(120);
  lineEdit->setClearButtonEnabled(true);
  lineEdit->setPlaceholderText(tr("Find in note"));

  mathCase=new QCheckBox(tr("&Case sensitive"));
  wholeWords=new QCheckBox(tr("&Whole words only"));

  findButton=new QPushButton(tr("&Find"));
  findButton->setDefault(true);
  findButton->setEnabled(false);

  // Кнопки перехода между совпадениями задают направление явно стрелками
  prevButton=new QPushButton(QString::fromUtf8("\u25C0"));
  prevButton->setToolTip(tr("Previous match (Shift+Enter)"));
  prevButton->setEnabled(false);

  nextButton=new QPushButton(QString::fromUtf8("\u25B6"));
  nextButton->setToolTip(tr("Next match (Enter)"));
  nextButton->setEnabled(false);

  // Счетчик вида "2 of 5". Пустой пока нет активного поиска
  matchCounter=new QLabel();
  matchCounter->setMinimumWidth(60);

  // Кнопка ухода в глобальный поиск с текущим запросом.
  // Мост между поиском в заметке и поиском по базе
  inbaseButton=new QPushButton(tr("Find in base"));
  inbaseButton->setToolTip(tr("Search this text in the whole base"));
  inbaseButton->setEnabled(false);

  closeButton=new QPushButton(QString::fromUtf8("\u2715"));
  closeButton->setToolTip(tr("Close find bar (Esc)"));
}


void EditorFindBar::setup_signals(void)
{
  connect(lineEdit, &QLineEdit::textChanged,
          this,     &EditorFindBar::enable_find_button);

  // Живая подсветка: текст или опции изменились - подсветить, курсор не двигать
  connect(lineEdit, &QLineEdit::textChanged,
          this,     &EditorFindBar::emit_highlight);

  connect(mathCase, &QCheckBox::toggled,
          this,     &EditorFindBar::emit_highlight);

  connect(wholeWords, &QCheckBox::toggled,
           this,      &EditorFindBar::emit_highlight);

  connect(findButton, &QPushButton::clicked,
          this,       &EditorFindBar::find_clicked);

  connect(prevButton, &QPushButton::clicked,
          this,       &EditorFindBar::prev_clicked);

  connect(nextButton, &QPushButton::clicked,
          this,       &EditorFindBar::next_clicked);

  connect(inbaseButton, &QPushButton::clicked,
          this,         &EditorFindBar::inbase_clicked);

  connect(closeButton, &QPushButton::clicked,
          this,        &EditorFindBar::close_clicked);
}


void EditorFindBar::assembly(void)
{
  // Все в один ряд: полоска живет между списком заметок и их содержимым
  // и не должна отъедать вертикальное место
  QHBoxLayout *centralLayout=new QHBoxLayout();
  centralLayout->setContentsMargins(2, 0, 2, 0);
  centralLayout->addWidget(lineEdit);
  centralLayout->addWidget(findButton);
  centralLayout->addWidget(prevButton);
  centralLayout->addWidget(nextButton);
  centralLayout->addWidget(matchCounter);
  centralLayout->addWidget(mathCase);
  centralLayout->addWidget(wholeWords);
  centralLayout->addWidget(inbaseButton);
  centralLayout->addWidget(closeButton);

  this->setLayout(centralLayout);
}


// Действия при нажатии кнопки Find: подсветить все и перейти к следующему
void EditorFindBar::find_clicked(void)
{
  emit find_text(lineEdit->text(), collectFlags());
}


// Переход к соседним совпадениям. Редактор сам разберется
// что делать если совпадений нет или запрос пуст
void EditorFindBar::prev_clicked(void)
{
  emit find_previous();
}


void EditorFindBar::next_clicked(void)
{
  emit find_next();
}


// Уход в глобальный поиск: запрос передается наружу как есть.
// Флаги не передаются: в глобальном поиске свои режимы (целые слова
// или подстрока), а регистр там всегда нечувствительный
void EditorFindBar::inbase_clicked(void)
{
  emit find_in_base(lineEdit->text());
}


// Крестик: спрятать полоску, редактор снимет подсветку по сигналу
void EditorFindBar::close_clicked(void)
{
  hideBar();
}


// Флаги поиска, собранные из состояния чекбоксов
QTextDocument::FindFlags EditorFindBar::collectFlags(void) const
{
  QTextDocument::FindFlags flags=0;
  if(mathCase->isChecked())   flags|=QTextDocument::FindCaseSensitively;
  if(wholeWords->isChecked()) flags|=QTextDocument::FindWholeWords;

  return flags;
}


// Текст или опции изменились: попросить подсветить, курсор не двигать
void EditorFindBar::emit_highlight(void)
{
  emit highlight_text(lineEdit->text(), collectFlags());
}


// Текущий текст поиска и флаги для обновления подсветки при показе полоски
QString EditorFindBar::searchText(void) const
{
  return lineEdit->text();
}


QTextDocument::FindFlags EditorFindBar::searchFlags(void) const
{
  return collectFlags();
}


// Установить текст и флаги извне. Изменение текста само обновляет
// подсветку через textChanged, двигать курсор не надо
void EditorFindBar::setSearchText(const QString &text)
{
  lineEdit->setText(text);
}


void EditorFindBar::setSearchFlags(QTextDocument::FindFlags flags)
{
  // Переключение чекбоксов само дает промежуточные пересчеты подсветки
  // через toggled. Это безвредно: итоговый пересчет даст setSearchText
  mathCase->setChecked(flags & QTextDocument::FindCaseSensitively);
  wholeWords->setChecked(flags & QTextDocument::FindWholeWords);
}


void EditorFindBar::setMatchCounter(const QString &text)
{
  matchCounter->setText(text);
}


void EditorFindBar::showBar(void)
{
  setVisible(true);
  lineEdit->setFocus();
  lineEdit->selectAll();
}


void EditorFindBar::hideBar(void)
{
  // Полоска спрятана: редактор должен снять подсветку совпадений
  setVisible(false);

  emit find_bar_hidden();
}


// Кнопки поиска, перехода и ухода в базу активны только тогда,
// когда есть текст для поиска
void EditorFindBar::enable_find_button(const QString &text)
{
  bool enable=!text.isEmpty();

  findButton->setEnabled(enable);
  prevButton->setEnabled(enable);
  nextButton->setEnabled(enable);
  inbaseButton->setEnabled(enable);
}


bool EditorFindBar::eventFilter(QObject *watched, QEvent *event)
{
  if(watched==lineEdit && event->type()==QEvent::KeyPress)
  {
    QKeyEvent *keyEvent=static_cast<QKeyEvent *>(event);

    // Enter ищет дальше, Shift+Enter ищет назад: повторный Ctrl+F
    // тоже сводится к переходу, см. Editor::onFindtextClicked
    if(keyEvent->key()==Qt::Key_Return || keyEvent->key()==Qt::Key_Enter)
    {
      if(keyEvent->modifiers() & Qt::ShiftModifier)
        emit find_previous();
      else
        emit find_next();

      return true;
    }

    // Esc прячет полоску
    if(keyEvent->key()==Qt::Key_Escape)
    {
      hideBar();

      return true;
    }
  }

  return QWidget::eventFilter(watched, event);
}
