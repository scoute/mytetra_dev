#ifndef _EDITORIMAGEPROPERTIES_H_
#define	_EDITORIMAGEPROPERTIES_H_

#include <QDialog>
#include <QImage>


class QLabel;
class QSpinBox;
class QPushButton;
class QDialogButtonBox;
class QWidget;


class EditorImageProperties : public QDialog
{
 Q_OBJECT

public:
 EditorImageProperties(QWidget *parent=nullptr);
 virtual ~EditorImageProperties();

 void set_info(QString infoText);
 void set_real_width(int width);
 void set_real_height(int height);
 void set_width(int width);
 void set_height(int height);

 int get_width(void);
 int get_height(void);

 void update_percent(void);

 // Разрешение кнопки истинного размера (мастер-тумблер из настроек,
 // по умолчанию выключен — кнопки нет вообще)
 void setTrueResizeAllowed(bool allowed);

 // Файл картинки и его наличие на диске; исходные пиксели для пересчета.
 // Без файла меняется только картинка в записи (документ пометится
 // измененным при записи). С файлом оригинал уходит в корзину
 void setTrueResizeFile(const QString &fileName, bool fileExists);
 void setSourceImage(const QImage &image);

 // Итог кнопки: была ли применена перезапись и чем
 bool trueResizeDone(void) const;
 QImage trueResizedImage(void) const;

 // Чистая операция для пробы: ужать файл-картинку до размера.
 // Формат берется по расширению файла, пропорции не ломаются:
 // вписывается в прямоугольник width x height
 static bool resizeImageFile(const QString &fileName, int width, int height);

private slots:

 void on_changed_percent(int n);
 void on_changed_width(int n);
 void on_changed_height(int n);

 void on_click_reset_size();

 // Кнопка "Применить размер к файлу": страшное предупреждение,
 // бэкап оригинала в корзину, пересчет, обновление реальных размеров
 void on_click_true_resize();

private:

 QLabel *infoLabel;

 QLabel *percentSizeLabel;
 QSpinBox *percentSizeSpin;


 QLabel *widthLabel;
 QSpinBox *widthSpin;

 QLabel *heightLabel;
 QSpinBox *heightSpin;

 QPushButton *resetSizeButton;

 QPushButton *trueResizeButton;

 QDialogButtonBox *buttonBox;

 void setup_ui(void);
 void setup_signals(void);
 void assembly(void);

 int imageRealWidth;
 int imageRealHeight;

 bool isRelateSizeSetted;

 // Состояние истинного ресайза: разрешен ли кнопкой, файл и его наличие,
 // исходные пиксели, итог применения
 bool trueResizeAllowed;
 QString trueResizeFileName;
 bool trueResizeFileExists;
 QImage trueResizeSource;
 bool trueResizeApplied;
 QImage trueResizeResult;
};

#endif	/* _EDITORIMAGEPROPERTIES_H_ */

