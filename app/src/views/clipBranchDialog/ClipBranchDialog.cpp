#include <algorithm>

#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QScrollArea>
#include <QTreeView>
#include <QDialogButtonBox>
#include <QModelIndex>
#include <QItemSelectionModel>

#include "ClipBranchDialog.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "views/tree/TreeScreen.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;


ClipBranchFilter::ClipBranchFilter(QObject *parent) : QSortFilterProxyModel(parent)
{
  // Предки совпадений показываются сами: глубина дерева не режется
  setRecursiveFilteringEnabled(true);
}


void ClipBranchFilter::setFilterText(const QString &text)
{
  m_text=text;
  invalidateFilter();
}


bool ClipBranchFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
  if(m_text.isEmpty())
    return true;

  const QModelIndex index=sourceModel()->index(sourceRow, 0, sourceParent);

  return index.data(Qt::DisplayRole).toString().contains(m_text, Qt::CaseInsensitive);
}


ClipBranchDialog::ClipBranchDialog(QWidget *parent) : QDialog(parent)
{
  setupUi();
}


ClipBranchDialog::~ClipBranchDialog(void)
{

}


// Корневые + два уровня вложений. Три плоских цикла вместо рекурсии:
// глубина фиксирована, читать проще, ошибиться негде
QList< QPair<QString, QString> > ClipBranchDialog::collectBranches(KnowTreeModel *model)
{
  QList< QPair<QString, QString> > result;

  if(!model)
    return result;

  TreeItem *rootItem=model->getItem(QModelIndex());
  if(!rootItem)
    return result;

  for(int i=0; i<rootItem->childCount(); ++i)
  {
    TreeItem *level1=rootItem->child(i);
    result << qMakePair(level1->getField(QStringLiteral("id")),
                        level1->getPathAsNameWithDelimeter(QStringLiteral(" / ")));

    for(int j=0; j<level1->childCount(); ++j)
    {
      TreeItem *level2=level1->child(j);
      result << qMakePair(level2->getField(QStringLiteral("id")),
                          level2->getPathAsNameWithDelimeter(QStringLiteral(" / ")));

      for(int k=0; k<level2->childCount(); ++k)
      {
        TreeItem *level3=level2->child(k);
        result << qMakePair(level3->getField(QStringLiteral("id")),
                            level3->getPathAsNameWithDelimeter(QStringLiteral(" / ")));
      }
    }
  }

  return result;
}


// Поверх всех: в момент хоткея в фокусе браузер, обычное модальное окно
// за ним бы потерялось. StayOnTop + raise + activateWindow вытягивают
// вопрос на передний фон
QString ClipBranchDialog::askBranch(QWidget *parent)
{
  ClipBranchDialog dialog(parent);
  dialog.setWindowFlags(dialog.windowFlags() | Qt::WindowStaysOnTopHint);
  dialog.show();
  dialog.raise();
  dialog.activateWindow();

  if(dialog.exec()!=QDialog::Accepted)
    return QString();

  return dialog.selectedBranchId();
}


QString ClipBranchDialog::selectedBranchId(void) const
{
  return m_selectedId;
}


void ClipBranchDialog::onTileClicked(void)
{
  QPushButton *tile=qobject_cast<QPushButton *>(sender());
  if(!tile)
    return;

  m_selectedId=tile->property("branchId").toString();
  accept();
}


// Фильтр режет оба вида: плитки прячутся по подстроке пути,
// дерево — через прокси (предки совпадений остаются на месте)
void ClipBranchDialog::onFilterChanged(const QString &text)
{
  for(int i=0; i<m_grid->count(); ++i)
    if(QWidget *tile=m_grid->itemAt(i)->widget())
      tile->setVisible(tile->property("branchPath").toString()
                       .contains(text, Qt::CaseInsensitive));

  if(m_treeFilter!=nullptr)
    m_treeFilter->setFilterText(text);
}


void ClipBranchDialog::onTreeDoubleClicked(const QModelIndex &index)
{
  if(!index.isValid() || m_treeFilter==nullptr)
    return;

  KnowTreeModel *model=nullptr;
  if(TreeScreen *treeScreen=globalParameters.getTreeScreen())
    model=treeScreen->knowTreeModel;

  if(!model)
    return;

  TreeItem *item=model->getItem(m_treeFilter->mapToSource(index));
  if(!item)
    return;

  m_selectedId=item->getField(QStringLiteral("id"));
  accept();
}


void ClipBranchDialog::onTreeSelectionChanged(void)
{
  bool hasSelection=false;

  if(m_treeView->selectionModel()!=nullptr)
    hasSelection=m_treeView->selectionModel()->hasSelection();

  if(QPushButton *okButton=m_buttonBox->button(QDialogButtonBox::Ok))
    okButton->setEnabled(hasSelection);
}


void ClipBranchDialog::onOkClicked(void)
{
  if(m_treeView->selectionModel()==nullptr)
    return;

  onTreeDoubleClicked(m_treeView->selectionModel()->currentIndex());
}


void ClipBranchDialog::setupUi(void)
{
  setWindowTitle(tr("Where to save the clip?"));

  QLabel *questionLabel=new QLabel(tr("Where do you want to paste?"), this);
  QFont questionFont=questionLabel->font();
  questionFont.setBold(true);
  questionLabel->setFont(questionFont);

  m_searchEdit=new QLineEdit(this);
  m_searchEdit->setPlaceholderText(tr("Filter branches..."));
  m_searchEdit->setClearButtonEnabled(true);
  connect(m_searchEdit, &QLineEdit::textChanged, this, &ClipBranchDialog::onFilterChanged);

  KnowTreeModel *model=nullptr;
  if(TreeScreen *treeScreen=globalParameters.getTreeScreen())
    model=treeScreen->knowTreeModel;

  setupTree(model);
  setupTiles();

  m_tilesLabel=new QLabel(this);
  if(mytetraConfig.get_clippertilemode()==1)
    m_tilesLabel->setText(tr("Frequent branches:"));
  else
    m_tilesLabel->setText(tr("Recent branches:"));

  m_buttonBox=new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(m_buttonBox, &QDialogButtonBox::accepted, this, &ClipBranchDialog::onOkClicked);
  connect(m_buttonBox, &QDialogButtonBox::rejected, this, &ClipBranchDialog::reject);

  QVBoxLayout *centralLayout=new QVBoxLayout;
  centralLayout->addWidget(questionLabel);
  centralLayout->addWidget(m_searchEdit);
  centralLayout->addWidget(m_treeView, 1);
  centralLayout->addWidget(m_tilesLabel);
  centralLayout->addWidget(m_tilesScroll);
  centralLayout->addWidget(m_buttonBox);
  setLayout(centralLayout);

  onTreeSelectionChanged();

  resize(640, 620);
}


// Внизу — 16 плиток: последние или частые по настройке (история одна,
// с счетчиками). Пусто или ветки удалены — полный список как раньше
void ClipBranchDialog::setupTiles(void)
{
  KnowTreeModel *model=nullptr;
  if(TreeScreen *treeScreen=globalParameters.getTreeScreen())
    model=treeScreen->knowTreeModel;

  m_grid=new QGridLayout;
  m_grid->setSpacing(8);

  // История: пары id:count, порядок — свежесть. Частым нужен
  // обратный порядок по счетчику, связи рвутся стабильной сортировкой
  QMap<QString, int> counts;
  QStringList order;
  const QStringList pairs=mytetraConfig.get_clipperrecent()
                           .split(QChar(';'), Qt::SkipEmptyParts);
  for(const QString &pair : pairs)
  {
    const int colon=pair.lastIndexOf(QChar(':'));
    QString id;
    int count=1;
    if(colon>0)
    {
      id=pair.left(colon);
      count=pair.mid(colon+1).toInt();
      if(count<1)
        count=1;
    }
    else
      id=pair;

    if(id.isEmpty() || counts.contains(id))
      continue;

    counts[id]=count;
    order << id;
  }

  if(mytetraConfig.get_clippertilemode()==1)
    std::stable_sort(order.begin(), order.end(),
                     [&](const QString &first, const QString &second)
                     { return counts.value(first, 0)>counts.value(second, 0); });

  QList< QPair<QString, QString> > branches;
  if(model!=nullptr)
    for(const QString &id : order)
    {
      if(branches.size()>=maxTiles)
        break;

      if(TreeItem *item=model->getItemById(id))
        branches << qMakePair(item->getField(QStringLiteral("id")),
                              item->getPathAsNameWithDelimeter(QStringLiteral(" / ")));
    }

  if(branches.isEmpty())
    branches=collectBranches(model);

  // Плитки 4x4: текст — путь ветки, id и путь — в свойствах кнопки
  // (путь нужен фильтру)
  const int columns=4;
  for(int n=0; n<branches.size() && n<maxTiles; ++n)
  {
    QPushButton *tile=new QPushButton(branches.at(n).second, this);
    tile->setProperty("branchId", branches.at(n).first);
    tile->setProperty("branchPath", branches.at(n).second);
    tile->setMinimumSize(QSize(140, 44));
    connect(tile, &QPushButton::clicked, this, &ClipBranchDialog::onTileClicked);
    m_grid->addWidget(tile, n/columns, n%columns);
  }

  QWidget *tilesWidget=new QWidget(this);
  tilesWidget->setLayout(m_grid);

  m_tilesScroll=new QScrollArea(this);
  m_tilesScroll->setWidgetResizable(true);
  m_tilesScroll->setWidget(tilesWidget);
}


// Дерево на живой модели: иерархия видна как есть, глубина любая.
// Раскрыто на два уровня — дальнее раскрывается руками
void ClipBranchDialog::setupTree(KnowTreeModel *model)
{
  m_treeView=new QTreeView(this);
  m_treeView->setHeaderHidden(true);
  m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
  m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);

  m_treeFilter=nullptr;

  if(model!=nullptr)
  {
    m_treeFilter=new ClipBranchFilter(this);
    m_treeFilter->setSourceModel(model);
    m_treeView->setModel(m_treeFilter);
    m_treeView->expandToDepth(2);

    // Кнопка Ok активна только при выборе: вставка вслепую ни к чему
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ClipBranchDialog::onTreeSelectionChanged);
  }

  connect(m_treeView, &QTreeView::doubleClicked, this, &ClipBranchDialog::onTreeDoubleClicked);
}
