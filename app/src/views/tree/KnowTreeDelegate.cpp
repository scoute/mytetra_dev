#include <QPainter>
#include <QTextDocument>
#include <QApplication>
#include <QAbstractItemView>

#include "KnowTreeDelegate.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"


KnowTreeDelegate::KnowTreeDelegate(QObject *parent) : QStyledItemDelegate(parent)
{

}


KnowTreeDelegate::~KnowTreeDelegate()
{

}


void KnowTreeDelegate::setFilterText(const QString &text)
{
  m_filterText=text;
}


// Подсветка совпадения: штатный фон с иконкой рисуется без текста,
// сам текст — богатым текстом с цветными вхождениями. Цвета
// фиксированные теплые: читаются и на темной, и на светлой теме,
// и поверх выделения курсором (как желтая подложка вырезанной ветки)
QString KnowTreeDelegate::highlightedHtml(const QString &text, const QFont &font, int width) const
{
  const QFontMetrics metrics(font);
  const QAbstractItemView *view=qobject_cast<const QAbstractItemView *>(parent());
  const Qt::TextElideMode elideMode=(view!=nullptr) ? view->textElideMode() : Qt::ElideRight;
  const QString elided=metrics.elidedText(text, elideMode, width);

  if(elided.indexOf(m_filterText, 0, Qt::CaseInsensitive)<0)
    return QString();

  QString html;
  int pos=0;
  while(pos<elided.size())
  {
    const int match=elided.indexOf(m_filterText, pos, Qt::CaseInsensitive);
    if(match<0)
    {
      html+=elided.mid(pos).toHtmlEscaped();
      break;
    }

    html+=elided.mid(pos, match-pos).toHtmlEscaped();
    html+="<span style=\"background-color:#ffe182;color:#202020;\">"
        + elided.mid(match, m_filterText.size()).toHtmlEscaped()
        + "</span>";
    pos=match+m_filterText.size();
  }

  return html;
}


void KnowTreeDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  // Совпадение фильтра: текст рисуется с подсветкой вхождений
  if(!m_filterText.isEmpty())
  {
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    QString html;
    QRect textRect;
    if(!opt.text.isEmpty())
    {
      const QWidget *widget=option.widget;
      QStyle *style=(widget!=nullptr) ? widget->style() : QApplication::style();
      textRect=style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);
      html=highlightedHtml(opt.text, opt.font, textRect.width());
    }

    if(!html.isEmpty())
    {
      // Фон с иконкой штатно, но без текста (иначе задвоится)
      opt.text.clear();
      QStyledItemDelegate::paint(painter, opt, index);

      QTextDocument document;
      document.setDefaultFont(opt.font);
      document.setHtml(html);
      document.setTextWidth(textRect.width());

      painter->save();
      painter->setClipRect(textRect);
      painter->translate(textRect.left(),
                         textRect.top()+(textRect.height()-document.size().height())/2.0);
      document.drawContents(painter);
      painter->restore();

      // Вырезанная ветка поверх: та же логика что ниже
      const KnowTreeModel *treeModel=qobject_cast<const KnowTreeModel *>(index.model());
      if(treeModel!=nullptr)
      {
        const QString cutId=treeModel->cutBranchId();
        if(!cutId.isEmpty())
        {
          TreeItem *item=treeModel->getItem(index);
          if(item!=nullptr && item->getField(QStringLiteral("id"))==cutId)
          {
            painter->save();
            painter->fillRect(option.rect, QColor(255, 235, 130, 110));
            painter->restore();
          }
        }
      }

      return;
    }
  }

  // Стандартная отрисовка строки со всеми иконками и выделениями
  QStyledItemDelegate::paint(painter, option, index);

  // Вырезанная ветка подсвечивается желтой подложкой поверх штатной
  // отрисовки. Полупрозрачность сохраняет читаемость текста и оставляет
  // видимым выделение курсором, лишь подкрашивая его
  const KnowTreeModel *treeModel=qobject_cast<const KnowTreeModel *>(index.model());
  if(treeModel==nullptr)
    return;

  QString cutId=treeModel->cutBranchId();
  if(cutId.isEmpty())
    return;

  TreeItem *item=treeModel->getItem(index);
  if(item==nullptr || item->getField("id")!=cutId)
    return;

  painter->save();
  painter->fillRect(option.rect, QColor(255, 235, 130, 110));
  painter->restore();
}
