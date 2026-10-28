#ifndef _EDITORSPECIALCHARDIALOG_H_
#define	_EDITORSPECIALCHARDIALOG_H_

#include <QDialog>
#include <QString>
#include <QStringList>

class QGridLayout;
class QPushButton;
class QLabel;
class QDialogButtonBox;
class QScrollArea;
class QShowEvent;


// Выбор спецсимволов для вставки в текст: таблица символов и ряд
// недавно использованных. Клик по кнопке сразу вставляет символ
// в редактор (сигнал symbolPicked) и по настройке закрывает диалог
// (по умолчанию) либо оставляет открытым для набора нескольких
// символов подряд. Закрытие — Cancel, Esc или крестик
class EditorSpecialCharDialog : public QDialog
{
 Q_OBJECT

public:
 EditorSpecialCharDialog(QWidget *parent=nullptr);
 virtual ~EditorSpecialCharDialog();

 QString selectedSymbol(void) const;

 // Весь набор вставляемых символов (для пробы и переиспользования)
 static QStringList allSymbols(void);

signals:

 // Клик по символу: вставить прямо сейчас, дальше по настройке
 // диалог закроется или останется открытым
 void symbolPicked(const QString &symbol);

private slots:

 void on_symbol_button_clicked(void);

private:

 void setup_ui(void);
 void assembly(void);

 // Одноразовое доращивание ширины при показе: QSS-рамки тем
 // выедают вьюпорт, ручной расчет в assembly их не видит
 void showEvent(QShowEvent *event) override;

 // Кнопки таблицы и недавних: текст кнопки и есть символ
 void fillGrid(QGridLayout *layout,
               const QStringList &symbols,
               int columns,
               int &row);

 QString pickedSymbol;

 QList<QPushButton *> symbolButtons;

 // Прокрутка таблицы: нужна в showEvent для проверки скролла
 QScrollArea *scroll=nullptr;

 // Ширина уже доращивалась: повторный показ не трогаем
 bool widthFixed=false;
};


#endif	/* _EDITORSPECIALCHARDIALOG_H_ */
