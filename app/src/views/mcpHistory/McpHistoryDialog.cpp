#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QDebug>

#include "McpHistoryDialog.h"


McpHistoryDialog::McpHistoryDialog(QWidget *parent) : QDialog(parent)
{
  setWindowTitle(tr("MCP action history"));
  setMinimumSize(560, 360);

  // Окно не модальное: сессия идёт фоном, пользователь работает дальше
  setModal(false);

  QLabel *filterLabel=new QLabel(this);
  filterLabel->setObjectName("mcpHistoryFilterLabel");
  filterLabel->setText(tr("Show:"));

  m_filterBox=new QComboBox(this);
  m_filterBox->addItem(tr("All"), QStringLiteral("all"));
  m_filterBox->addItem(tr("Reads"), QStringLiteral("read"));
  m_filterBox->addItem(tr("Writes"), QStringLiteral("write"));

  m_list=new QListWidget(this);

  QHBoxLayout *topLayout=new QHBoxLayout();
  topLayout->addWidget(filterLabel);
  topLayout->addWidget(m_filterBox);
  topLayout->addStretch();

  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addLayout(topLayout);
  centralLayout->addWidget(m_list);

  setLayout(centralLayout);

  connect(m_filterBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &McpHistoryDialog::onFilterChanged);
}


McpHistoryDialog::~McpHistoryDialog()
{

}


void McpHistoryDialog::setHistory(const QList<McpHistoryEntry> &entries)
{
  m_entries=entries;
  render();
}


int McpHistoryDialog::visibleCount(void) const
{
  return m_list!=nullptr ? m_list->count() : 0;
}


void McpHistoryDialog::onFilterChanged(int index)
{
  m_filter=m_filterBox->itemData(index).toString();
  render();
}


void McpHistoryDialog::render(void)
{
  if(m_list==nullptr)
    return;

  m_list->clear();

  for(const McpHistoryEntry &entry : m_entries)
  {
    if(m_filter!=QStringLiteral("all") && entry.kind!=m_filter)
      continue;

    QListWidgetItem *item=new QListWidgetItem(renderEntry(entry), m_list);

    if(entry.kind==QStringLiteral("read"))
      item->setBackground(QBrush(QColor(212, 237, 218)));
    else if(entry.kind==QStringLiteral("write"))
      item->setBackground(QBrush(QColor(248, 215, 218)));
  }
}


QString McpHistoryDialog::renderEntry(const McpHistoryEntry &entry) const
{
  QString kindText=entry.kind;

  if(entry.kind==QStringLiteral("read"))
    kindText=tr("read");
  else if(entry.kind==QStringLiteral("write"))
    kindText=tr("write");

  const QString time=QDateTime::fromMSecsSinceEpoch(entry.msecs)
                     .toString(QStringLiteral("hh:mm:ss"));

  if(entry.summary.isEmpty())
    return QString("%1 [%2] %3").arg(time, kindText, entry.tool);

  return QString("%1 [%2] %3 %4").arg(time, kindText, entry.tool, entry.summary);
}


void McpHistoryDialog::retranslateUi(void)
{
  setWindowTitle(tr("MCP action history"));

  QLabel *filterLabel=findChild<QLabel *>(QStringLiteral("mcpHistoryFilterLabel"));

  if(filterLabel!=nullptr)
    filterLabel->setText(tr("Show:"));

  // Текущий фильтр запоминается кодом, а не индексом
  const QString kept=m_filter;

  m_filterBox->setItemText(0, tr("All"));
  m_filterBox->setItemText(1, tr("Reads"));
  m_filterBox->setItemText(2, tr("Writes"));

  const int idx=m_filterBox->findData(kept);

  if(idx>=0)
    m_filterBox->setCurrentIndex(idx);

  render();
}
