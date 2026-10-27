#ifndef ATTACHTABLESCREEN_H
#define ATTACHTABLESCREEN_H

#include <QWidget>

class AttachTableView;
class AttachTableTileView;
class AttachTableController;
class QToolBar;
class QVBoxLayout;
class QAction;
class QRadioButton;

// Виджет таблицы приаттаченных файлов (с кнопками)

class AttachTableScreen : public QWidget
{
  Q_OBJECT
public:
  AttachTableScreen(QWidget *parent = nullptr);
  virtual ~AttachTableScreen();

  void clear(void);

  QAction *actionAddAttach; // Добавление локального файла
  QAction *actionAddAttachFromUrl; // Добавление файла по интернет-Url
  QAction *actionAddLink; // Добавление линка на локальный файл
  QAction *actionEditFileName;
  QAction *actionDeleteAttach;
  QAction *actionOpenAttach;
  QAction *actionSaveAsAttach;
  QAction *actionShowAttachInfo;

  void setReadOnly(bool state);

public slots:

  void setupShortcuts(void);

  // Смена вида списка/плитки радиокнопками
  void onAttachViewModeChanged(void);

protected:

  void setupActions(void);
  void setupUI(void);
  void setupSignals(void);
  void assembly(void);


  // Указатель на контроллер таблицы приаттаченных файлов
  AttachTableController *attachTableController;

  // Указатель на таблицу приаттаченных файлов (представление)
  AttachTableView *attachTableView;

  // Переключатель вида: список или плитка
  QRadioButton *viewModeListRadio;
  QRadioButton *viewModeTilesRadio;

  // Экранные элементы
  QToolBar *toolsLine;
  QVBoxLayout *screenLayout;

  QAction *actionSwitchToEditor;

  // Применение выбранного вида (список/плитка) к видимости виджетов
  void updateAttachViewVisibility(void);

};

#endif // ATTACHTABLESCREEN_H
