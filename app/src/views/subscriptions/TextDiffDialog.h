#ifndef TEXTDIFFDIALOG_H
#define TEXTDIFFDIALOG_H

#include <QDialog>
#include <QPair>
#include <QString>

// Просмотр построчного diff текста записи в отдельном окне.
//
// Дополнение ко встроенному diff в диалоге «Что изменилось»:
//   вкладка «Diff» — построчный diff (локальная копия -> версия владельца);
//   вкладка «Было / Стало» — полный рендер обеих страниц рядом
//   (удобно для картинок и форматирования, которые в построчном виде
//   теряются). Только чтение, базу не трогает.

class QComboBox;
class QSplitter;

class TextDiffDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TextDiffDialog(const QString &recordTitle,
                            const QString &diffHtml,
                            QWidget *parent=nullptr,
                            const QString &oldHtml=QString(),
                            const QString &newHtml=QString(),
                            const QPair<QString, QString> &baseDirs
                                =QPair<QString, QString>());

private slots:
    void onViewModeChanged(int index);

private:
    // Тройной вид: слева построчный diff, справа — рендер «Было / Стало»
    // (режим 0, по умолчанию) либо diff сверху, рендеры снизу (режим 1)
    QSplitter *mainSplitter;
    QComboBox *viewModeSelector;
};

#endif // TEXTDIFFDIALOG_H
