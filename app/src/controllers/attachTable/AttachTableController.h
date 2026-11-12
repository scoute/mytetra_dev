#ifndef __ATTACHTABLECONTROLLER_H__
#define __ATTACHTABLECONTROLLER_H__

#include <QObject>
#include <QModelIndexList>
#include <QItemSelectionModel>


class AttachTableView;
class AttachTableTileView;
class AttachTableModel;
class AttachTableData;
class MetaEditor;

class AttachTableController : public QObject
{
  Q_OBJECT

public:

  AttachTableController(QObject *parent, QObject *iMetaEditor );
  virtual ~AttachTableController();

  AttachTableView *getView(void);

  // Плитка и текущий видимый вид (для родителей диалогов и ширины)
  AttachTableTileView *getTileView(void);
  QWidget *currentView(void) const;

  void setAttachTableData(AttachTableData *attachTableData);
  AttachTableData *getAttachTableData();

  QList<QString> getSelectedId(void);

public slots:

  void onAddAttach(void);
  void onAddAttachFromUrl(void);
  void onAddLink(void);
  void onEditFileName(void);
  void onDeleteAttach(void);
  void onOpenAttach(void);
  void onShowAttachInfo(void);
  void onSaveAsAttach(void);

  void onSwitchToEditor(void);

  // Файлы из drag'n'drop (вид дергает напрямую, без диалога)
  void addDroppedFiles(const QStringList &files);

  // Один файл из дропа в текст: тот же движок что addFileList,
  // но возвращает имена для вставки кликабельной метки.
  // Пусто в innerFileName если файл не добавился
  bool addSingleFileAsAttach(const QString &filePath,
                             QString &innerFileName,
                             QString &displayFileName);

  // Одна папка из дропа в текст или с панели: строкой-ссылкой
  // в таблицу. Возвращает id строки и видимое имя для метки
  // в тексте. Пусто в rowId если папка не добавилась (ошибка,
  // несохраненная запись, отмена диалога). Режим ссылки —
  // настройка attachLinkMode или диалог при "спрашивать"
  bool addSingleFolderAsLink(const QString &dirPath,
                             QString &rowId,
                             QString &displayName);

  // Файл из дропа в текст заметки: при настройке "всегда спрашивать"
  // показывает меню дропа (копия/ссылка, см. requestDropFileMode),
  // иначе молча копирует как раньше. Возвращает true если файл
  // добавлен (копией или ссылкой). addedAsLink=true если строкой-ссылкой:
  // метку-копию в текст не вставлять, вызыватель ставит метку file://
  // сам (см. Editor). Вставка меток из буфера идет мимо
  // (там всегда копии, см. onAttachBadgesPasted)
  bool addTextDropFile(const QString &filePath,
                       QString &innerFileName,
                       QString &displayFileName,
                       bool *addedAsLink);

  // Удаление аттача по id строки для движка меток папок:
  // строка удаляется, дерево сохраняется, иконка/статистика
  // обновляются. Пустой id или нет строки: false
  bool deleteAttachById(const QString &rowId);

  // Удаление аттача по внутреннему имени для движка переноса метки:
  // находит строку (прямое имя или любая схема), удаляет файл
  // и запись, сохраняет дерево и обновляет иконку/статистику
  bool removeAttachByInnerName(const QString &innerName);

protected:

  void addSmart(QString attachType);
  // presetLinkMode: предустановленный режим ссылок для всей пачки
  // (выбор дропа, см. requestDropFileMode). AttachLinkAsk означает
  // "резолвить как обычно" — настройка или диалог по первому вхождению
  void addFileList(QString attachType, const QStringList &files, int presetLinkMode);
  bool addAttach(QString attachType, QString currFullFileName, QString currShortFileName, const QString &source=QString());
  void saveState();
  QStringList selectFilesForAdding(QString attachType);
  void saveAttachToUserPlace(QString fromFullFileName, QString toFullFileName, QString attachType, bool isAttachCrypt);
  void updateAttachListInEditor(void);

  // Режим ссылки для добавляемой папки или файла: настройка
  // attachLinkMode, при "всегда спрашивать" — диалог с запоминанием
  // ("всегда" пишет настройку). isFolder меняет заголовок и пояснение
  // диалога. Возвращает AttachLinkAbsolute/Relative,
  // при отмене диалога -1
  int requestLinkMode(bool isFolder);

  // Режим для перетаскиваемых файлов при настройке "всегда спрашивать":
  // копией в базу или ссылкой (разовый выбор, только этот дроп).
  // Возвращает AttachLinkAbsolute/Relative, *addAsCopy=true для копии,
  // -1 при отмене. Папки в том же дропе идут выбранным ссылочным
  // режимом без второго диалога; при копии файлов папки спрашивают
  // обычным диалогом. Без Ask не вызывается
  int requestDropFileMode(bool *addAsCopy);

  // Открытая в редакторе заметка — та же что таблица аттачей.
  // Метки в текст трогаются только при совпадении, иначе дроп
  // ушел бы в чужую заметку
  bool isMetaEditorSameNote(void);

  // Общая обработка добавляемой папки: поиск дубля по каноническому
  // пути, вопрос переключения режима, добавление новой строки.
  // wantedMode — резолвленный режим (не Ask). Возвращает id строки:
  // новой, существующей (дубль/переключение) или пусто при отказе
  // движка. addedNew=true только если строка создана сейчас.
  // Чистый дубль: пропуск + сообщение, метку восстанавливает вызыватель
  QString processFolderLink(const QString &absolutePath, int wantedMode, bool *addedNew);

  // Папка уже прикреплена другим режимом: спросить переключить
  // на новый или оставить. Переключение правит поле link строки
  // (rowId не меняется — метки остаются валидны), дерево сохраняется.
  // Возвращает true если переключили
  bool askAndSwitchFolderLink(const QString &folderName, int oldMode, int newMode);

  // Общая обработка добавляемого файла-ссылки: поиск дубля
  // по каноническому пути, вопрос переключения режима, добавление
  // новой строки. Симметрия processFolderLink, но без меток в текст
  // (метки файлов — только копии через addSingleFileAsAttach).
  // Копия того же файла (type file) — не дубль: снимок и живая
  // ссылка живут рядом. Возвращает id строки: новой, существующей
  // (дубль/переключение) или пусто при отказе движка
  QString processFileLink(const QString &absolutePath, int wantedMode, bool *addedNew);

  // Файл уже прикреплен ссылкой другим режимом: спросить переключить
  // на новый или оставить. Возвращает true если переключили
  bool askAndSwitchFileLink(const QString &fileName, int oldMode, int newMode);

  AttachTableView *view;
  AttachTableModel *model;

  // Плитка с тем же model; выделение общее через selectionModel —
  // действия контроллера не зависят от активного вида
  AttachTableTileView *tileView;
  QItemSelectionModel *selectionModel;

  MetaEditor *metaEditor;
};

#endif // __ATTACHTABLECONTROLLER_H__
