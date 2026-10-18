#ifndef _PRINTPREVIEW_H_
#define _PRINTPREVIEW_H_

#include <QWidget>
#include <QPrinter>


class QTextDocument;
class QVBoxLayout;
class QToolButton;

class PreviewView;

class PrintPreview : public QWidget
{
    Q_OBJECT
public:
    // Окно намеренно QWidget+Qt::Window, а не QDialog с родителем:
    // модальный transient-диалог оконный менеджер лишает кнопок
    // свернуть/развернуть на свой вкус (проверено: ни OR-хинты, ни полный
    // набор через CustomizeWindowHint кнопок не дали). Отдельное окно
    // верхнего уровня с WindowModal получает полное оформление
    PrintPreview(const QTextDocument *document, QWidget *parent);
    virtual ~PrintPreview();

public slots:

  void setupShortcuts(void);

private slots:
    void print();
    void pageSetup();

private:
    void setupPrintDoc();
    void setupUI();
    void setupSignals();
    void assembly();

    QTextDocument *doc;
    PreviewView *view;
    QPrinter printer;

    QToolButton *buttonPrint;
    QToolButton *buttonPageSetup;
    QToolButton *buttonZoomIn;
    QToolButton *buttonZoomOut;
    QToolButton *buttonClose;
    
    QVBoxLayout *centralLayout;
};

#endif // _PRINTPREVIEW_H_
