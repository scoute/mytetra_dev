#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QModelIndex>

#include "ClipBranchDialog.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "views/tree/TreeScreen.h"
#include "libraries/GlobalParameters.h"


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


void ClipBranchDialog::setupUi(void)
{
  setWindowTitle(tr("Where to save the clip?"));

  QLabel *questionLabel=new QLabel(tr("Where do you want to paste?"), this);
  QFont questionFont=questionLabel->font();
  questionFont.setBold(true);
  questionLabel->setFont(questionFont);

  // Плитки: три колонки, текст — путь ветки, id — в свойстве кнопки
  m_grid=new QGridLayout;
  m_grid->setSpacing(8);

  KnowTreeModel *model=nullptr;
  if(TreeScreen *treeScreen=globalParameters.getTreeScreen())
    model=treeScreen->knowTreeModel;

  const QList< QPair<QString, QString> > branches=collectBranches(model);

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

  QScrollArea *scrollArea=new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setWidget(tilesWidget);

  QDialogButtonBox *buttonBox=new QDialogButtonBox(QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &ClipBranchDialog::reject);

  QVBoxLayout *centralLayout=new QVBoxLayout;
  centralLayout->addWidget(questionLabel);
  centralLayout->addWidget(scrollArea);
  centralLayout->addWidget(buttonBox);
  setLayout(centralLayout);

  resize(640, 480);
}
