#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QStyle>

#include "Editor.h"
#include "EditorConfig.h"
#include "EditorFindBar.h"


EditorFindBar::EditorFindBar(QWidget *parent) : QWidget(parent)
{
  setup_ui();
  setup_signals();
  assembly();

  // Поля ввода сами сообщают о спецклавишах через фильтр событий
  lineEdit->installEventFilter(this);
  replaceEdit->installEventFilter(this);
}


void EditorFindBar::setup_ui(void)
{
  lineEdit=new QLineEdit();
  lineEdit->setMinimumWidth(120);
  lineEdit->setClearButtonEnabled(true);
  lineEdit->setPlaceholderText(tr("Find in note"));

  // Опции компактными значками вместо длинных подписей: полоска
  // не должна расталкивать соседние окна на маленьких ноутбуках.
  // Полный текст живёт в подсказках. Мнемоник Alt+C/Alt+W больше нет
  mathCase=new QCheckBox(tr("Aa"));
  mathCase->setToolTip(tr("Match case"));
  wholeWords=new QCheckBox(tr("\"ab\""));
  wholeWords->setToolTip(tr("Whole words only"));

  // Поле замены живет в той же строке полоски: отдельное окно не нужно.
  // Пустое поле означает удаление совпадения
  replaceEdit=new QLineEdit();
  replaceEdit->setMinimumWidth(120);
  replaceEdit->setClearButtonEnabled(true);
  replaceEdit->setPlaceholderText(tr("Replace with"));

  replaceButton=new QPushButton(tr("&Replace"));
  replaceButton->setEnabled(false);

  replaceAllButton=new QPushButton(tr("Replace &all"));
  replaceAllButton->setEnabled(false);

  findButton=new QPushButton(tr("&Find"));
  findButton->setDefault(true);
  findButton->setEnabled(false);

  // Текстовые кнопки не растягиваются: все свободное место строки
  // забирают поля ввода через stretch в assembly. Иначе стиль
  // (особенно Windows) раздувает кнопки, а поля жмутся к минимуму.
  // Горизонтальные отступы ужаты до минимума: кнопке хватает ширины
  // слова плюс пара пикселей. Сами отступы и рамки живут в CSS тем
  // (селектор EditorFindBar QPushButton), а не в инлайн-стиле: тогда
  // светлая тема рисует кнопки как раньше, а темная добавляет белую
  // обводку 1px с компенсацией отступов, и размер кнопок не меняется.
  // Замер под Fusion: "Найти" 80 -> 64 пикселя при тексте 53 пикселя,
  // высота кнопок как у полей ввода.
  findButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  replaceButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  replaceAllButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

  // Кнопки перехода между совпадениями рисуются значками активного
  // стиля (стрелки назад/вперед), а не текстовыми глифами. Глифы
  // треугольников отсутствуют в шрифте Windows, их подменяет
  // посторонний шрифт с мелкими метриками - стрелки выходят
  // маленькими и странными. Значок стиля рисуется одинаково везде.
  // Кнопки-стрелки и крестик квадратные: ширина равна высоте, иначе стиль
  // (особенно Windows) растягивает их до ширины текстовых кнопок.
  // Размер ставится не здесь, а в fixSquareButtons при показе: высота
  // самой кнопки зависит от стиля и шрифта платформы (на windowsvista
  // она в разы больше), а эталоном служит высота поля ввода.
  // ObjectName выделяет их в CSS: в темной теме у них свой отступ,
  // скомпенсированный под белую обводку, чтобы квадрат не разъехался
  prevButton=new QPushButton(this);
  prevButton->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
  prevButton->setObjectName("findbarSquareButton");
  prevButton->setToolTip(tr("Previous match (Shift+Enter)"));
  prevButton->setEnabled(false);

  nextButton=new QPushButton(this);
  nextButton->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
  nextButton->setObjectName("findbarSquareButton");
  nextButton->setToolTip(tr("Next match (Enter)"));
  nextButton->setEnabled(false);

  // Счетчик вида "2 of 5". Пустой пока нет активного поиска.
  // Ширина не резервируется: пустой счетчик не должен оставлять
  // промежуток между стрелками и чекбоксами, текст раздвигает
  // полоску по факту появления
  matchCounter=new QLabel();
  matchCounter->setMinimumWidth(0);

  // Кнопка ухода в глобальный поиск с текущим запросом.
  // Мост между поиском в заметке и поиском по базе
  inbaseButton=new QPushButton(tr("Find in base"));
  inbaseButton->setToolTip(tr("Search this text in the whole base"));
  inbaseButton->setEnabled(false);
  inbaseButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

  closeButton=new QPushButton(this);
  closeButton->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
  closeButton->setObjectName("findbarSquareButton");
  closeButton->setToolTip(tr("Close find bar (Esc)"));

  // Открепление в отдельное окно: запрос и флаги переезжают с полоской
  detachButton=new QPushButton(this);
  detachButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
  detachButton->setObjectName("findbarSquareButton");
  detachButton->setToolTip(tr("Open as separate window"));
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

  connect(replaceButton, &QPushButton::clicked,
          this,          &EditorFindBar::replace_clicked);

  connect(replaceAllButton, &QPushButton::clicked,
          this,             &EditorFindBar::replace_all_clicked);

  connect(inbaseButton, &QPushButton::clicked,
          this,         &EditorFindBar::inbase_clicked);

  connect(closeButton, &QPushButton::clicked,
          this,        &EditorFindBar::close_clicked);

  connect(detachButton, &QPushButton::clicked,
          this,         &EditorFindBar::detach_clicked);
}


void EditorFindBar::assembly(void)
{
  // Все в один ряд: полоска живет между списком заметок и их содержимым
  // и не должна отъедать вертикальное место. Поля ввода с растяжением
  // забирают все свободное место, кнопки остаются компактными.
  // Промежутки между виджетами убраны полностью: пустое место между
  // стрелками, счетчиком и галочками на узких экранах ни к чему,
  // у кнопок и полей свои внутренние отступы
  QHBoxLayout *centralLayout=new QHBoxLayout();
  centralLayout->setContentsMargins(2, 0, 2, 0);
  centralLayout->setSpacing(0);
  centralLayout->addWidget(lineEdit, 1);
  centralLayout->addWidget(findButton);
  centralLayout->addWidget(prevButton);
  centralLayout->addWidget(nextButton);
  centralLayout->addWidget(matchCounter);
  centralLayout->addWidget(mathCase);
  centralLayout->addWidget(wholeWords);
  centralLayout->addWidget(replaceEdit, 1);
  centralLayout->addWidget(replaceButton);
  centralLayout->addWidget(replaceAllButton);
  centralLayout->addWidget(inbaseButton);
  centralLayout->addWidget(closeButton);
  centralLayout->addWidget(detachButton);

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


// Замена текущего совпадения или всех совпадений.
// Редактор сам разберется что делать если совпадений нет
void EditorFindBar::replace_clicked(void)
{
  emit replace_one(lineEdit->text(), replaceEdit->text(), collectFlags());
}


void EditorFindBar::replace_all_clicked(void)
{
  emit replace_all(lineEdit->text(), replaceEdit->text(), collectFlags());
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


// Открепление: редактор перенесет запрос в окно и спрячет полоску
void EditorFindBar::detach_clicked(void)
{
  emit detach_to_window();
}


// Флаги поиска, собранные из состояния чекбоксов
QTextDocument::FindFlags EditorFindBar::collectFlags(void) const
{
  QTextDocument::FindFlags flags;
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
  fixSquareButtons();

  setVisible(true);
  lineEdit->setFocus();
  lineEdit->selectAll();
}


// Квадратные кнопки в размер поля ввода. Вызывается при каждом показе:
// sizeHint поля посчитан под текущий стиль и шрифт, а собственная
// высота кнопки на чужой платформе непредсказуема (на windowsvista
// в разы больше). Текущий height() брать нельзя: до раскладки он мусор.
// Значки масштабируются под кнопку: слишком крупный значок стиль
// обрежет, поэтому оставляется запас на внутренние отступы
void EditorFindBar::fixSquareButtons(void)
{
  int side=lineEdit->sizeHint().height();

  if(side<=0)
    return;

  prevButton->setFixedSize(side, side);
  nextButton->setFixedSize(side, side);
  closeButton->setFixedSize(side, side);
  detachButton->setFixedSize(side, side);

  int iconSide=qMax(side-8, 12);
  prevButton->setIconSize(QSize(iconSide, iconSide));
  nextButton->setIconSize(QSize(iconSide, iconSide));
  closeButton->setIconSize(QSize(iconSide, iconSide));
  detachButton->setIconSize(QSize(iconSide, iconSide));
}


void EditorFindBar::hideBar(void)
{
  // Полоска спрятана: редактор должен снять подсветку совпадений
  setVisible(false);

  emit find_bar_hidden();
}


// Кнопки поиска, перехода, замены и ухода в базу активны только тогда,
// когда есть текст для поиска. Текст замены при этом может быть пустым:
// пустая замена означает удаление совпадения
void EditorFindBar::enable_find_button(const QString &text)
{
  bool enable=!text.isEmpty();

  findButton->setEnabled(enable);
  prevButton->setEnabled(enable);
  nextButton->setEnabled(enable);
  replaceButton->setEnabled(enable);
  replaceAllButton->setEnabled(enable);
  inbaseButton->setEnabled(enable);
}


bool EditorFindBar::eventFilter(QObject *watched, QEvent *event)
{
  if(event->type()==QEvent::KeyPress &&
     (watched==lineEdit || watched==replaceEdit))
  {
    QKeyEvent *keyEvent=static_cast<QKeyEvent *>(event);

    // Enter в поле поиска ищет дальше, в поле замены заменяет текущее
    if(keyEvent->key()==Qt::Key_Return || keyEvent->key()==Qt::Key_Enter)
    {
      if(watched==replaceEdit)
      {
        emit replace_one(lineEdit->text(), replaceEdit->text(), collectFlags());
      }
      else if(keyEvent->modifiers() & Qt::ShiftModifier)
      {
        emit find_previous();
      }
      else
      {
        emit find_next();
      }

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
