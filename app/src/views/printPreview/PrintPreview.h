#ifndef _PRINTPREVIEW_H_
#define _PRINTPREVIEW_H_

#include <QWidget>
#include <QPrinter>


class QTextDocument;
class QVBoxLayout;
class QToolButton;

class PreviewView;
class PageThumbnailBar;

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

    // Следование подсветки ленты за прокруткой основного вида
    void onMainScrollChanged(int value);

private:
    void setupPrintDoc();
    void setupUI();
    void setupSignals();
    void assembly();

    QTextDocument *doc;
    PreviewView *view;
    PageThumbnailBar *thumbBar;
    QPrinter printer;

    QToolButton *buttonPrint;
    QToolButton *buttonPageSetup;
    QToolButton *buttonZoomIn;
    QToolButton *buttonZoomOut;
    QToolButton *buttonZoomReset;
    QToolButton *buttonClose;
    
    QVBoxLayout *centralLayout;
};

#endif // _PRINTPREVIEW_H_
