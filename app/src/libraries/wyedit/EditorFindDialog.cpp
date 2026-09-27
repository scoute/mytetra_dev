#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QtGlobal>
#include <QtDebug>
#include <QShowEvent>

#include "Editor.h"
#include "EditorConfig.h"
#include "EditorFindDialog.h"


EditorFindDialog::EditorFindDialog(QWidget *parent) : QDialog(parent)
{
  setup_ui();
  setup_signals();
  assembly();

  QShowEvent event;
  EditorFindDialog::showEvent(&event);
}


void EditorFindDialog::setup_ui(void)
{
  lineEdit=new QLineEdit();
  lineEdit->setMinimumWidth(120);

  mathCase=new QCheckBox(tr("&Case sensitive"));
  wholeWords=new QCheckBox(tr("&Whole words only"));

  findButton=new QPushButton(tr("&Find"));
  findButton->setDefault(true);
  findButton->setEnabled(false);

  // Кнопки перехода между совпадениями задают направление явно стрелками
  prevButton=new QPushButton(QString::fromUtf8("\u25C0"));
  prevButton->setToolTip(tr("Previous match"));
  prevButton->setEnabled(false);

  nextButton=new QPushButton(QString::fromUtf8("\u25B6"));
  nextButton->setToolTip(tr("Next match"));
  nextButton->setEnabled(false);

  // Счетчик вида "2 of 5". Пустой пока нет активного поиска
  matchCounter=new QLabel();
  matchCounter->setMinimumWidth(60);

  // Кнопка ухода в глобальный поиск с текущим запросом.
  // Мост между поиском в заметке и поиском по базе
  inbaseButton=new QPushButton(tr("Find in base"));
  inbaseButton->setToolTip(tr("Search this text in the whole base"));
  inbaseButton->setEnabled(false);

  this->setWindowTitle(tr("Find in the text"));
}


void EditorFindDialog::setup_signals(void)
{
  connect(lineEdit, &QLineEdit::textChanged,
          this,     &EditorFindDialog::enable_find_button);

  // Живая подсветка: текст или опции изменились - подсветить, курсор не двигать
  connect(lineEdit, &QLineEdit::textChanged,
          this,     &EditorFindDialog::emit_highlight);

  connect(mathCase, &QCheckBox::toggled,
          this,     &EditorFindDialog::emit_highlight);

  connect(wholeWords, &QCheckBox::toggled,
           this,      &EditorFindDialog::emit_highlight);

  connect(findButton, &QPushButton::clicked,
          this,       &EditorFindDialog::find_clicked);

  connect(prevButton, &QPushButton::clicked,
          this,       &EditorFindDialog::prev_clicked);

  connect(nextButton, &QPushButton::clicked,
          this,       &EditorFindDialog::next_clicked);

  connect(inbaseButton, &QPushButton::clicked,
          this,         &EditorFindDialog::inbase_clicked);
}


void EditorFindDialog::assembly(void)
{
  QHBoxLayout *findLineLayout=new QHBoxLayout();
  findLineLayout->addWidget(lineEdit);
  findLineLayout->addWidget(findButton);
  findLineLayout->addWidget(prevButton);
  findLineLayout->addWidget(nextButton);
  findLineLayout->addWidget(matchCounter);
  findLineLayout->addWidget(inbaseButton);

  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addLayout(findLineLayout);
  centralLayout->addWidget(mathCase);
  centralLayout->addWidget(wholeWords);
  
  this->setLayout(centralLayout);

  this->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowCloseButtonHint);
}


// Действия при нажатии кнопки Find: подсветить все и перейти к следующему
void EditorFindDialog::find_clicked(void)
{
  emit find_text(lineEdit->text(), collectFlags());
}


// Переход к соседним совпадениям. Редактор сам разберется
// что делать если совпадений нет или запрос пуст
void EditorFindDialog::prev_clicked(void)
{
  emit find_previous();
}


void EditorFindDialog::next_clicked(void)
{
  emit find_next();
}


// Уход в глобальный поиск: запрос передается наружу как есть.
// Флаги не передаются: в глобальном поиске свои режимы (целые слова
// или подстрока), а регистр там всегда нечувствительный
void EditorFindDialog::inbase_clicked(void)
{
  emit find_in_base(lineEdit->text());
}


// Флаги поиска, собранные из состояния чекбоксов.
// Направление задают стрелки перехода, поэтому флага назад здесь нет:
// кнопка Find всегда идет вперед
QTextDocument::FindFlags EditorFindDialog::collectFlags(void) const
{
  QTextDocument::FindFlags flags=0;
  if(mathCase->isChecked())   flags|=QTextDocument::FindCaseSensitively;
  if(wholeWords->isChecked()) flags|=QTextDocument::FindWholeWords;

  return flags;
}


// Текст или опции изменились: попросить подсветить, курсор не двигать
void EditorFindDialog::emit_highlight(void)
{
  emit highlight_text(lineEdit->text(), collectFlags());
}


// Текущий текст поиска и флаги для обновления подсветки при показе диалога
QString EditorFindDialog::searchText(void) const
{
  return lineEdit->text();
}


QTextDocument::FindFlags EditorFindDialog::searchFlags(void) const
{
  return collectFlags();
}


// Установить текст и флаги извне. Изменение текста само обновляет
// подсветку через textChanged, двигать курсор не надо
void EditorFindDialog::setSearchText(const QString &text)
{
  lineEdit->setText(text);
}


void EditorFindDialog::setSearchFlags(QTextDocument::FindFlags flags)
{
  // Переключение чекбоксов само дает промежуточные пересчеты подсветки
  // через toggled. Это безвредно: итоговый пересчет даст setSearchText
  mathCase->setChecked(flags & QTextDocument::FindCaseSensitively);
  wholeWords->setChecked(flags & QTextDocument::FindWholeWords);
}


void EditorFindDialog::setMatchCounter(const QString &text)
{
  matchCounter->setText(text);
}


// Кнопки поиска, перехода и ухода в базу активны только тогда,
// когда есть текст для поиска
void EditorFindDialog::enable_find_button(const QString &text)
{
  bool enable=!text.isEmpty();

  findButton->setEnabled(enable);
  prevButton->setEnabled(enable);
  nextButton->setEnabled(enable);
  inbaseButton->setEnabled(enable);
}


void EditorFindDialog::hideEvent(QHideEvent *event)
{
  qDebug() << "Hide event of find dialog, window x " << this->x() << " y " << this->y() << " width " << this->width() << " height " << this->height();

  if( this->width()>0 && this->height()>0 && !(this->x()<=0 && this->y()<=0) )
  {
    // Получение ссылки в parent виджете на нужное поле
    EditorConfig *edConf=qobject_cast<Editor *>(parent())->editorConfig;

    // Запоминается геометрия
    QRect g=this->frameGeometry();
    qDebug() << "Frame geometry X " << g.x() << " Y " << g.y() << " W " << g.width() << " H" << g.height();
    QString gs=QString::number(g.x())+","+
               QString::number(g.y())+","+
               QString::number(g.width())+","+
               QString::number(g.height());
    edConf->set_finddialog_geometry(gs);
  }

  // Диалог скрыт: редактор должен снять подсветку совпадений
  emit find_dialog_hidden();

  QWidget::hideEvent(event);
}


void EditorFindDialog::showEvent(QShowEvent *event)
{
  qDebug() << "Show event of find dialog";

  lineEdit->setFocus();

  // Получение ссылки в parent виджете на нужное свойство
  EditorConfig *edConf=qobject_cast<Editor *>(parent())->editorConfig;

  QString geometry=edConf->get_finddialog_geometry();

  // Если была запомнена геометрия окна, устанавливается прежняя геометрия
  if(!geometry.isEmpty())
  {
    QStringList geometry_split=geometry.split(",");
    int x=geometry_split.at(0).toInt();
    int y=geometry_split.at(1).toInt();
    // int w=geometry_split.at(2).toInt();
    // int h=geometry_split.at(3).toInt();
    this->move(x,y);
  }
  else
    qDebug() << "Previos geometry of find dialog is not setted";

  QWidget::showEvent(event);
}

