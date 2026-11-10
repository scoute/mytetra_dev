#ifndef _PRINTPREVIEW_H_
#define _PRINTPREVIEW_H_

#include <QWidget>
#include <QPrinter>


class QTextDocument;
class QVBoxLayout;
class QHBoxLayout;
class QToolButton;
class QDialog;

class PreviewView;
class PageThumbnailBar;
class MtComboBox;
class QCheckBox;

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

  // Общая подготовка документа к печати и предпросмотру (см. 584-586):
  // размер страницы и поля — в пунктах (1/72 дюйма): это единицы,
  // в которых QTextDocument::print() верстает и маппит на принтер.
  // Пиксели принтера/экрана здесь неверны: layout в device-px дает
  // микроскопический вывод, layout в экранных px — мелкий шрифт.
  // Явный pageSize заодно гасит автонумерацию страниц Qt
  static void prepareDocument(QPrinter &printer, QTextDocument *doc);
  static void fixStockDialogCombos(QDialog *dialog);

  // Печать постранично вручную вместо QTextDocument::print():
  // тот без явного pageSize пагинирует сам и добавляет номера,
  // а с явным — масштабирует непредсказуемо. Здесь раскладка
  // из prepareDocument, номера — тем же drawFooterNumber что
  // в превью (настройка печати и положение — из конфига)
  static void printDocument(QPrinter &printer, QTextDocument *doc);

private slots:
    void print();
    void pageSetup();
    void onAlignChanged(int index);
    void onFootersToggled(bool checked);

    // Следование подсветки ленты за прокруткой основного вида
    void onMainScrollChanged(int value);

private:
    void setupPrintDoc();
    void setupUI();
    void setupSignals();
    void assembly();

  private:
    QTextDocument *doc;
    PreviewView *view;
    PageThumbnailBar *thumbBar;
    QPrinter printer;

    QToolButton *buttonPrint;
    QToolButton *buttonPageSetup;
    QToolButton *buttonZoomIn;
    QToolButton *buttonZoomOut;
    QToolButton *buttonZoomReset;
    QToolButton *buttonZoomFit;
    MtComboBox *alignCombo;
    QCheckBox *footersCheckBox;
    QToolButton *buttonClose;
    
    QVBoxLayout *centralLayout;
};

#endif // _PRINTPREVIEW_H_
