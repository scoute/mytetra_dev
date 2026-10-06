#ifndef IMAGEFORMATTER_H
#define IMAGEFORMATTER_H

#include <QTextImageFormat>
#include <QTextDocumentFragment>

#include "Formatter.h"

// Класс для работы с картинками в тексте

// class QTextDocumentFragment;

class ImageFormatter : public Formatter
{
  Q_OBJECT

public:
  ImageFormatter();

  QTextImageFormat imageFormatOnSelect(void);
  QTextImageFormat imageFormatOnCursor(void);

  void editImageProperties(void);

  // Открыть картинку под курсором во внешней программе ОС
  void openImage(void);

  // Открыть картинку под курсором в выбранной через диалог программе
  void openImageWith(void);

signals:

  void downloadImagesSuccessfull(const QString html,
                                 const QMap<QString, QByteArray> referencesAndMemoryFiles,
                                 const QMap<QString, QString> referencesAndInternalNames);

public slots:

  void onInsertImageFromFileClicked(void);
  void onContextMenuEditImageProperties(void);
  void onShowImageInFolder(void);

  // Открытие картинки из контекстного меню и по Ctrl+клику
  void onContextMenuOpenImage(void);
  void onContextMenuOpenImageWith(void);
  void onClickOnImage(void);

  void onDownloadImages(const QString html);

  void onDoubleClickOnImage(void);

private:

  // Путь к файлу картинки под курсором. Пусто если курсор не на
  // картинке или файла нет: диагностика уже показана
  QString resolveImageFilePath(void);

};

#endif // IMAGEFORMATTER_H
