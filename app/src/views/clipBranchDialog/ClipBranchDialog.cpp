#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTreeView>
#include <QDialogButtonBox>
#include <QPushButton>
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


// Переключалка вида. Выбор пишется в конфиг сразу: диалог модальный,
// кнопки "Применить" у него нет, а вид хочется помнить между клипами
void ClipBranchDialog::onViewModeChanged(void)
{
  const bool treeMode=m_treeRadio->isChecked();

  mytetraConfig.set_clipperaskview(treeMode ? 1 : 0);
  m_stack->setCurrentIndex(treeMode ? 1 : 0);

  if(QPushButton *okButton=m_buttonBox->button(QDialogButtonBox::Ok))
  {
    okButton->setVisible(treeMode);
    if(treeMode)
      onTreeSelectionChanged();
  }
}


void ClipBranchDialog::onTreeDoubleClicked(const QModelIndex &index)
{
  if(!index.isValid())
    return;

  KnowTreeModel *model=nullptr;
  if(TreeScreen *treeScreen=globalParameters.getTreeScreen())
    model=treeScreen->knowTreeModel;

  if(!model)
    return;

  TreeItem *item=model->getItem(index);
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

  // Переключалка вида вверху: плитка для быстрого клипа,
  // дерево когда связь веток по путям не читается
  m_tilesRadio=new QRadioButton(tr("Tiles"), this);
  m_treeRadio=new QRadioButton(tr("Tree"), this);

  connect(m_tilesRadio, &QRadioButton::toggled, this, &ClipBranchDialog::onViewModeChanged);

  QHBoxLayout *topLayout=new QHBoxLayout;
  topLayout->addWidget(questionLabel);
  topLayout->addStretch();
  topLayout->addWidget(m_tilesRadio);
  topLayout->addWidget(m_treeRadio);

  KnowTreeModel *model=nullptr;
  if(TreeScreen *treeScreen=globalParameters.getTreeScreen())
    model=treeScreen->knowTreeModel;

  setupTiles();
  setupTree(model);

  m_stack=new QStackedWidget(this);
  m_stack->addWidget(m_tilesScroll);
  m_stack->addWidget(m_treeView);

  m_buttonBox=new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(m_buttonBox, &QDialogButtonBox::accepted, this, &ClipBranchDialog::onOkClicked);
  connect(m_buttonBox, &QDialogButtonBox::rejected, this, &ClipBranchDialog::reject);

  QVBoxLayout *centralLayout=new QVBoxLayout;
  centralLayout->addLayout(topLayout);
  centralLayout->addWidget(m_stack);
  centralLayout->addWidget(m_buttonBox);
  setLayout(centralLayout);

  // Вид из конфига, дефолт — плитка. Ok видна только в дереве:
  // у плитки клик сам по себе вставка, кнопка там бессмысленна
  const bool treeMode=(mytetraConfig.get_clipperaskview()==1);
  m_treeRadio->setChecked(treeMode);
  m_tilesRadio->setChecked(!treeMode);
  onViewModeChanged();

  resize(640, 480);
}


void ClipBranchDialog::setupTiles(void)
{
  KnowTreeModel *model=nullptr;
  if(TreeScreen *treeScreen=globalParameters.getTreeScreen())
    model=treeScreen->knowTreeModel;

  m_grid=new QGridLayout;
  m_grid->setSpacing(8);

  const QList< QPair<QString, QString> > branches=collectBranches(model);

  // Плитки: три колонки, текст — путь ветки, id — в свойстве кнопки
  const int columns=3;
  for(int n=0; n<branches.size(); ++n)
  {
    QPushButton *tile=new QPushButton(branches.at(n).second, this);
    tile->setProperty("branchId", branches.at(n).first);
    tile->setMinimumSize(QSize(180, 48));
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
// Раскрыто на два уровня — то же содержимое что в плитке, дальнее
// раскрывается руками
void ClipBranchDialog::setupTree(KnowTreeModel *model)
{
  m_treeView=new QTreeView(this);
  m_treeView->setHeaderHidden(true);
  m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
  m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);

  if(model!=nullptr)
  {
    m_treeView->setModel(model);
    m_treeView->expandToDepth(2);

    // Кнопка Ok активна только при выборе: вставка вслепую ни к чему
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ClipBranchDialog::onTreeSelectionChanged);
  }

  connect(m_treeView, &QTreeView::doubleClicked, this, &ClipBranchDialog::onTreeDoubleClicked);
}
