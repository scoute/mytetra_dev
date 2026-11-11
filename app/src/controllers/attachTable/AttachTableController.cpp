#include <QObject>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

#include <algorithm>

#include "AttachTableController.h"
#include "views/attachTable/AttachTableView.h"
#include "views/attachTable/AttachTableTileView.h"
#include "models/attachTable/AttachTableModel.h"
#include "models/attachTable/AttachTableData.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/crypt/CryptService.h"
#include "libraries/helpers/DiskHelper.h"
#include "views/tree/TreeScreen.h"
#include "models/tree/KnowTreeModel.h"
#include "views/mainWindow/MainWindow.h"
#include "views/dialog/ReduceMessageBox.h"
#include "libraries/Downloader.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/MessageHelper.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "views/record/MetaEditor.h"
#include "libraries/wyedit/formatters/ReferenceFormatter.h"
#include "libraries/AppContext.h"
#include "models/appConfig/AppConfigStorage.h"

AttachTableController::AttachTableController(QObject *parent, QObject *iMetaEditor) : QObject(parent)
{
  // Запоминается указатель на редактор
  // Область со списком приаттаченных файлов находится на том же логическом уровне что и редактор, и взаимодействует с ним
  metaEditor=static_cast<MetaEditor*>(iMetaEditor);

  // Создается область со списком файлов
  view=new AttachTableView( qobject_cast<QWidget *>(parent) ); // Вид размещается внутри виджета Screen
  view->setObjectName("attachTableView");
  view->setController(this);
  view->init(); // Инициализация запускается только после установки контроллера

  // Создание модели данных (тонкой обертки над AttachTableData)
  model=new AttachTableModel(this);
  model->setObjectName("attachTableModel");

  // Модель данных задается для вида
  view->setModel(model);

  // Плитка на той же модели; выделение общее с таблицей, чтобы все
  // действия (открыть, удалить, инфо) не зависели от активного вида
  tileView=new AttachTableTileView( qobject_cast<QWidget *>(parent) );
  tileView->setObjectName("attachTableTileView");
  tileView->setController(this);
  tileView->init();
  tileView->setModel(model);

  selectionModel=new QItemSelectionModel(model, this);
  view->setSelectionModel(selectionModel);
  tileView->setSelectionModel(selectionModel);
}

// Папки и файлы уже прикреплены: дубли обычны при мульти-дропе,
// вежливого однострочника в статусбаре достаточно вместо модалки
static void showAttachStatus(const QString &message)
{
  if(AppContext::instance().globalParameters()->getStatusBar()!=nullptr)
    AppContext::instance().globalParameters()->getStatusBar()->showMessage(message, 4000);
}

AttachTableController::~AttachTableController()
{
  delete view;
  delete tileView;
  delete model;
}

AttachTableView *AttachTableController::getView(void)
{
  return view;
}

AttachTableTileView *AttachTableController::getTileView(void)
{
  return tileView;
}

// Видимый сейчас вид (таблица или плитка): родитель диалогов,
// ширина и прочие виджетные нужды
QWidget *AttachTableController::currentView(void) const
{
  if(tileView!=nullptr && tileView->isVisible())
    return tileView;

  return view;
}

// Основной метод установки указателя на данные
void AttachTableController::setAttachTableData(AttachTableData *attachTableData)
{
  model->setData(QModelIndex(), QVariant::fromValue(attachTableData), ATTACHTABLE_ROLE_TABLE_DATA);
}

AttachTableData *AttachTableController::getAttachTableData()
{
  return (model->data(QModelIndex(), ATTACHTABLE_ROLE_TABLE_DATA)).value<AttachTableData *>();
}

void AttachTableController::onAddAttach(void)
{
  addSmart("file");
}

void AttachTableController::onAddLink()
{
  addSmart("link");
}

void AttachTableController::onAddAttachFromUrl(void)
{
  // Окно запроса URL
  QInputDialog inputDialog(currentView());

  // Установка ширины окна запроса URL
  int dialogWidth=int(0.9*(float)currentView()->width());
  inputDialog.setMinimumWidth( dialogWidth );
  inputDialog.resize(inputDialog.size());

  // Основные настройки окна запроса URL
  inputDialog.setWindowTitle(tr("Enter file URL"));
  inputDialog.setLabelText(tr("Enter file URL"));
  inputDialog.setTextEchoMode(QLineEdit::Normal);

  // Запуск окна запроса URL
  bool ok=inputDialog.exec();

  // Если при вводе URL была нажата отмена или введено пустой URL
  if(!ok || inputDialog.textValue().trimmed()=="")
    return;

  // Перечень скачиваемых файлов из одного файла
  QStringList downloadReferences=(QStringList() << inputDialog.textValue());

  // Виджет скачивания файлов
  Downloader downloader;
  downloader.setAboutText(tr("Download file"));
  downloader.setDownloadMode(Downloader::disk);
  downloader.setSaveDirectory( QDir::tempPath() );
  downloader.setReferencesList(downloadReferences);

  // Установка ширины для виджета скачивания файлов
  downloader.setMinimumWidth( dialogWidth );
  downloader.resize( downloader.size() );

  // На передний план
  downloader.raise();

  // Запуск виджета скачивания файлов
  downloader.run();

  // Если при скачивании файлов появились какие-то ошибки
  if(!downloader.isSuccess())
  {
    QMessageBox msgBox;
    msgBox.setText(tr("Probably error in a process of files download."));

    if(downloader.getErrorLog().length()>0)
      msgBox.setDetailedText(downloader.getErrorLog());

    msgBox.exec();

    return; // Файл был один, и при ошибке скачивания не происходит сохранения файла
  }

  // Здесь закачка завершена, и надо сохранить файлы как Attach
  QMap<QString, QString> referencesAndFileNames=downloader.getReferencesAndFileNames();

  int sucessLoadCount=0;
  foreach (QString reference, referencesAndFileNames.keys())
  {
    QString shortFileName=referencesAndFileNames.value(reference);
    QString fullFileName=downloader.getSaveDirectory()+"/"+shortFileName;

    QUrl url(reference);
    QString displayName=url.fileName();

    if( addAttach("file", fullFileName, displayName, reference) )
      sucessLoadCount++;

    // Исходный файл в директории закачки удаляется
    QFile(fullFileName).remove();
  }

  // Дерево записей сохраняется
  saveState();
}

void AttachTableController::addSmart(QString attachType)
{
  QStringList files=selectFilesForAdding(attachType);

  if(files.size()==0)
    return; // Если ни один файл не выбран

  addFileList(attachType, files);
}

// Файлы из drag'n'drop: тот же движок что у пикера, но без диалога.
// Несохраненная запись (нет данных таблицы) — вежливый отказ вместо
// критической ошибки: бросать файл некуда
void AttachTableController::addDroppedFiles(const QStringList &files)
{
  if(files.size()==0)
    return;

  if(getAttachTableData()==NULL)
  {
    showMessageBox(tr("Save the note first"));
    return;
  }

  addFileList("file", files);
}

// Один файл из дропа в текст заметки: добавляет как аттач
// и отдает внутреннее имя (для href метки) и видимое имя
// (для текста метки). Добавление идет тем же движком что пикер:
// копия в базу, сохранение дерева, обновление списка в редакторе.
// Дубль (тот же исходник уже строкой): новая копия не заводится,
// отдаются имена существующей строки — вставка меток между заметками
// на этом построена, href перепишется на живой аттач
bool AttachTableController::addSingleFileAsAttach(const QString &filePath,
                                                 QString &innerFileName,
                                                 QString &displayFileName)
{
  innerFileName.clear();
  displayFileName.clear();

  if(filePath.isEmpty())
    return false;

  if(getAttachTableData()==NULL)
  {
    showMessageBox(tr("Save the note first"));
    return false;
  }

  QFileInfo fileInfo(filePath);

  // Папка в текст меткой не вставляется: метка умеет открывать только
  // файлы внутри каталога записи. Папку бросают на панель аттачей,
  // где она добавится ссылкой. Отказ вместо битой метки
  if(fileInfo.isDir())
  {
    showMessageBox(tr("Folders cannot be inserted into note text. Drop the folder onto the attachments panel to add it as a link."));
    return false;
  }

  QString shortFileName=fileInfo.fileName();
  QString sourceFileName=fileInfo.absoluteFilePath();

  if(fileInfo.isSymLink() && !fileInfo.symLinkTarget().isEmpty())
    sourceFileName=fileInfo.symLinkTarget();

  const QString dupId=getAttachTableData()->findFileAttachRowBySource(sourceFileName);

  if(!dupId.isEmpty())
  {
    showAttachStatus(tr("File already attached: %1").arg(shortFileName));
    innerFileName=getAttachTableData()->getAttach(dupId).getExistingInnerFileName();
    displayFileName=getAttachTableData()->getFileNameById(dupId);

    return !innerFileName.isEmpty();
  }

  if(!addAttach(QStringLiteral("file"), filePath, shortFileName, sourceFileName))
    return false;

  saveState();
  updateAttachListInEditor();

  AttachTableData *attachTableData=getAttachTableData();

  if(attachTableData==NULL || attachTableData->size()==0)
    return false;

  // Добавленный аттач в конце списка: addAttach дописывает в хвост
  const int lastRow=attachTableData->size()-1;
  innerFileName=attachTableData->getInnerFileName(lastRow);
  displayFileName=attachTableData->getFileName(lastRow);

  return !innerFileName.isEmpty();
}

// Одна папка из дропа в текст или с панели: строкой-ссылкой
// в таблицу. Возвращает id строки и видимое имя для метки в тексте.
// Пусто в rowId если папка не добавилась: несохраненная запись
// (показывает "Save the note first"), не каталог, ошибка движка
// или отмена диалога режима. Режим ссылки — настройка attachLinkMode,
// при "всегда спрашивать" показывает диалог (там же запоминание)
bool AttachTableController::addSingleFolderAsLink(const QString &dirPath,
                                                  QString &rowId,
                                                  QString &displayName)
{
  rowId.clear();
  displayName.clear();

  if(dirPath.isEmpty())
    return false;

  if(getAttachTableData()==NULL)
  {
    showMessageBox(tr("Save the note first"));
    return false;
  }

  QFileInfo dirInfo(dirPath);

  if(!dirInfo.isDir())
    return false;

  const int linkMode=requestFolderLinkMode();

  // Отмена диалога: молча ничего, метки нет
  if(linkMode!=AppConfig::AttachLinkAbsolute &&
     linkMode!=AppConfig::AttachLinkRelative)
    return false;

  // Дубли, переключение режима и новая строка — внутри.
  // Дерево и список в редакторе там же
  bool addedNew=false;
  rowId=processFolderLink(dirInfo.absoluteFilePath(), linkMode, &addedNew);

  if(rowId.isEmpty())
    return false;

  displayName=getAttachTableData()->getFileNameById(rowId);

  return true;
}


// Режим ссылки для добавляемой папки: настройка attachLinkMode,
// при "всегда спрашивать" — диалог с запоминанием ("всегда"
// пишет настройку). Возвращает AttachLinkAbsolute/Relative,
// при отмене диалога -1
int AttachTableController::requestFolderLinkMode(void)
{
  const int storedMode=AppContext::instance().appConfig()->storageSection()->get_attachLinkMode();

  if(storedMode!=AppConfig::AttachLinkAsk)
    return storedMode;

  QDialog dialog(currentView());
  dialog.setWindowTitle(tr("Attach folder"));

  QLabel *infoLabel=new QLabel(&dialog);
  infoLabel->setWordWrap(true);
  infoLabel->setText(tr("The folder is added to attachments and as a clickable link in note text. Relative links survive database relocation."));

  QRadioButton *onceRelative=new QRadioButton(tr("Relative link"), &dialog);
  QRadioButton *onceAbsolute=new QRadioButton(tr("Absolute link"), &dialog);
  QRadioButton *alwaysRelative=new QRadioButton(tr("Always relative links"), &dialog);
  QRadioButton *alwaysAbsolute=new QRadioButton(tr("Always absolute links"), &dialog);
  onceRelative->setChecked(true);

  QDialogButtonBox *buttons=new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  QVBoxLayout *layout=new QVBoxLayout(&dialog);
  layout->addWidget(infoLabel);
  layout->addWidget(onceRelative);
  layout->addWidget(onceAbsolute);
  layout->addWidget(alwaysRelative);
  layout->addWidget(alwaysAbsolute);
  layout->addWidget(buttons);

  if(dialog.exec()!=QDialog::Accepted)
    return -1;

  int mode=AppConfig::AttachLinkRelative;
  bool remember=false;

  if(onceAbsolute->isChecked())
    mode=AppConfig::AttachLinkAbsolute;
  else if(alwaysRelative->isChecked())
  {
    mode=AppConfig::AttachLinkRelative;
    remember=true;
  }
  else if(alwaysAbsolute->isChecked())
  {
    mode=AppConfig::AttachLinkAbsolute;
    remember=true;
  }

  if(remember)
    AppContext::instance().appConfig()->storageSection()->set_attachLinkMode(mode);

  return mode;
}


// Общая обработка добавляемой папки: поиск дубля по каноническому
// пути, вопрос переключения режима, добавление новой строки.
// wantedMode — резолвленный режим (не Ask). Возвращает id строки:
// новой, существующей (дубль/переключение) или пусто при отказе
// движка. addedNew=true только если строка создана сейчас
QString AttachTableController::processFolderLink(const QString &absolutePath, int wantedMode, bool *addedNew)
{
  if(addedNew!=nullptr)
    *addedNew=false;

  AttachTableData *attachTableData=getAttachTableData();

  if(attachTableData==nullptr || absolutePath.isEmpty())
    return QString();

  if(wantedMode!=AppConfig::AttachLinkAbsolute &&
     wantedMode!=AppConfig::AttachLinkRelative)
    return QString();

  // Дубль: та же папка уже строкой-ссылкой в этой заметке.
  // Папки в других заметках — не дубли, у каждой свой контекст
  const QString existingId=attachTableData->findLinkRowByPath(absolutePath);

  if(!existingId.isEmpty())
  {
    const QString storedLink=attachTableData->getAttach(existingId).getField(QStringLiteral("link"));
    const int oldMode=QFileInfo(storedLink).isAbsolute()
      ? AppConfig::AttachLinkAbsolute
      : AppConfig::AttachLinkRelative;

    // Тот же режим: решать нечего, пропуск с однострочником.
    // Метку при необходимости восстанавливает вызыватель
    if(oldMode==wantedMode)
    {
      showAttachStatus(tr("Folder already attached: %1").arg(attachTableData->getFileNameById(existingId)));
      return existingId;
    }

    // Другой режим: переключить или оставить
    if(askAndSwitchFolderLink(attachTableData->getFileNameById(existingId),
                              oldMode,
                              wantedMode))
    {
      const QString newStoredLink=Attach::makeStoredLink(attachTableData->getRecordDirName(),
                                                         absolutePath,
                                                         wantedMode);

      Attach switchedAttach=attachTableData->getAttach(existingId);
      switchedAttach.setField(QStringLiteral("link"), newStoredLink);

      // Отказ движка (цель пропала между вопросом и записью):
      // строка остается как была
      if(switchedAttach.getField(QStringLiteral("link"))==newStoredLink)
      {
        attachTableData->modifyAttach(existingId, switchedAttach);

        saveState();
        updateAttachListInEditor();
      }
    }

    return existingId;
  }

  // Новая строка-ссылка
  const QFileInfo dirInfo(absolutePath);

  const QString linkPath=Attach::makeStoredLink(attachTableData->getRecordDirName(),
                                                dirInfo.absoluteFilePath(),
                                                wantedMode);

  if(!addAttach(QStringLiteral("link"),
                linkPath,
                dirInfo.fileName(),
                dirInfo.absoluteFilePath()))
    return QString();

  saveState();
  updateAttachListInEditor();

  if(attachTableData->size()==0)
    return QString();

  // Добавленная строка в хвосте: addAttach дописывает в конец
  const int lastRow=attachTableData->size()-1;
  const QString newRowId=attachTableData->getIdByRow(lastRow);

  if(newRowId.isEmpty())
    return QString();

  if(addedNew!=nullptr)
    *addedNew=true;

  return newRowId;
}


// Папка уже прикреплена другим режимом: спросить переключить
// на новый или оставить. Возвращает true если переключили.
// Отмена и "оставить" — false, строка не трогается
bool AttachTableController::askAndSwitchFolderLink(const QString &folderName,
                                                   int oldMode,
                                                   int newMode)
{
  const QString oldName=(oldMode==AppConfig::AttachLinkAbsolute)
    ? tr("Absolute link") : tr("Relative link");
  const QString newName=(newMode==AppConfig::AttachLinkAbsolute)
    ? tr("Absolute link") : tr("Relative link");

  QMessageBox msgBox(currentView());
  msgBox.setWindowTitle(tr("Folder already attached"));
  msgBox.setText(tr("Folder \"%1\" is already attached as %2.").arg(folderName).arg(oldName));

  QPushButton *switchButton=msgBox.addButton(tr("Switch to %1").arg(newName),
                                             QMessageBox::AcceptRole);
  QPushButton *keepButton=msgBox.addButton(tr("Keep as %1").arg(oldName),
                                           QMessageBox::RejectRole);
  msgBox.addButton(QMessageBox::Cancel);
  msgBox.setDefaultButton(keepButton);
  msgBox.exec();

  return msgBox.clickedButton()==switchButton;
}


// Открытая в редакторе заметка — та же что таблица аттачей.
// Метки в текст трогаются только при совпадении каталогов,
// иначе метка ушла бы в чужой текст
bool AttachTableController::isMetaEditorSameNote(void)
{
  AttachTableData *attachTableData=getAttachTableData();

  if(metaEditor==nullptr || attachTableData==nullptr)
    return false;

  const QString tableDir=QDir::cleanPath(QDir(attachTableData->getRecordDirName()).absolutePath());
  const QString editorDir=QDir::cleanPath(QDir(metaEditor->getWorkDirectory()).absolutePath());

  return !tableDir.isEmpty() && tableDir==editorDir;
}




// Удаление аттача по id строки для движка меток папок:
// строка удаляется, дерево сохраняется, иконка/статистика
// обновляются. Пустой id или нет строки: false
bool AttachTableController::deleteAttachById(const QString &rowId)
{
  AttachTableData *attachTableData=getAttachTableData();

  if(attachTableData==nullptr || rowId.isEmpty())
    return false;

  if(attachTableData->getRowById(rowId)<0)
    return false;

  attachTableData->deleteAttach(rowId);

  saveState();

  return true;
}


// Удаление аттача по внутреннему имени для движка переноса метки
bool AttachTableController::removeAttachByInnerName(const QString &innerName)
{
  AttachTableData *attachTableData=getAttachTableData();

  if(attachTableData==nullptr || innerName.isEmpty())
    return false;

  // Строка ищется по прямому имени и по реально лежащему файлу:
  // режим имен могли сменить после вставки метки, коллизия
  // оригинала могла увести файл в гибридную схему
  for(int row=0; row<attachTableData->size(); ++row)
  {
    if(attachTableData->getInnerFileName(row)!=innerName &&
       attachTableData->getAttach(attachTableData->getIdByRow(row)).getExistingInnerFileName()!=innerName)
      continue;

    attachTableData->deleteAttach(attachTableData->getIdByRow(row));

    saveState();

    return true;
  }

  return false;
}

void AttachTableController::addFileList(QString attachType, const QStringList &files)
{
  // Перебираются файлы (из диалога или из drag'n'drop)
  int sucessLoadCount=0;

  // Режим ссылок для папок пачки: при "всегда спрашивать" диалог
  // один на все папки. Отмена пропускает папки, файлы идут как обычно
  int dirMode=AppContext::instance().appConfig()->storageSection()->get_attachLinkMode();

  if(dirMode==AppConfig::AttachLinkAsk)
    dirMode=requestFolderLinkMode();

  // Добавленные папки для меток в текст: пары id строки и имени
  QList<QPair<QString, QString>> addedFolders;

  for(int i=0; i<files.size(); ++i)
  {
    // Текущее полное имя файла
    QString currFullFileName=files.at(i);

    QFileInfo currFileInfo(currFullFileName);

    // Директория добавляется ссылкой (без копирования в базу):
    // открывается в файловом менеджере как обычный линк.
    // Файлы ниже идут прежним путем с копированием в базу.
    // В поле link кладется абсолютный путь или относительный
    // каталога записи (настройка attachLinkMode или диалог):
    // относительные живут при переезде базы. В source для истории —
    // всегда исходный. Плюс кликабельная метка в текст заметки.
    // Дубли (та же папка уже строкой) разруливает processFolderLink:
    // пропуск, переключение режима или новая строка
    if(currFileInfo.isDir())
    {
        // Диалог отменен: папки пропускаются
        if(dirMode!=AppConfig::AttachLinkAbsolute &&
           dirMode!=AppConfig::AttachLinkRelative)
          continue;

        bool addedNew=false;
        const QString rowId=processFolderLink(currFileInfo.absoluteFilePath(),
                                              dirMode,
                                              &addedNew);

        // Отказ движка: папки нет
        if(rowId.isEmpty())
          continue;

        sucessLoadCount++;

        // Новая или восстановленная строка: метка в текст, если ее
        // там еще нет. Дубли с живой меткой повторно не вставляются
        if(isMetaEditorSameNote() &&
           !metaEditor->hasBadgeHref(ReferenceFormatter::folderHrefFromRowId(rowId)))
          addedFolders.append(qMakePair(rowId, getAttachTableData()->getFileNameById(rowId)));

        continue;
    }

    qDebug() << "Select file from disk: " << currFullFileName;

    // Текущее короткое имя файла (имя файла без пути но с расширением или несколькими расширениями если их несколько)
    QString currShortFileName=currFileInfo.fileName();

    // Добавляется файл аттача. Происхождение: цель симлинка, иначе
    // полный исходный путь (потом видно в колонке "Ссылка")
    QString currSourceFileName=currFileInfo.absoluteFilePath();

    if(currFileInfo.isSymLink() && !currFileInfo.symLinkTarget().isEmpty())
      currSourceFileName=currFileInfo.symLinkTarget();

    // Дубль файла: тот же исходник уже строкой в этой заметке.
    // Копии — снапшоты, но повторный выбор того же файла почти всегда
    // случаен: пропуск с однострочником вместо второй копии.
    // Файлы в других заметках — не дубли, у каждой свой контекст
    const QString dupFileId=getAttachTableData()->findFileAttachRowBySource(currSourceFileName);

    if(!dupFileId.isEmpty())
    {
      showAttachStatus(tr("File already attached: %1").arg(currShortFileName));
      continue;
    }

    // Добавляется файл аттача
    if( addAttach(attachType, currFullFileName, currShortFileName, currSourceFileName) )
      sucessLoadCount++;
    else
      break;
  } // Закончился цикл перебора файлов

  // Дерево записей сохраняется
  saveState();

  // В редакторе обновляется список приаттаченных файлов
  updateAttachListInEditor();

  // Метки добавленных папок в конец открытого текста той же заметки.
  // Чужая заметка и только чтение отсекаются внутри appendFolderBadge,
  // совпадение заметки — здесь: иначе метка ушла бы в чужой текст
  if(!addedFolders.isEmpty() && isMetaEditorSameNote())
    for(int n=0; n<addedFolders.size(); ++n)
      metaEditor->appendFolderBadge(addedFolders.at(n).first, addedFolders.at(n).second);
}

bool AttachTableController::addAttach(QString attachType, QString currFullFileName, QString currShortFileName, const QString &source)
{
  // Выясняется указатель на данные таблицы приаттаченных файлов
  AttachTableData *attachTableData=getAttachTableData();
  if(attachTableData==NULL)
    criticalError("Unset attach table data in AttachTableController::addAttach()");

  // Конструируется Attach, который нужно добавить
  Attach attach(attachType, attachTableData);
  attach.setField("id", getUniqueId());
  attach.setField("fileName", currShortFileName);

  // Происхождение файла (URL загрузки, цель симлинка, исходный путь):
  // показывается в колонке "Ссылка". У линка источником служит он сам
  if(!source.isEmpty())
    attach.setField("source", source);

  bool result=false;
  if(attachType=="file")
    result=attach.copyFileToBase(currFullFileName); // Файл аттача копируется в базу
  else if(attachType=="link")
  {
    attach.setField("link", currFullFileName); // Запоминается куда указывает линк
    result=true;
  }
  else
    criticalError("Unsupport adding mode");

  // Если запись, к которой добавляется аттач, зашифрована.
  // Только при успехе копирования (fix/543): раньше encrypt() до
  // проверки result ронял процесс criticalError на битой копии
  // вместо вежливого showMessageBox
  if(result && attachTableData->isRecordCrypt())
    attach.encrypt();

  if(result)
  {
    // Данные аттача добавляются в таблицу приаттаченных файлов
    attachTableData->addAttach(attach);
    return true;
  }
  else
  {
    showMessageBox(tr("Error copying file(s). Unable to attach file(s)."));
    return false;
  }
}

void AttachTableController::saveState()
{
  // Сохранение дерева веток
  TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
  treeScreen->saveKnowTree();

  // Указатель на данные таблицы приаттаченных файлов, чтобы обновить иконку аттачей в редакторе
  AttachTableData *attachTableData=getAttachTableData();
  if(attachTableData==NULL)
    criticalError("Unset attach table data in AttachTableController::addAttach()");

  if(attachTableData->size()>0)
    metaEditor->switchAttachIconExists(true);
  else
    metaEditor->switchAttachIconExists(false);

  // Файлы приехали: строка записи в кеше статистики без полного прохода
  if(metaEditor!=nullptr)
    treeScreen->knowTreeModel->refreshRecordStats(metaEditor->getMiscField("id"));

  // Число файлов изменилось - освежить статистику в статусбаре
  MainWindow *mainWindow=find_object<MainWindow>("mainwindow");

  if(mainWindow!=nullptr)
    mainWindow->updateStats();
}

QStringList AttachTableController::selectFilesForAdding(QString attachType)
{
  // Диалог выбора файлов
  QFileDialog fileSelectDialog;

  QString title;
  if(attachType=="file")
    title=tr("Attach file");
  if(attachType=="link")
    title=tr("Add link to file");

  fileSelectDialog.setWindowTitle(title);

  fileSelectDialog.setFileMode(QFileDialog::ExistingFiles); // QFileDialog::ExistingFile
  fileSelectDialog.setNameFilter("*");
  fileSelectDialog.setDirectory(QDir::homePath());

  // Если существует каталог, открытый при предыдущем выборе файла
  QDir appendDir( AppContext::instance().appConfig()->storageSection()->getAttachAppendDir() );
  if( appendDir.exists() )
    fileSelectDialog.setDirectory( appendDir );

  // Отрисовка диалога выбора
  int dialogResult=fileSelectDialog.exec();

  // Если была нажата отмена
  if(dialogResult==QDialog::Rejected)
    return QStringList();

  // Выясняется список выбранных файлов
  QStringList files=fileSelectDialog.selectedFiles();

  // Запоминается директория, в которой был сделан выбор
  if(files.size()>0)
    AppContext::instance().appConfig()->storageSection()->setAttachAppendDir( fileSelectDialog.directory().absolutePath() );

  return files;
}

// Выбрано действие "Сохранить как..."
void AttachTableController::onSaveAsAttach(void)
{
  QList<QString> selectedId=getSelectedId();

  // Если ни один аттач не выбран
  if(selectedId.size()==0)
  {
    showMessageBox(tr("Please, select at least one attached file to save."));
    return;
  }

  // Если выбран только один аттач
  if(selectedId.size()==1)
  {
    // Диалог выбора имени файла
    QFileDialog fileSelectDialog;
    fileSelectDialog.setFileMode(QFileDialog::ExistingFile); // Один файл
    fileSelectDialog.setWindowTitle(tr("Save as..."));
    fileSelectDialog.setDirectory(QDir::homePath());
    fileSelectDialog.setAcceptMode(QFileDialog::AcceptSave); // Чтобы была кнока "Сохранить" а не "Выбрать"
    fileSelectDialog.setFileMode(QFileDialog::AnyFile); // Чтобы кнопка "Сохранить" была активной

    // Если существует каталог, открытый при предыдущем сохранении
    QDir saveAsDir( AppContext::instance().appConfig()->storageSection()->getAttachSaveAsDir() );
    if( saveAsDir.exists() )
      fileSelectDialog.setDirectory( saveAsDir );

    // В диалоге устанавливается имя файла выбранного аттача
    QString id=selectedId.at(0);
    AttachTableData *attachTableData=getAttachTableData();
    QString attachType=attachTableData->getAttach(id).getField("type");
    QString fileName=attachTableData->getFileNameById(id);
    QString fullFileName=attachTableData->getAbsoluteInnerFileNameById(id);
    fileSelectDialog.selectFile(fileName);

    // Отрисовка диалога выбора
    int dialogResult=fileSelectDialog.exec();
    if(dialogResult==QDialog::Rejected)
      return;

    // Запоминается директория, в которой был сделан выбор
    AppContext::instance().appConfig()->storageSection()->setAttachSaveAsDir( fileSelectDialog.directory().absolutePath() );

    // Выясняется список выбранных файлов
    QStringList selectFiles=fileSelectDialog.selectedFiles();

    // Должен быть выбран только один файл
    if(selectFiles.size()!=1)
    {
      showMessageBox(tr("To save single file you must set single result file name."));
      return;
    }

    // Указанное пользователем имя файла для сохранения аттача, взятое из формы ввода
    QString targetFileName=selectFiles.at(0);

    // Непосредственное сохранение файла
    saveAttachToUserPlace(fullFileName, targetFileName, attachType, attachTableData->isRecordCrypt());
  }
  else if(selectedId.size()>1) // Если выбрано несколько аттачей
  {
    // Диалог выбора имени директории
    QFileDialog fileSelectDialog;
    fileSelectDialog.setFileMode(QFileDialog::DirectoryOnly); // Выбор директории
    fileSelectDialog.setWindowTitle(tr("Save attaches to directory..."));
    fileSelectDialog.setDirectory(QDir::homePath());

    // Если существует каталог, открытый при предыдущем сохранении
    QDir saveAsDir( AppContext::instance().appConfig()->storageSection()->getAttachSaveAsDir() );
    if( saveAsDir.exists() )
      fileSelectDialog.setDirectory( saveAsDir );

    // Отрисовка диалога выбора
    int dialogResult=fileSelectDialog.exec();
    if(dialogResult==QDialog::Rejected)
      return;

    // Запоминается директория, в которой был сделан выбор
    AppContext::instance().appConfig()->storageSection()->setAttachSaveAsDir( fileSelectDialog.directory().absolutePath() );

    // Выбранная директория
    QString toDir=fileSelectDialog.directory().absolutePath();

    AttachTableData *attachTableData=getAttachTableData();

    // Перебор выбранных для сохранения аттачей
    foreach(QString id, selectedId)
    {
      QString attachType=attachTableData->getAttach(id).getField("type");
      QString fileName=attachTableData->getFileNameById(id);
      QString fromFileName=attachTableData->getInnerFileNameById(id);
      QString fromFullFileName=attachTableData->getAbsoluteInnerFileNameById(id);
      QString toFullFileName=toDir+"/"+fileName;

      // Непосредственное сохранение файла
      saveAttachToUserPlace(fromFullFileName, toFullFileName, attachType, attachTableData->isRecordCrypt());
    }
  }
}

void AttachTableController::saveAttachToUserPlace(QString fromFullFileName, QString toFullFileName, QString attachType, bool isAttachCrypt)
{
  // Проверка наличия исходного файла (ведь по каким-то причинам его может не быть, например после какого-нибудь сбоя)
  QFile file(fromFullFileName);

  if(file.exists()==false)
  {
    showMessageBox(tr("Unable to save the file: file %1 not found in the database.").arg(fromFullFileName));
    return;
  }

  // Сохранение
  bool result=file.copy( toFullFileName );

  if(!result)
  {
    showMessageBox(tr("Unable to save the file: file %1 input/output error.").arg(toFullFileName));
    return;
  }

  // Расшифровка файла, если он был зашифрован и данные хранились в базе (то есть, это именно тип file, а не линк на файл)
  if(isAttachCrypt && attachType=="file")
    CryptService::decryptFile(AppContext::instance().globalParameters()->getCryptKey(), toFullFileName);
}

void AttachTableController::onEditFileName(void)
{
  QList<QString> selectedId=getSelectedId();

  // Если ни один аттач не выбран
  if(selectedId.size()==0)
    return;

  // Если выбрано больше одного аттача
  if(selectedId.size()>1)
  {
    showMessageBox(tr("Please select single attach for edit."));
    return;
  }

  // Имя файла выбранного аттача
  QString id=selectedId.at(0);
  AttachTableData *attachTableData=getAttachTableData();
  QString fileName=attachTableData->getFileNameById(id);

  // Запрос нового имени файла
  bool isOk;
  QString newFileName = QInputDialog::getText(currentView(),
                                              tr("File name editing"),
                                              tr("File name:"),
                                              QLineEdit::Normal,
                                              fileName,
                                              &isOk);

  if(!isOk)
    return; // Была нажата кнопка Cancel

  if(newFileName.size()==0)
  {
    showMessageBox(tr("Cant save file with empty name."));
    return;
  }

  // Данные изменяются
  Attach tempAttach=attachTableData->getAttach(id);
  tempAttach.renameFile(newFileName);
  attachTableData->modifyAttach(id, tempAttach);

  // Сохранение дерева веток
  TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
  treeScreen->saveKnowTree();

  // Расширение могло смениться: строка записи в кеше без полного прохода
  if(metaEditor!=nullptr)
    treeScreen->knowTreeModel->refreshRecordStats(metaEditor->getMiscField("id"));
}

void AttachTableController::onDeleteAttach(void)
{
  QList<QString> selectedId=getSelectedId();

  // Если ни один аттач не выбран
  if(selectedId.size()==0)
  {
    showMessageBox(tr("Please select any attach(es) for delete."));
    return;
  }

  // Запрос подтверждения об удалении
  QMessageBox msgBox;
  msgBox.setText(tr("Do you want to delete attached file(s)?"));
  msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
  msgBox.setDefaultButton(QMessageBox::Cancel);
  int ret = msgBox.exec();

  if(ret!=QMessageBox::Ok)
    return;

  // Удаление выбранных аттачей
  AttachTableData *attachTableData=getAttachTableData();

  // Id удаляемых папок-ссылок: их метки в тексте спросить отдельно
  QStringList deletedLinkIds;

  foreach( QString id, selectedId )
  {
    if(attachTableData->getAttach(id).getField(QStringLiteral("type"))==QStringLiteral("link"))
      deletedLinkIds << id;

    attachTableData->deleteAttach(id);
  }

  // Сохранение дерева веток
  TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
  treeScreen->saveKnowTree();

  // Обновление иконки аттачей в редакторе
  if(attachTableData->size()==0)
    metaEditor->switchAttachIconExists(false);

  // Число файлов изменилось: строка записи в кеше без полного прохода
  if(metaEditor!=nullptr)
    treeScreen->knowTreeModel->refreshRecordStats(metaEditor->getMiscField("id"));

  // Число файлов изменилось - освежить статистику в статусбаре
  MainWindow *mainWindow=find_object<MainWindow>("mainwindow");

  if(mainWindow!=nullptr)
    mainWindow->updateStats();

  // Метки удаленных папок в открытом тексте той же заметки:
  // спросить, убрать ли и ссылку. Закрытая заметка: метка останется
  // мертвой и честно сообщит об этом по клику
  if(!deletedLinkIds.isEmpty() && isMetaEditorSameNote())
    foreach(const QString &linkId, deletedLinkIds)
    {
      const QString href=ReferenceFormatter::folderHrefFromRowId(linkId);

      if(!metaEditor->hasBadgeHref(href))
        continue;

      if(QMessageBox::question(currentView(),
                               tr("Delete folder link"),
                               tr("Remove the folder link from note text as well?"),
                               QMessageBox::Yes | QMessageBox::No,
                               QMessageBox::Yes)==QMessageBox::Yes)
        metaEditor->removeBadgeByHref(href);
    }
}

// Открытие аттача (аттачей) на просмотр
void AttachTableController::onOpenAttach(void)
{
  AttachTableData *attachTableData=getAttachTableData();

  QList<QString> selectedId=getSelectedId();

  foreach( QString id, selectedId )
  {
    QString fullFileName=attachTableData->getAbsoluteInnerFileNameById(id);

    // Если запись зашифрована и открывается файл (не линк), аттач надо скопировать в директорию корзины и расшифровать
    if( attachTableData->isRecordCrypt() && attachTableData->getAttach(id).getField("type")=="file")
    {
      if(AppContext::instance().appConfig()->storageSection()->getEnableDecryptFileToTrashDirectory())
      {
        fullFileName=DiskHelper::copyFileToTrash(fullFileName); // Копирование
        CryptService::decryptFile(AppContext::instance().globalParameters()->getCryptKey(), fullFileName); // Расшифровка
      }
      else
      {
        showMessageBox(tr("Unable to preview the encrypted attached file %1.\nPlease, use «Save As...» button, or\nenable «Decrypt to a temporary file» option in Settings.").arg(fullFileName));
        continue;
      }
    }

    qDebug() << "Open file: "+fullFileName;

    // Открытие файла средствами операционной системы
    QUrl urlFile("file:"+fullFileName);
    QDesktopServices::openUrl(urlFile);
  }
}

void AttachTableController::onShowAttachInfo(void)
{
  QList<QString> selectedId=getSelectedId();

  // Если ни один аттач не выбран
  if(selectedId.size()==0)
    return;

  // Если выбрано больше одного аттача
  if(selectedId.size()>1)
  {
    showMessageBox(tr("Please, select a single file to see information about one."));
    return;
  }

  QString id=selectedId.at(0);
  AttachTableData *attachTableData=getAttachTableData();

  ReduceMessageBox messageBox;
  messageBox.setText("Attach file info");
  messageBox.setDetailedText(QString("<pre><p style='font-family:monospace'>")+\
                             "<b>Attach:</b> "+attachTableData->getFileNameById(id)+"\n"+\
                             "<b>Attach ID:</b> "+id+"\n"+\
                             "<b>Attach type:</b> "+attachTableData->getAttach(id).getField("type")+"\n"+\
                             "<b>File size:</b> "+DiskHelper::formatFileSize( attachTableData->getAttach(id).getFileSize() )+"\n"+\
                             "<b>Full path:</b> "+attachTableData->getAbsoluteInnerFileNameById(id)+\
                             "</p></pre>"
                             );
  messageBox.setDetailedTextReadOnly(true);
  messageBox.setStandardButtons(QDialogButtonBox::Ok); // Для двух кнопок можно так: QDialogButtonBox::Ok | QDialogButtonBox::Cancel

  // int result=messageBox.exec();
  // qDebug() << "Result code: " << result;
  messageBox.exec();
}

void AttachTableController::onSwitchToEditor(void)
{
    updateAttachListInEditor();

    metaEditor->switchToEditorLayout();
}

void AttachTableController::updateAttachListInEditor(void)
{
    // В редакторе обновляется информация о приаттаченных к записи файлах
    metaEditor->setMiscField("attachFileNameList", getAttachTableData()->getInnerFileNameOnDiskList().join(","));
    qDebug() << "Set attach file name list to editor: " << metaEditor->getMiscField("attachFileNameList");
}

// Получение списка идентификаторов аттачей, выделенных в представлении
QList<QString> AttachTableController::getSelectedId(void)
{
  QList<QString> selectedId;

  // Получение списка выделенных в таблице на экране Item-элементов
  // (выделение общее для таблицы и плитки)
  QModelIndexList selectItems=selectionModel->selectedIndexes();

  // Перебор выделенных элементов. Так как имеем несколько столбцов, то для одной строки будет несколько QModelIndex.
  // Порядок selectedIndexes не гарантирован: строки сортируются,
  // иначе previousRow-фильтр пропускал дубли и терял строки (fix/547)
  std::sort(selectItems.begin(), selectItems.end(),
            [](const QModelIndex &a, const QModelIndex &b) {
              if(a.row()!=b.row())
                return a.row()<b.row();
              return a.column()<b.column();
            });

  int previousRow=-1;
  for(int i=0; i<selectItems.size(); i++)
  {
    int row=selectItems.at(i).row();

    // Строка обратабывается только один раз (из-за того что для одной строки несколько QModelIndex)
    if(row!=previousRow)
    {
      previousRow=row;

      selectedId.append( model->data(selectItems.at(i), ATTACHTABLE_ROLE_ID).toString() );
    }
  }

  return selectedId;
}
