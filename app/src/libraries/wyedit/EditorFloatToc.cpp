#include <QTimer>
#include <QListWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollBar>

#include "EditorFloatToc.h"
#include "EditorTextArea.h"


EditorFloatToc::EditorFloatToc(QWidget *parent) : QWidget(parent)
{
 setObjectName("floatToc");
 setVisible(false); // Показывается первым rebuild при наличии заголовков
 setFixedWidth(232);

 // Без этого флага QSS-фон с альфой заливается глухо:
 // виджет должен просвечивать, а не перекрывать текст
 setAttribute(Qt::WA_TranslucentBackground);

 // Полупрозрачный фон рисуется вручную в paintEvent: QSS-фон
 // с альфой на дочернем виджете вьюпорта ложится через раз.
 // Цвета нейтральные, читаются и на светлой, и на темной теме
 setStyleSheet(QStringLiteral(
  "#floatToc QListWidget { background: transparent; border: none;"
  " color: #E8E8E8; outline: none; }"
  "#floatToc QListWidget::item:selected { background: rgba(255, 255, 255, 45);"
  " border-radius: 3px; }"
  "#floatToc QToolButton { background: transparent; border: none;"
  " color: #E8E8E8; font-weight: bold; }"));

 m_headerButton=new QToolButton(this);
 m_headerButton->setText(tr("Contents"));
 m_headerButton->setArrowType(Qt::DownArrow);
 m_headerButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
 m_headerButton->setCheckable(false);
 m_headerButton->setCursor(Qt::PointingHandCursor);

 m_list=new QListWidget(this);
 m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
 m_list->setUniformItemSizes(true);
 m_list->setFocusPolicy(Qt::NoFocus);

 QVBoxLayout *layout=new QVBoxLayout(this);
 layout->setContentsMargins(6, 4, 6, 4);
 layout->setSpacing(2);
 layout->addWidget(m_headerButton);
 layout->addWidget(m_list, 1);

 m_timer=new QTimer(this);
 m_timer->setSingleShot(true);
 m_timer->setInterval(300); // Дебаунс: перестройка не на каждую клавишу
 connect(m_timer, &QTimer::timeout, this, &EditorFloatToc::onRebuildTimeout);

 connect(m_list, &QListWidget::itemClicked, this, &EditorFloatToc::onJump);
 connect(m_headerButton, &QToolButton::clicked, this, &EditorFloatToc::onToggleCollapsed);
}


EditorFloatToc::~EditorFloatToc()
{

}


void EditorFloatToc::setTextArea(EditorTextArea *area)
{
 m_area=area;

 if(m_area==nullptr)
  return;

 connect(m_area->document(), &QTextDocument::contentsChanged,
         this,                &EditorFloatToc::onDocumentChanged);
 connect(m_area, &QTextEdit::cursorPositionChanged,
         this,    &EditorFloatToc::onCursorMoved);

 // Ресайз и редактора, и вьюпорта: геометрия вьюпорта (а с ней
 // и привязка оверлея) меняется и при появлении скроллбаров.
 // Диапазоны скроллбаров — на случай асинхронной раскладки
 // документа: диапазоны встают позже показа, ресайза вьюпорта
 // при этом может уже не быть
 m_area->installEventFilter(this);

 if(m_area->viewport()!=nullptr)
  m_area->viewport()->installEventFilter(this);

 connect(m_area->verticalScrollBar(), &QScrollBar::rangeChanged,
         this,                         &EditorFloatToc::onBarsRangeChanged);
 connect(m_area->horizontalScrollBar(), &QScrollBar::rangeChanged,
         this,                           &EditorFloatToc::onBarsRangeChanged);

 // Появление/скрытие скроллбара меняет геометрию вьюпорта,
 // но ресайз вьюпорта при этом не всегда доходит: ловим
 // показ/скрытие самих полос напрямую
 m_area->verticalScrollBar()->installEventFilter(this);
 m_area->horizontalScrollBar()->installEventFilter(this);

 rebuild();
}


bool EditorFloatToc::eventFilter(QObject *watched, QEvent *event)
{
 if(m_area==nullptr)
  return QWidget::eventFilter(watched, event);

 if((watched==m_area || watched==m_area->viewport()) &&
    event->type()==QEvent::Resize)
  place();

 // Появление/скрытие полос меняет геометрию вьюпорта,
 // а ресайз вьюпорта при этом не всегда доходит
 if((watched==m_area->verticalScrollBar() ||
     watched==m_area->horizontalScrollBar()) &&
    (event->type()==QEvent::Show || event->type()==QEvent::Hide))
  place();

 return QWidget::eventFilter(watched, event);
}


// Оверлей прижат к правому верху вьюпорта с отступом.
// Координаты — в системе редактора (оверлей его потомок),
// за базу берется геометрия вьюпорта
// Высота ограничена вьюпортом: длинный список уезжает в свой скролл
void EditorFloatToc::place(void)
{
 if(m_area==nullptr || m_area->viewport()==nullptr)
  return;

 const int margin=8;
 QWidget *viewport=m_area->viewport();
 const int viewportWidth=viewport->width();
 const int viewportHeight=viewport->height();

 setMaximumHeight(qMax(120, viewportHeight-margin*2-16));
 adjustSize();

 const int w=width();
 move(viewport->pos().x()+viewportWidth-w-margin,
      viewport->pos().y()+margin);
}


int EditorFloatToc::count(void) const
{
 return m_list ? m_list->count() : 0;
}


QListWidgetItem *EditorFloatToc::item(int row) const
{
 return m_list ? m_list->item(row) : nullptr;
}


int EditorFloatToc::currentRow(void) const
{
 return m_list ? m_list->currentRow() : -1;
}


void EditorFloatToc::rebuild(void)
{
 if(m_area==nullptr || m_list==nullptr)
  return;

 m_sync=true;
 m_list->clear();

 QTextDocument *doc=m_area->document();
 for(QTextBlock block=doc->begin(); block.isValid(); block=block.next())
 {
  const int level=block.blockFormat().headingLevel();
  if(level<1 || level>3)
   continue;

  QString text=block.text().simplified();
  if(text.isEmpty())
   text=tr("(empty heading)");
  if(text.length()>100)
   text=text.left(100)+QStringLiteral("…");

  // Вложенность видна отступом: H1 без отступа, H2 и H3 глубже
  QListWidgetItem *item=new QListWidgetItem(QString((level-1)*2, QLatin1Char(' '))+text, m_list);
  item->setData(Qt::UserRole, block.position());
  item->setToolTip(block.text().simplified());
 }

 // Показ только при наличии заголовков. Свернутость переживает
 // перестройку: прячется/показывается только список.
 // Высота списка — ровно по строкам (лишней пустоты нет),
 // длинные списки уезжают в свой скролл
 const int rowCount=m_list->count();
 const int rowHeight=rowCount>0 ? m_list->sizeHintForRow(0) : 24;
 m_list->setFixedHeight(qMin(rowCount*rowHeight+4, 320));
 m_list->setVisible(!m_collapsed);
 setVisible(rowCount>0);
 place();

 onCursorMoved();
 m_sync=false;
}


void EditorFloatToc::onDocumentChanged(void)
{
 m_timer->start();
}


void EditorFloatToc::paintEvent(QPaintEvent *event)
{
 Q_UNUSED(event);

 QPainter painter(this);
 painter.setRenderHint(QPainter::Antialiasing, true);
 painter.setPen(QPen(QColor(255, 255, 255, 70), 1));
 painter.setBrush(QColor(32, 36, 44, 215));
 painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);
}


void EditorFloatToc::onRebuildTimeout(void)
{
 rebuild();
}


void EditorFloatToc::onBarsRangeChanged(int minimum, int maximum)
{
 Q_UNUSED(minimum);
 Q_UNUSED(maximum);

 place();
}


void EditorFloatToc::onToggleCollapsed(void)
{
 m_collapsed=!m_collapsed;
 m_list->setVisible(!m_collapsed);
 m_headerButton->setArrowType(m_collapsed ? Qt::RightArrow : Qt::DownArrow);
 place();
}


void EditorFloatToc::onJump(QListWidgetItem *item)
{
 if(m_sync || m_area==nullptr || item==nullptr)
  return;

 bool ok=false;
 const int pos=item->data(Qt::UserRole).toInt(&ok);
 if(!ok)
  return;

 QTextCursor cursor(m_area->document());
 cursor.setPosition(pos);
 m_area->setTextCursor(cursor);
 m_area->ensureCursorVisible();
 m_area->setFocus(Qt::MouseFocusReason);
}


void EditorFloatToc::onCursorMoved(void)
{
 if(m_sync || m_area==nullptr || m_list==nullptr || m_list->count()==0)
  return;

 const int row=currentHeadingRow(m_area->textCursor().position());

 m_sync=true;
 if(row>=0)
  m_list->setCurrentRow(row);
 else
  m_list->clearSelection();
 m_sync=false;
}


int EditorFloatToc::currentHeadingRow(int cursorPos) const
{
 int row=-1;

 for(int i=0; i<count(); ++i)
 {
  bool ok=false;
  const int pos=item(i)->data(Qt::UserRole).toInt(&ok);
  if(!ok)
   return -1;

  if(pos<=cursorPos)
   row=i;
  else
   break;
 }

 return row;
}

