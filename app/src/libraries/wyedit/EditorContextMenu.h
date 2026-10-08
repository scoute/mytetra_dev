#ifndef EDITORCONTEXTMENU_H
#define EDITORCONTEXTMENU_H

#include <QMenu>

class EditorContextMenu : public QMenu
{
 Q_OBJECT

public:
  EditorContextMenu(QWidget *parent=nullptr);
  ~EditorContextMenu(void);

 void update(void);

 QList<QAction *> getActionsList(void);

 void setImageProperties(bool flag);
 void setCopySecret(bool flag); // Показ пункта "Скопировать секрет"
 void setChangeSecretColor(bool flag); // Показ пункта "Изменить цвет секретов"
 void setEditMathExpression(bool flag); // Активирование добавочного пункта меню "Редактирование формулы")
 void setGotoReference(bool flag); // Активирование добавочного пункта меню "Перейти по ссылке")
 void setEditReference(bool flag); // Пункт "Редактировать ссылку", виден на ссылке
 void setPasteNoteReference(bool flag); // Пункт "Вставить ссылку на заметку", виден если в буфере внутренняя ссылка
 void setPasteAsPlainText(bool flag);
 void setFormatToLowerCase(bool flag); // Активирование добавочного пункта меню "Строчные"
 void setFormatToUpperCase(bool flag); // Активирование добавочного пункта меню "ПРОПИСНЫЕ"

 // todo: разобраться, почему потребовалось иметь доступ к действиям Undo/Redo в этом месте
 inline QAction *getActionUndo() const {return actionUndo;}
 inline QAction *getActionRedo() const {return actionRedo;}

signals:
  void undo(void);
  void redo(void);
  void cut(void);
  void copy(void);
  void paste(void);
  void pasteAsPlainText(void);
  void selectAll(void);
  void contextMenuOpenImage(void);
  void contextMenuOpenImageWith(void);
  void contextMenuEditImageProperties(void);
  void contextMenuShowImageInFolder(void);
  void contextMenuCopySecret(void);
  void contextMenuChangeSecretColor(void);
  void contextMenuEditMathExpression(void);
  void contextMenuGotoReference(void);
  void contextMenuPasteNoteReference(void);
  void contextMenuInsertNoteReference(void);
  void contextMenuEditReference(void);
  void lowercase(void);
  void uppercase(void);

protected slots:

  void setupShortcuts(void);

private:

  QAction *actionUndo;
  QAction *actionRedo;
  QAction *actionCut;
  QAction *actionCopy;
  QAction *actionPaste;
  QAction *actionPasteAsPlainText;
  QAction *actionSelectAll;
  QAction *actionLowercase; // строчный
  QAction *actionUppercase; // ПРОПИСНЫЕ

  QAction *actionEditImageProperties;
  QAction *actionShowImageInFolder;
  QAction *actionCopySecret;
  QAction *actionChangeSecretColor;
  QAction *actionOpenImage;
  QAction *actionOpenImageWith;
  QAction *actionEditMathExpression;
  QAction *actionGotoReference;
  QAction *actionEditReference;
  QAction *actionPasteNoteReference;
  QAction *actionInsertNoteReference;

  void setupActions(void);
  void setupSignals(void);
  void setupMenu(void);

protected slots:

  void onActionUndo(void);
  void onActionRedo(void);
  void onActionCut(void);
  void onActionCopy(void);
  void onActionPaste(void);
  void onActionPasteAsPlainText(void);
  void onActionSelectAll(void);
  void onActionContextMenuOpenImage(void);
  void onActionContextMenuOpenImageWith(void);
  void onActionContextMenuEditImageProperties(void);
  void onActionContextMenuShowImageInFolder(void);
  void onActionContextMenuCopySecret(void);
  void onActionContextMenuChangeSecretColor(void);
  void onActionContextMenuEditMathExpression(void);
  void onActionContextMenuGotoReference(void);
  void onActionContextMenuEditReference(void);
  void onActionContextMenuPasteNoteReference(void);
  void onActionContextMenuInsertNoteReference(void);

  void onActionLowercase();
  void onActionUppercase();

};

#endif // EDITORCONTEXTMENU_H
