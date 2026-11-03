#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QDebug>
#include <QPushButton>
#include <QWidget>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QShortcut>
#include <QKeyEvent>
#include <QTextDocument>
#include <QInputDialog>
#include <QPlainTextEdit>
#include <QSplitter>

#include "EditorMultiLineInputDialog.h"
#include "libraries/wyedit/HtmlSourceEdit.h"
#include "libraries/wyedit/HtmlTagTree.h"
#include "libraries/ShortcutManager.h"
extern ShortcutManager shortcutManager;

EditorMultiLineInputDialog::EditorMultiLineInputDialog(QWidget *parent) : QDialog(parent)
{
  sizeCoefficient=1.0;

  setupUi();
  setupShortcuts();
  setupSignals();
  assembly();
}

EditorMultiLineInputDialog::~EditorMultiLineInputDialog()
{
  if(textArea!=NULL)
    delete textArea;

  if(buttonBox!=NULL)
    delete buttonBox;
}

void EditorMultiLineInputDialog::setupUi()
{
  QSizePolicy sizePolicy;
  sizePolicy.setHorizontalPolicy(QSizePolicy::Expanding);
  sizePolicy.setVerticalPolicy(QSizePolicy::Expanding);

  // Исходник с подсветкой тегов, гуттером и движком поиска/замены
  textArea=new HtmlSourceEdit(this);
  textArea->setSizePolicy(sizePolicy);

  // Дерево тегов слева: структура документа, клик прыгает к тегу
  tagTree=new HtmlTagTree(this);
  tagTree->setSizePolicy(sizePolicy);
  tagTree->setEditor(textArea);

  buttonBox=new QDialogButtonBox(Qt::Horizontal);
  buttonBox->setOrientation(Qt::Horizontal);
  buttonBox->setStandardButtons(QDialogButtonBox::Ok|QDialogButtonBox::NoButton|QDialogButtonBox::Cancel);

  // На кнопку OK назначается комбинация клавиш Ctrl+Enter и она устанавливается как кнопка по умолчанию
  QPushButton *OkButton=buttonBox->button(QDialogButtonBox::Ok); // Выясняется указатель на кнопку OK
  OkButton->setAutoDefault(true);
  OkButton->setDefault(true);

  // С кнопки Cancel снимается флаг срабатывания по умолчанию,
  // это нужно для тулкитов основанных на GTK и прочих подобных библиотеках
  QPushButton *CancelButton=buttonBox->button(QDialogButtonBox::Cancel); // Выясняется указатель на кнопку Cancel
  CancelButton->setAutoDefault(false);
  CancelButton->setDefault(false);

  // Панель поиска и замены, скрыта пока не позовут Ctrl+F / Ctrl+H
  findPanel=new QWidget(this);
  findPanel->setVisible(false);

  QLabel *findLabel=new QLabel(tr("Find:"), findPanel);
  findEdit=new QLineEdit(findPanel);

  QPushButton *prevButton=new QPushButton(tr("Prev"), findPanel);
  QPushButton *nextButton=new QPushButton(tr("Next"), findPanel);

  QLabel *replaceLabel=new QLabel(tr("Replace:"), findPanel);
  replaceEdit=new QLineEdit(findPanel);

  QPushButton *replaceButton=new QPushButton(tr("Replace"), findPanel);
  QPushButton *replaceAllButton=new QPushButton(tr("All"), findPanel);

  caseCheck=new QCheckBox(tr("Aa"), findPanel);
  caseCheck->setToolTip(tr("Case sensitive"));

  matchLabel=new QLabel(findPanel);
  matchLabel->setMinimumWidth(110);

  QPushButton *closeFindButton=new QPushButton(tr("x"), findPanel);
  closeFindButton->setMaximumWidth(28);

  // Позиция курсора: обновляется движением по тексту
  posLabel=new QLabel(findPanel);
  posLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  posLabel->setMinimumWidth(110);

  QGridLayout *findLayout=new QGridLayout(findPanel);
  findLayout->setContentsMargins(0, 4, 0, 0);
  findLayout->addWidget(findLabel, 0, 0);
  findLayout->addWidget(findEdit, 0, 1);
  findLayout->addWidget(prevButton, 0, 2);
  findLayout->addWidget(nextButton, 0, 3);
  findLayout->addWidget(caseCheck, 0, 4);
  findLayout->addWidget(matchLabel, 0, 5);
  findLayout->addWidget(closeFindButton, 0, 6);
  findLayout->addWidget(replaceLabel, 1, 0);
  findLayout->addWidget(replaceEdit, 1, 1);
  findLayout->addWidget(replaceButton, 1, 2);
  findLayout->addWidget(replaceAllButton, 1, 3);
  findLayout->addWidget(posLabel, 1, 4, 1, 3);
  findLayout->setColumnStretch(1, 1);

  connect(prevButton, &QPushButton::clicked, this, &EditorMultiLineInputDialog::onFindPrevious);
  connect(nextButton, &QPushButton::clicked, this, &EditorMultiLineInputDialog::onFindNext);
  connect(replaceButton, &QPushButton::clicked, this, &EditorMultiLineInputDialog::onReplaceOne);
  connect(replaceAllButton, &QPushButton::clicked, this, &EditorMultiLineInputDialog::onReplaceAll);
  connect(closeFindButton, &QPushButton::clicked, this, &EditorMultiLineInputDialog::hideFindPanel);
  connect(findEdit, &QLineEdit::textChanged, this, &EditorMultiLineInputDialog::onFindTextChanged);
  connect(findEdit, &QLineEdit::returnPressed, this, &EditorMultiLineInputDialog::onFindNext);
  connect(replaceEdit, &QLineEdit::returnPressed, this, &EditorMultiLineInputDialog::onReplaceOne);
  connect(textArea, &QPlainTextEdit::cursorPositionChanged,
          this, &EditorMultiLineInputDialog::updateCursorPosition);
  updateCursorPosition();

  // F3 шагает по вхождениям, Shift+F3 - назад. Только при видимой
  // панели: иначе F3 уходит в глобальные хоткеи редактора
  QShortcut *findNextShortcut=new QShortcut(QKeySequence(Qt::Key_F3), this);
  findNextShortcut->setContext(Qt::WindowShortcut);
  connect(findNextShortcut, &QShortcut::activated, [&]()
  {
      if(findPanel->isVisible())
          onFindNext();
  });

  QShortcut *findPrevShortcut=new QShortcut(QKeySequence(Qt::ShiftModifier | Qt::Key_F3), this);
  findPrevShortcut->setContext(Qt::WindowShortcut);
  connect(findPrevShortcut, &QShortcut::activated, [&]()
  {
      if(findPanel->isVisible())
          onFindPrevious();
  });

  // Устанавливается размер окна, основанный на размере виджета, из которого
  // этот виджет был вызван
  if(this->parent()!=nullptr)
    if(this->parent()->isWidgetType())
        updateSize();
}

void EditorMultiLineInputDialog::setupShortcuts(void)
{
    qDebug() << "Setup shortcut for" << staticMetaObject.className();

    QPushButton *okButton=buttonBox->button(QDialogButtonBox::Ok); // Выясняется указатель на кнопку OK
    okButton->setShortcut( shortcutManager.getKeySequence("misc-editConfirm") ); // Устанавливается шорткат
    okButton->setToolTip(shortcutManager.getKeySequenceAsText("misc-editConfirm")); // ToolTip зависит от шортката

    // Поиск и замена по исходнику: диалог модальный, шорткаты
    // оконные, глобальные хоткеи редактора не задевают
    QShortcut *findShortcut=new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this);
    findShortcut->setContext(Qt::WindowShortcut);
    connect(findShortcut, &QShortcut::activated, this, &EditorMultiLineInputDialog::showFindPanel);

    QShortcut *replaceShortcut=new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_H), this);
    replaceShortcut->setContext(Qt::WindowShortcut);
    connect(replaceShortcut, &QShortcut::activated, this, &EditorMultiLineInputDialog::showReplacePanel);

    // Переход к строке: движок готов (goToLine), здесь только ввод номера
    QShortcut *gotoShortcut=new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_G), this);
    gotoShortcut->setContext(Qt::WindowShortcut);
    connect(gotoShortcut, &QShortcut::activated, this, &EditorMultiLineInputDialog::showGoToLine);
}

void EditorMultiLineInputDialog::setupSignals()
{
    connect(buttonBox, &QDialogButtonBox::accepted, this, &EditorMultiLineInputDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &EditorMultiLineInputDialog::reject);

    // Обновление горячих клавиш, если они были изменены
    connect(&shortcutManager, &ShortcutManager::updateWidgetShortcut, this, &EditorMultiLineInputDialog::setupShortcuts);
}

void EditorMultiLineInputDialog::assembly()
{
  QVBoxLayout *mainLayout=new QVBoxLayout(this);

  // Слева дерево тегов, справа исходник. Дерево сворачивается
  // разделителем, если структура не нужна
  QSplitter *splitter=new QSplitter(Qt::Horizontal, this);
  splitter->addWidget(tagTree);
  splitter->addWidget(textArea);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  splitter->setCollapsible(0, true);
  QList<int> sizes;
  sizes << 200 << 560;
  splitter->setSizes(sizes);

  // Добавляется область текста с деревом тегов
  mainLayout->addWidget(splitter, 1);

  // Панель поиска между текстом и кнопками
  mainLayout->addWidget(findPanel);

  // Добавляется линейка кнопок OK и Cancel
  mainLayout->addWidget(buttonBox);
}

void EditorMultiLineInputDialog::keyPressEvent(QKeyEvent *event)
{
  // Esc при видимой панели поиска только прячет ее, фокус
  // возвращается в текст. Диалог отменяется обычным путем
  if(event->key()==Qt::Key_Escape && findPanel->isVisible())
  {
      hideFindPanel();
      event->accept();

      return;
  }

  QDialog::keyPressEvent(event);
}

void EditorMultiLineInputDialog::showFindPanel(void)
{
  findPanel->setVisible(true);

  // Сид из выделения, как в основном редакторе: первая строка,
  // не длиннее поля. Только если поле пустое - набранное вручную
  // дороже выделенного
  if(findEdit->text().isEmpty())
  {
      const QString seed=textArea->textCursor().selectedText()
                         .replace(QChar::ParagraphSeparator, '\n')
                         .section('\n', 0, 0).trimmed().left(256);

      if(!seed.isEmpty())
          findEdit->setText(seed);
  }

  onFindTextChanged(findEdit->text());
  findEdit->setFocus();
  findEdit->selectAll();
}

void EditorMultiLineInputDialog::showReplacePanel(void)
{
  showFindPanel();
  replaceEdit->setFocus();
  replaceEdit->selectAll();
}

void EditorMultiLineInputDialog::showGoToLine(void)
{
  const QTextCursor cursor=textArea->textCursor();
  const int current=cursor.blockNumber()+1;
  const int last=textArea->document()->blockCount();

  bool ok=false;
  const int line=QInputDialog::getInt(this, tr("Go to line"), tr("Line:"),
                                      current, 1, qMax(1, last), 1, &ok);

  if(ok)
  {
      textArea->goToLine(line);
      textArea->setFocus();
  }
}

void EditorMultiLineInputDialog::updateCursorPosition(void)
{
  const QTextCursor cursor=textArea->textCursor();
  posLabel->setText(tr("Ln %1, Col %2").arg(cursor.blockNumber()+1).arg(cursor.columnNumber()+1));
}

void EditorMultiLineInputDialog::hideFindPanel(void)
{
  findPanel->setVisible(false);
  textArea->clearMatchHighlight();
  matchLabel->clear();
  textArea->setFocus();
}

void EditorMultiLineInputDialog::onFindTextChanged(const QString &text)
{
  if(text.isEmpty())
  {
      textArea->clearMatchHighlight();
      matchLabel->clear();

      return;
  }

  const int count=textArea->highlightMatches(text, currentFlags());
  matchLabel->setText(count>0 ? tr("%n match(es)", "", count) : tr("Not found"));
}

void EditorMultiLineInputDialog::onFindNext(void)
{
  if(findEdit->text().isEmpty())
      return;

  if(!textArea->findNext(findEdit->text(), currentFlags()))
      matchLabel->setText(tr("Not found"));
}

void EditorMultiLineInputDialog::onFindPrevious(void)
{
  if(findEdit->text().isEmpty())
      return;

  if(!textArea->findPrevious(findEdit->text(), currentFlags()))
      matchLabel->setText(tr("Not found"));
}

void EditorMultiLineInputDialog::onReplaceOne(void)
{
  if(findEdit->text().isEmpty())
      return;

  textArea->replaceCurrent(findEdit->text(), replaceEdit->text(), currentFlags());
  onFindTextChanged(findEdit->text());
  findEdit->setFocus();
}

void EditorMultiLineInputDialog::onReplaceAll(void)
{
  if(findEdit->text().isEmpty())
      return;

  const int count=textArea->replaceAll(findEdit->text(), replaceEdit->text(), currentFlags());
  matchLabel->setText(tr("Replaced %n time(s)", "", count));
  onFindTextChanged(findEdit->text());
  findEdit->setFocus();
}

QTextDocument::FindFlags EditorMultiLineInputDialog::currentFlags(void) const
{
  if(caseCheck->isChecked())
      return QTextDocument::FindCaseSensitively;

  return QTextDocument::FindFlags();
}

void EditorMultiLineInputDialog::setText(QString text)
{
  textArea->setPlainText(text);
}

QString EditorMultiLineInputDialog::getText()
{
  return textArea->toPlainText();
}

// Выясняется, был ли изменен текст, показанный в диалоге
bool EditorMultiLineInputDialog::isModified()
{
  return textArea->document()->isModified();
}

void EditorMultiLineInputDialog::setWordWrapMode(QTextOption::WrapMode mode)
{
  // Перенос по умолчанию как раньше у QTextEdit: все кроме NoWrap
  // тянется по ширине виджета
  if(mode==QTextOption::NoWrap)
      textArea->setLineWrapMode(QPlainTextEdit::NoWrap);
  else
      textArea->setLineWrapMode(QPlainTextEdit::WidgetWidth);
}

void EditorMultiLineInputDialog::setSizeCoefficient(float f)
{
  sizeCoefficient=f;

  updateSize();
}

void EditorMultiLineInputDialog::updateSize()
{
  QWidget *parentWidget=qobject_cast<QWidget *>(this->parent());
  QRect geom(parentWidget->pos(), parentWidget->size());

  geom.setHeight( (int)((float)geom.height()*sizeCoefficient) );
  geom.setWidth ( (int)((float)geom.width()*sizeCoefficient) );

  qDebug() << "Parent window geometry " << geom.x() << geom.y() << geom.width() << geom.height();

  setMinimumSize(geom.width(), geom.height());
}
