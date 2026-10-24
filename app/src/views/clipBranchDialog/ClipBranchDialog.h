#ifndef CLIPBRANCHDIALOG_H
#define CLIPBRANCHDIALOG_H

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>

class KnowTreeModel;
class QGridLayout;


// Модальный вопрос клиппера "куда вставить": плитка кнопок из корневых
// веток с двумя уровнями вложений. Всплывает поверх браузера поверх
// хоткея, клик по плитке сразу вставляет. Отмена — пустая строка

class ClipBranchDialog : public QDialog
{
  Q_OBJECT

 public:
  explicit ClipBranchDialog(QWidget *parent=nullptr);
  virtual ~ClipBranchDialog(void);

  // Общий листинг для диалога и настроек: пары (id ветки, путь " / ").
  // Корневые + два уровня вложений. Пустой список — дерево не готово
  static QList< QPair<QString, QString> > collectBranches(KnowTreeModel *model);

  // Модальный опрос: id выбранной ветки, пустая строка при отмене.
  // Окно поднимается поверх всех (браузер в фокусе в момент хоткея)
  static QString askBranch(QWidget *parent=nullptr);

  QString selectedBranchId(void) const;

 private slots:
  void onTileClicked(void);

 private:
  void setupUi(void);

  QString m_selectedId;
  QGridLayout *m_grid;
};

#endif // CLIPBRANCHDIALOG_H
