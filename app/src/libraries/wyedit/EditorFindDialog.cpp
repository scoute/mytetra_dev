#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QStyle>
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

  replaceEdit=new QLineEdit();
  replaceEdit->setMinimumWidth(120);
  replaceEdit->setPlaceholderText(tr("Replace with"));

  mathCase=new QCheckBox(tr("&Case sensitive"));
  wholeWords=new QCheckBox(tr("&Whole words only"));
  loopSearch=new QCheckBox(tr("&Loop search"));
  loopSearch->setChecked(true);

  findButton=new QPushButton(tr("&Find"));
  findButton->setDefault(true);
  findButton->setEnabled(false);

  // Маленькие стрелки вперед и назад: иконки из стиля вместо глифов,
  // которых может не быть в шрифтах системы
  prevButton=new QPushButton();
  prevButton->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
  prevButton->setToolTip(tr("Find previous"));
  prevButton->setMaximumWidth(28);

  nextButton=new QPushButton();
  nextButton->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
  nextButton->setToolTip(tr("Find next"));
  nextButton->setMaximumWidth(28);

  replaceButton=new QPushButton(tr("&Replace"));
  replaceAllButton=new QPushButton(tr("Replace &all"));

  // Прикрепление полоской вместо окна: запрос и флаги переезжают с окном
  attachButton=new QPushButton();
  attachButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarShadeButton));
  attachButton->setToolTip(tr("Show as embedded bar"));
  attachButton->setMaximumWidth(28);

  this->setWindowTitle(tr("Find and replace"));
}


void EditorFindDialog::setup_signals(void)
{
  connect(lineEdit, &QLineEdit::textChanged,
          this,     &EditorFindDialog::enable_find_button);

  connect(findButton, &QPushButton::clicked,
          this,       &EditorFindDialog::find_clicked);

  connect(prevButton, &QPushButton::clicked,
          this,       &EditorFindDialog::find_prev_clicked);

  connect(nextButton, &QPushButton::clicked,
          this,       &EditorFindDialog::find_next_clicked);

  connect(replaceButton, &QPushButton::clicked,
          this,         &EditorFindDialog::replace_clicked);

  connect(replaceAllButton, &QPushButton::clicked,
          this,             &EditorFindDialog::replace_all_clicked);

  connect(attachButton, &QPushButton::clicked,
          this,        &EditorFindDialog::attach_clicked);
}


void EditorFindDialog::assembly(void)
{
  QHBoxLayout *findLineLayout=new QHBoxLayout();
  findLineLayout->addWidget(lineEdit);
  findLineLayout->addWidget(findButton);
  findLineLayout->addWidget(prevButton);
  findLineLayout->addWidget(nextButton);
  findLineLayout->addWidget(attachButton);

  QHBoxLayout *replaceLineLayout=new QHBoxLayout();
  replaceLineLayout->addWidget(replaceEdit);
  replaceLineLayout->addWidget(replaceButton);
  replaceLineLayout->addWidget(replaceAllButton);

  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addLayout(findLineLayout);
  centralLayout->addLayout(replaceLineLayout);
  centralLayout->addWidget(mathCase);
  centralLayout->addWidget(wholeWords);
  centralLayout->addWidget(loopSearch);
  
  this->setLayout(centralLayout);

  this->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowCloseButtonHint);
}


// Действия при нажатии кнопки Find и стрелки вперед
void EditorFindDialog::find_clicked(void)
{
  emit find_text(lineEdit->text(), collectFlags());
}


void EditorFindDialog::find_next_clicked(void)
{
  emit find_text(lineEdit->text(), collectFlags());
}


// Действия при нажатии стрелки назад
void EditorFindDialog::find_prev_clicked(void)
{
  QTextDocument::FindFlags flags=collectFlags();
  flags|=QTextDocument::FindBackward;

  emit find_prev_text(lineEdit->text(), flags);
}


// Действия при нажатии кнопки Replace
void EditorFindDialog::replace_clicked(void)
{
  emit replace_text(lineEdit->text(), replaceEdit->text(), collectFlags());
}


// Действия при нажатии кнопки Replace all
void EditorFindDialog::replace_all_clicked(void)
{
  emit replace_all_text(lineEdit->text(), replaceEdit->text(), collectFlags());
}


// Прикрепление: редактор перенесет запрос в полоску и спрячет окно
void EditorFindDialog::attach_clicked(void)
{
  emit attach_to_bar();
}


// Текст и флаги запроса для переноса между окном и полоской
QString EditorFindDialog::findRequestText(void) const
{
  return lineEdit->text();
}


QTextDocument::FindFlags EditorFindDialog::findRequestFlags(void) const
{
  return collectFlags();
}


void EditorFindDialog::setFindRequest(const QString &text, QTextDocument::FindFlags flags)
{
  lineEdit->setText(text);
  mathCase->setChecked(flags & QTextDocument::FindCaseSensitively);
  wholeWords->setChecked(flags & QTextDocument::FindWholeWords);
}


// Флаги поиска из состояния галочек.
// Направление задают кнопки (назад добавляет FindBackward сама)
QTextDocument::FindFlags EditorFindDialog::collectFlags(void) const
{
  QTextDocument::FindFlags flags;
  if(mathCase->isChecked())  flags|=QTextDocument::FindCaseSensitively;
  if(wholeWords->isChecked())flags|=QTextDocument::FindWholeWords;

  return flags;
}


// Зациклить ли поиск с другого конца документа
bool EditorFindDialog::isLoopSearch(void) const
{
  return loopSearch->isChecked();
}


// Кнопки действий активны только тогда, когда есть текст для поиска
void EditorFindDialog::enable_find_button(const QString &text)
{
  const bool hasText=!text.isEmpty();

  findButton->setEnabled(hasText);
  prevButton->setEnabled(hasText);
  nextButton->setEnabled(hasText);
  replaceButton->setEnabled(hasText);
  replaceAllButton->setEnabled(hasText);
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

