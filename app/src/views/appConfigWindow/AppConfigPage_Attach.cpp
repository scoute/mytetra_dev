#include <QWidget>
#include <QColorDialog>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include "libraries/MtComboBox.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "AppConfigPage_Attach.h"
#include "models/appConfig/AppConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "models/recordTable/Record.h"
#include "models/attachTable/Attach.h"
#include "models/attachTable/AttachTableData.h"
#include "views/tree/TreeScreen.h"
#include "views/record/MetaEditor.h"
#include "libraries/wyedit/EditorShowTextDispatcher.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/AppContext.h"
#include "models/appConfig/AppConfigUi.h"
#include "models/appConfig/AppConfigStorage.h"

AppConfigPage_Attach::AppConfigPage_Attach(QWidget *parent) : ConfigPage(parent)
{
    setupUi();
    setupSignals();
    assembly();
}

AppConfigPage_Attach::~AppConfigPage_Attach()
{
    qDebug() << Q_FUNC_INFO;
}

void AppConfigPage_Attach::setupUi(void)
{
    qDebug() << "Create attach config page";

    // Галка разрешения/запрещения подсветки записи с прикрепленными файлами
    enableRecordWithAttachHighlight=new QCheckBox(this);
    enableRecordWithAttachHighlight->setText( tr("Enable highlight notes with attachments") );
    enableRecordWithAttachHighlight->setChecked( AppContext::instance().appConfig()->uiSection()->getEnableRecordWithAttachHighlight() );

    // Выбор цвета для записи с прикрепленными файлами
    labelHighlightColor=new QLabel(this);
    labelHighlightColor->setText( tr("Highlight color: ") );

    buttonHighlightColor=new QToolButton(this);
    highlightColor=new QColor();
    this->setColorForButtonHighlightColor( QColor( AppContext::instance().appConfig()->uiSection()->getRecordWithAttachHighlightColor() ) );

    // Начальный выбор цвета активируется или отключается в зависимости от галки
    this->onEnableRecordWithAttachHighlight( AppContext::instance().appConfig()->uiSection()->getEnableRecordWithAttachHighlight() );

    // Пояснение про схему имен файлов на диске
    fileNamesInfo=new QLabel(this);
    fileNamesInfo->setWordWrap(true);
    fileNamesInfo->setText( tr("File naming for uploaded attachments: system names (id.extension), system + original names (hybrid, id_name.extension), or original names (as uploaded). The choice applies to new attachments.") );

    // Выбор режима имен: системные, гибридные, оригинальные
    attachNameModeLabel=new QLabel(this);
    attachNameModeLabel->setText( tr("Name new attachments: ") );

    attachNameModeCombo=new MtComboBox(this);
    attachNameModeCombo->addItem( tr("System names (id.extension)"), AppConfig::AttachNameSystem );
    attachNameModeCombo->addItem( tr("System + original names (id_name.extension)"), AppConfig::AttachNameHybrid );
    attachNameModeCombo->addItem( tr("Original names (as uploaded)"), AppConfig::AttachNameOriginal );
    attachNameModeCombo->setCurrentIndex( attachNameModeCombo->findData(AppContext::instance().appConfig()->storageSection()->get_attachNameMode()) );

    // Кнопка приведения всех вложений к выбранному виду
    convertAttachesButton=new QPushButton(this);
    convertAttachesButton->setText( tr("Convert attachments to the selected form") );

    // Предупреждение: действие опасное и пока в бета-версии
    fileNamesWarning=new QLabel(this);
    fileNamesWarning->setWordWrap(true);
    fileNamesWarning->setText( tr("Danger, beta version: back up your database directory before converting. If the database is encrypted, unlock it first. With original names, files with identical names in one note conflict and are skipped.") );

    // Пояснение про схему имен каталогов записей на диске
    recordDirsInfo=new QLabel(this);
    recordDirsInfo->setWordWrap(true);
    recordDirsInfo->setText( tr("Record directory naming in base/: system names (id, as in stock MyTetra) or system + note names (hybrid, id_name). The choice applies to new notes and note renames.") );

    // Выбор режима имен: системные, гибридные
    recordDirNameModeLabel=new QLabel(this);
    recordDirNameModeLabel->setText( tr("Name new record directories: ") );

    recordDirNameModeCombo=new MtComboBox(this);
    recordDirNameModeCombo->addItem( tr("System names (id)"), AppConfig::RecordDirSystem );
    recordDirNameModeCombo->addItem( tr("System + note names (id_name)"), AppConfig::RecordDirHybrid );
    recordDirNameModeCombo->setCurrentIndex( recordDirNameModeCombo->findData(AppContext::instance().appConfig()->storageSection()->get_recordDirNameMode()) );

    // Кнопка приведения всех каталогов записей к выбранному виду
    convertRecordDirsButton=new QPushButton(this);
    convertRecordDirsButton->setText( tr("Convert record directories to the selected form") );

    // Предупреждение: действие опасное и пока в бета-версии
    recordDirsWarning=new QLabel(this);
    recordDirsWarning->setWordWrap(true);
    recordDirsWarning->setText( tr("Danger, beta version: back up your database directory before converting. If the database is encrypted, unlock it first. The tree will be saved with new directory names.") );

    // Пояснение про ссылки на папки и файлы
    folderLinksInfo=new QLabel(this);
    folderLinksInfo->setWordWrap(true);
    folderLinksInfo->setText( tr("Folders and files attach as links without copying; folders also get a clickable link in note text. Absolute paths break when the database moves; relative paths are stored from the note directory and survive relocation. \"Always ask\" shows a choice dialog for every folder. The choice applies to new links. Convert links only after decryption (database unlocked), otherwise the database ends up out of sync.") );

    // Выбор вида пути: спрашивать, абсолютный или относительный каталога записи
    attachLinkModeLabel=new QLabel(this);
    attachLinkModeLabel->setText( tr("Attach folders as: ") );

    attachLinkModeCombo=new MtComboBox(this);
    attachLinkModeCombo->addItem( tr("Always ask"), AppConfig::AttachLinkAsk );
    attachLinkModeCombo->addItem( tr("Relative to note directory"), AppConfig::AttachLinkRelative );
    attachLinkModeCombo->addItem( tr("Absolute path"), AppConfig::AttachLinkAbsolute );
    attachLinkModeCombo->setCurrentIndex( attachLinkModeCombo->findData(AppContext::instance().appConfig()->storageSection()->get_attachLinkMode()) );

    // Конвертер хранимых путей в выбранный вид
    convertFolderLinksButton=new QPushButton(this);
    convertFolderLinksButton->setText( tr("Convert folder and file links to the selected form") );
}

void AppConfigPage_Attach::setupSignals(void)
{
    connect( enableRecordWithAttachHighlight, &QCheckBox::toggled, this, &AppConfigPage_Attach::onEnableRecordWithAttachHighlight);
    connect( buttonHighlightColor, &QToolButton::clicked, this, &AppConfigPage_Attach::onClickedHighlightColor);
    connect( convertAttachesButton, &QPushButton::clicked, this, &AppConfigPage_Attach::onClickedConvertAttaches);
    connect( convertRecordDirsButton, &QPushButton::clicked, this, &AppConfigPage_Attach::onClickedConvertRecordDirs);
    connect( convertFolderLinksButton, &QPushButton::clicked, this, &AppConfigPage_Attach::onClickedConvertFolderLinks);
}

void AppConfigPage_Attach::assembly(void)
{
    // Слой для надписи выбора цвета и кнопки выбора цвета
    QHBoxLayout *colorLayout = new QHBoxLayout;
    colorLayout->addWidget( labelHighlightColor );
    colorLayout->addWidget( buttonHighlightColor );
    colorLayout->addStretch();

    // Слой для рамки
    QVBoxLayout *decorBoxlayout = new QVBoxLayout;
    decorBoxlayout->addWidget( enableRecordWithAttachHighlight );
    decorBoxlayout->addLayout( colorLayout );

    // Рамка
    decorBox=new QGroupBox(this);
    decorBox->setTitle( tr("Displaying notes with attachments") );
    decorBox->setLayout( decorBoxlayout );

    // Рамка имен файлов на диске
    QHBoxLayout *modeLayout=new QHBoxLayout;
    modeLayout->addWidget( attachNameModeLabel );
    modeLayout->addWidget( attachNameModeCombo );
    modeLayout->addStretch();

    QVBoxLayout *fileNamesBoxLayout=new QVBoxLayout;
    fileNamesBoxLayout->addWidget( fileNamesInfo );
    fileNamesBoxLayout->addLayout( modeLayout );
    fileNamesBoxLayout->addWidget( convertAttachesButton );
    fileNamesBoxLayout->addWidget( fileNamesWarning );

    fileNamesBox=new QGroupBox(this);
    fileNamesBox->setTitle( tr("File names on disk") );
    fileNamesBox->setLayout( fileNamesBoxLayout );

    // Рамка имен каталогов записей на диске
    QHBoxLayout *dirModeLayout=new QHBoxLayout;
    dirModeLayout->addWidget( recordDirNameModeLabel );
    dirModeLayout->addWidget( recordDirNameModeCombo );
    dirModeLayout->addStretch();

    // Рамка ссылок на папки
    QHBoxLayout *linkModeLayout=new QHBoxLayout;
    linkModeLayout->addWidget( attachLinkModeLabel );
    linkModeLayout->addWidget( attachLinkModeCombo );
    linkModeLayout->addStretch();

    QVBoxLayout *folderLinksBoxLayout=new QVBoxLayout;
    folderLinksBoxLayout->addWidget( folderLinksInfo );
    folderLinksBoxLayout->addLayout( linkModeLayout );
    folderLinksBoxLayout->addWidget( convertFolderLinksButton );

    folderLinksBox=new QGroupBox(this);
    folderLinksBox->setTitle( tr("Folder links") );
    folderLinksBox->setLayout( folderLinksBoxLayout );

    QVBoxLayout *recordDirsBoxLayout=new QVBoxLayout;
    recordDirsBoxLayout->addWidget( recordDirsInfo );
    recordDirsBoxLayout->addLayout( dirModeLayout );
    recordDirsBoxLayout->addWidget( convertRecordDirsButton );
    recordDirsBoxLayout->addWidget( recordDirsWarning );

    recordDirsBox=new QGroupBox(this);
    recordDirsBox->setTitle( tr("Record directories on disk") );
    recordDirsBox->setLayout( recordDirsBoxLayout );

    // Собирается основной слой
    QVBoxLayout *centralLayout=new QVBoxLayout();
    centralLayout->addWidget( decorBox );
    centralLayout->addWidget( fileNamesBox );
    centralLayout->addWidget( recordDirsBox );
    centralLayout->addWidget( folderLinksBox );
    centralLayout->addStretch();

    // Основной слой устанавливается
    setLayout( centralLayout );
}

void AppConfigPage_Attach::setColorForButtonHighlightColor(QColor iColor)
{
    // Квадратик на кнопке выбора цвета кода
    QPixmap pix(16, 16);
    pix.fill( iColor.rgb() );
    buttonHighlightColor->setIcon(pix);

    *highlightColor=iColor;
}

void AppConfigPage_Attach::onEnableRecordWithAttachHighlight(bool state)
{
    labelHighlightColor->setEnabled(state);
    buttonHighlightColor->setEnabled(state);
}

void AppConfigPage_Attach::onClickedHighlightColor()
{
    // Диалог запроса цвета
    QColor selectedColor=QColorDialog::getColor( *highlightColor, this, QString(), QColorDialog::ShowAlphaChannel);

    // Если цвет выбран, и он правильный
    if(selectedColor.isValid())
        setColorForButtonHighlightColor(selectedColor);
}

void AppConfigPage_Attach::retranslateUi(void)
{
  enableRecordWithAttachHighlight->setText(tr("Enable highlight notes with attachments"));
  labelHighlightColor->setText(tr("Highlight color: "));
  decorBox->setTitle(tr("Displaying notes with attachments"));
  fileNamesBox->setTitle(tr("File names on disk"));
  fileNamesInfo->setText(tr("File naming for uploaded attachments: system names (id.extension), system + original names (hybrid, id_name.extension), or original names (as uploaded). The choice applies to new attachments."));
  attachNameModeLabel->setText(tr("Name new attachments: "));
  attachNameModeCombo->setItemText(0, tr("System names (id.extension)"));
  attachNameModeCombo->setItemText(1, tr("System + original names (id_name.extension)"));
  attachNameModeCombo->setItemText(2, tr("Original names (as uploaded)"));
  convertAttachesButton->setText(tr("Convert attachments to the selected form"));
  fileNamesWarning->setText(tr("Danger, beta version: back up your database directory before converting. If the database is encrypted, unlock it first. With original names, files with identical names in one note conflict and are skipped."));
  recordDirsBox->setTitle(tr("Record directories on disk"));
  recordDirsInfo->setText(tr("Record directory naming in base/: system names (id, as in stock MyTetra) or system + note names (hybrid, id_name). The choice applies to new notes and note renames."));
  recordDirNameModeLabel->setText(tr("Name new record directories: "));
  recordDirNameModeCombo->setItemText(0, tr("System names (id)"));
  recordDirNameModeCombo->setItemText(1, tr("System + note names (id_name)"));
  convertRecordDirsButton->setText(tr("Convert record directories to the selected form"));
  recordDirsWarning->setText(tr("Danger, beta version: back up your database directory before converting. If the database is encrypted, unlock it first. The tree will be saved with new directory names."));
  folderLinksBox->setTitle(tr("Folder links"));
  folderLinksInfo->setText(tr("Folders attach as links without copying, and as a clickable link in note text. Absolute paths break when the database moves; relative paths are stored from the note directory and survive relocation. \"Always ask\" shows a choice dialog for every folder. The choice applies to new folder links."));
  attachLinkModeLabel->setText(tr("Attach folders as: "));
  attachLinkModeCombo->setItemText(0, tr("Always ask"));
  attachLinkModeCombo->setItemText(1, tr("Relative to note directory"));
  attachLinkModeCombo->setItemText(2, tr("Absolute path"));
  convertFolderLinksButton->setText(tr("Convert folder and file links to the selected form"));
}

// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Attach::applyChanges(void)
{
    qDebug() << "Apply changes attach";

    int result=0;

    // Сохраняется настройка разрешения/запрещения подсветки записи с прикрепленными файлами
    if(AppContext::instance().appConfig()->uiSection()->getEnableRecordWithAttachHighlight()!=enableRecordWithAttachHighlight->isChecked())
        AppContext::instance().appConfig()->uiSection()->setEnableRecordWithAttachHighlight(enableRecordWithAttachHighlight->isChecked());

    // Сохраняется цвет подсветки
    if(AppContext::instance().appConfig()->uiSection()->getRecordWithAttachHighlightColor()!=highlightColor->name())
        AppContext::instance().appConfig()->uiSection()->setRecordWithAttachHighlightColor(highlightColor->name());

    // Сохраняется режим имен новых вложений (без переименования
    // существующих, это делает только кнопка конвертации)
    int selectedMode=attachNameModeCombo->currentData().toInt();
    if(AppContext::instance().appConfig()->storageSection()->get_attachNameMode()!=selectedMode)
        AppContext::instance().appConfig()->storageSection()->set_attachNameMode(selectedMode);

    // Сохраняется режим имен каталогов новых записей (без переименования
    // существующих, это делает только кнопка конвертации)
    int selectedDirMode=recordDirNameModeCombo->currentData().toInt();
    if(AppContext::instance().appConfig()->storageSection()->get_recordDirNameMode()!=selectedDirMode)
        AppContext::instance().appConfig()->storageSection()->set_recordDirNameMode(selectedDirMode);

    // Сохраняется вид ссылок на новые папки (без переписки
    // существующих, они хранятся и открываются как были)
    int selectedLinkMode=attachLinkModeCombo->currentData().toInt();
    if(AppContext::instance().appConfig()->storageSection()->get_attachLinkMode()!=selectedLinkMode)
        AppContext::instance().appConfig()->storageSection()->set_attachLinkMode(selectedLinkMode);

    return result;
}

// Приведение всех вложений базы к заданному виду имен.
// Проход идет только по метаданным (id, видимое имя, тип) и файлам на диске,
// содержимое вложений в память не загружается. XML базы не меняется:
// внутреннее имя вычисляется из id и видимого имени, а не хранится.
// Зашифрованные при закрытой базе записи пропускаются отдельно
// (видимое имя недоступно без пароля), ненайденные на диске - отдельно.
// Конфликтующее целевое имя (в оригинальном режиме) не затирается,
// а попадает в список ошибок
int AppConfigPage_Attach::convertAllAttachesToMode(int targetMode, int &notFoundCount, int &encryptedCount, QStringList &errors)
{
    notFoundCount=0;
    encryptedCount=0;
    errors.clear();

    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
    if(treeScreen==nullptr || treeScreen->knowTreeModel==nullptr)
    {
        errors << tr("Database tree is unavailable.");
        return 0;
    }

    KnowTreeModel *model=treeScreen->knowTreeModel;

    QSharedPointer< QSet<QString> > idList=model->getAllRecordsIdList();

    int renamedCount=0;

    foreach(QString recordId, *idList)
    {
        Record *record=model->getRecord(recordId);
        if(record==nullptr)
            continue;

        AttachTableData *attachTable=record->getAttachTablePointer();
        if(attachTable==nullptr)
            continue;

        for(int row=0; row<attachTable->size(); ++row)
        {
            QString attachId=attachTable->getIdByRow(row);
            Attach attach=attachTable->getAttach(attachId);

            if(attach.getField("type")!="file")
                continue;

            // Каталог записи доступен через публичный метод аттача
            // (Record::getFullDirName защищен, друзья - только Attach/AttachTableData)
            QString attachDir=attach.getFullInnerDirName();

            QString visibleName=attach.getField("fileName");

            // Пустое видимое имя означает зашифрованную при закрытой базе
            // запись: конвертировать нельзя, считается отдельно
            if(visibleName.isEmpty())
            {
                ++encryptedCount;
                continue;
            }

            QString targetName=Attach::fileNameForMode(targetMode, "file", attachId, visibleName);

            // Выясняется, какой файл реально лежит на диске
            // (перебираются все схемы имен)
            QStringList candidates=Attach::candidateFileNames("file", attachId, visibleName);
            QString existingName;
            foreach(QString candidate, candidates)
                if(QFile::exists(attachDir+"/"+candidate))
                {
                    existingName=candidate;
                    break;
                }

            // Исходного файла нет на диске: запись-сирота, считается отдельно
            if(existingName.isEmpty())
            {
                ++notFoundCount;
                continue;
            }

            // Файл уже в заданном виде: конвертировать нечего
            if(existingName==targetName)
                continue;

            // Целевое имя занято другим файлом: не затирается, ошибка
            if(QFile::exists(attachDir+"/"+targetName))
            {
                errors << tr("Name %1 in note \"%2\" is already taken, file %3 skipped.").arg(targetName).arg(record->getField("name")).arg(existingName);
                continue;
            }

            if(!QFile::rename(attachDir+"/"+existingName, attachDir+"/"+targetName))
            {
                errors << tr("Unable to rename file %1 to %2.").arg(attachDir+"/"+existingName).arg(attachDir+"/"+targetName);
                continue;
            }

            ++renamedCount;
        }
    }

    return renamedCount;
}

// Приведение всех ссылок на папки и файлы к заданному виду пути.
// Линком может быть и каталог, и файл (пикер Add link).
// Резолв в абсолютный, затем в целевой вид. Протухшие (цели нет
// на диске) пропускаются: setField отказался бы с модалкой на каждой
// строке. Шифрованные при закрытой базе — по пустому видимому имени.
// Возвращает число сконвертированных строк, -1 если дерево недоступно
int AppConfigPage_Attach::convertAllFolderLinksToMode(int targetMode, int &notFoundCount, int &encryptedCount)
{
    notFoundCount=0;
    encryptedCount=0;

    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
    if(treeScreen==nullptr || treeScreen->knowTreeModel==nullptr)
        return -1;

    KnowTreeModel *model=treeScreen->knowTreeModel;

    QSharedPointer< QSet<QString> > idList=model->getAllRecordsIdList();

    int convertedCount=0;

    foreach(QString recordId, *idList)
    {
        Record *record=model->getRecord(recordId);
        if(record==nullptr)
            continue;

        AttachTableData *attachTable=record->getAttachTablePointer();
        if(attachTable==nullptr)
            continue;

        const QString recordDir=attachTable->getRecordDirName();

        for(int row=0; row<attachTable->size(); ++row)
        {
            QString attachId=attachTable->getIdByRow(row);
            Attach attach=attachTable->getAttach(attachId);

            if(attach.getField("type")!="link")
                continue;

            // Пустое видимое имя означает зашифрованную при закрытой базе
            // запись: конвертировать нельзя, считается отдельно
            if(attach.getField("fileName").isEmpty())
            {
                ++encryptedCount;
                continue;
            }

            const QString absolutePath=Attach::resolveStoredLink(recordDir,
                                                                 attach.getField("link"));

            // Цели нет на диске: запись-сирота, считается отдельно
            if(!QFileInfo(absolutePath).exists())
            {
                ++notFoundCount;
                continue;
            }

            const QString newStoredLink=Attach::makeStoredLink(recordDir,
                                                               absolutePath,
                                                               targetMode);

            // Уже в заданном виде: конвертировать нечего
            if(newStoredLink==attach.getField("link"))
                continue;

            attach.setField("link", newStoredLink);
            attachTable->modifyAttach(attachId, attach);

            ++convertedCount;
        }
    }

    return convertedCount;
}


void AppConfigPage_Attach::onClickedConvertAttaches()
{
    int targetMode=attachNameModeCombo->currentData().toInt();
    QString modeName=attachNameModeCombo->currentText();

    // Подтверждение: действие меняет много файлов на диске
    QMessageBox::StandardButton answer=QMessageBox::question(
        this,
        tr("Convert attachments"),
        tr("All attachments will be brought to the form \"%1\". Back up your database directory first. Continue?").arg(modeName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if(answer!=QMessageBox::Yes)
        return;

    int notFoundCount=0;
    int encryptedCount=0;
    QStringList errors;
    int renamedCount=convertAllAttachesToMode(targetMode, notFoundCount, encryptedCount, errors);

    // Режим переключается только после прохода без ошибок, чтобы новые
    // вложения сразу сохранялись в заданном виде
    if(errors.isEmpty())
    {
        AppContext::instance().appConfig()->storageSection()->set_attachNameMode(targetMode);
        AppContext::instance().appConfig()->sync();
    }

    QString resultText=tr("Converted files: %1.").arg(renamedCount);
    if(notFoundCount>0)
        resultText+=QString(" ")+tr("Not found on disk: %1.").arg(notFoundCount);
    if(encryptedCount>0)
        resultText+=QString(" ")+tr("Skipped (encrypted record, unlock the database first): %1.").arg(encryptedCount);
    if(!errors.isEmpty())
        resultText+=QString(" ")+tr("Errors: %1. First error: %2").arg(errors.size()).arg(errors.first());

    if(errors.isEmpty())
        QMessageBox::information(this, tr("Convert attachments"), resultText);
    else
        QMessageBox::warning(this, tr("Convert attachments"), resultText);
}


void AppConfigPage_Attach::onClickedConvertRecordDirs()
{
    int targetMode=recordDirNameModeCombo->currentData().toInt();
    QString modeName=recordDirNameModeCombo->currentText();

    // Подтверждение: действие меняет много каталогов на диске и поля dir
    QMessageBox::StandardButton answer=QMessageBox::question(
        this,
        tr("Convert record directories"),
        tr("All record directories will be brought to the form \"%1\". Back up your database directory first. Continue?").arg(modeName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if(answer!=QMessageBox::Yes)
        return;

    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
    if(treeScreen==nullptr || treeScreen->knowTreeModel==nullptr)
    {
        QMessageBox::warning(this,
                             tr("Convert record directories"),
                             tr("Record tree is unavailable."));
        return;
    }

    int notFoundCount=0;
    int lockedCount=0;
    QStringList errors;
    int renamedCount=treeScreen->knowTreeModel->convertRecordDirsToMode(targetMode,
                                                                        notFoundCount,
                                                                        lockedCount,
                                                                        errors);

    // Режим и дерево сохраняются только после прохода без ошибок,
    // чтобы новые записи сразу именовались в заданном виде, а XML
    // не остался в промежуточном состоянии
    if(errors.isEmpty())
    {
        treeScreen->saveKnowTree();
        AppContext::instance().appConfig()->storageSection()->set_recordDirNameMode(targetMode);
        AppContext::instance().appConfig()->sync();
    }

    // Каталоги переехали: открытые редакторы смотрят в старые пути.
    // Переводятся на свежие dir из живой таблицы без перезапуска,
    // иначе show-in-folder и сейвы бьют мимо (даже при частичных
    // ошибках - удачные переименования уже на диске)
    MetaEditor *edView=find_object<MetaEditor>("editorScreen");

    if(edView!=nullptr)
    {
        const QString openId=edView->getMiscField(QStringLiteral("id"));

        if(!openId.isEmpty())
        {
            const QString freshDir=treeScreen->knowTreeModel->recordDirPath(openId);

            if(!freshDir.isEmpty())
                edView->refreshWorkDirectory(freshDir);
        }
    }

    AppContext::instance().dispatcher()->refreshAllWorkDirectories();

    QString resultText=tr("Converted directories: %1.").arg(renamedCount);
    if(notFoundCount>0)
        resultText+=QString(" ")+tr("Not found on disk (field fixed): %1.").arg(notFoundCount);
    if(lockedCount>0)
        resultText+=QString(" ")+tr("Skipped (encrypted record, unlock the database first): %1.").arg(lockedCount);
    if(!errors.isEmpty())
        resultText+=QString(" ")+tr("Errors: %1. First error: %2").arg(errors.size()).arg(errors.first());

    if(errors.isEmpty())
        QMessageBox::information(this, tr("Convert record directories"), resultText);
    else
        QMessageBox::warning(this, tr("Convert record directories"), resultText);
}


void AppConfigPage_Attach::onClickedConvertFolderLinks()
{
    int targetMode=attachLinkModeCombo->currentData().toInt();
    QString modeName=attachLinkModeCombo->currentText();

    // "Всегда спрашивать" — не хранимый вид: конвертировать не во что
    if(targetMode==AppConfig::AttachLinkAsk)
    {
        QMessageBox::information(this,
                                 tr("Convert folder and file links"),
                                 tr("Select relative or absolute path form to convert folder and file links."));
        return;
    }

    // Подтверждение: действие переписывает ссылки во всей базе.
    // Конвертировать только после расшифровки (база разблокирована):
    // иначе зашифрованные строки останутся в старом виде, база выйдет
    // из синхрона, а часть ссылок может оказаться повреждена
    QMessageBox::StandardButton answer=QMessageBox::question(
        this,
        tr("Convert folder and file links"),
        tr("All folder and file links will be brought to the form \"%1\". Back up your database directory first. Convert only after decryption (database unlocked): otherwise the database ends up out of sync and some links may get damaged. Continue?").arg(modeName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if(answer!=QMessageBox::Yes)
        return;

    int notFoundCount=0;
    int encryptedCount=0;
    int convertedCount=convertAllFolderLinksToMode(targetMode, notFoundCount, encryptedCount);

    if(convertedCount<0)
    {
        QMessageBox::warning(this,
                             tr("Convert folder and file links"),
                             tr("Database tree is unavailable."));
        return;
    }

    // Режим и дерево сохраняются: XML поменялся, новые папки сразу
    // сохраняются в заданном виде
    if(convertedCount>0)
    {
        TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

        if(treeScreen!=nullptr)
            treeScreen->saveKnowTree();

        AppContext::instance().appConfig()->storageSection()->set_attachLinkMode(targetMode);
        AppContext::instance().appConfig()->sync();
    }

    QString resultText=tr("Converted links: %1.").arg(convertedCount);
    if(notFoundCount>0)
        resultText+=QString(" ")+tr("Not found on disk: %1.").arg(notFoundCount);
    if(encryptedCount>0)
        resultText+=QString(" ")+tr("Skipped (encrypted record, unlock the database first): %1.").arg(encryptedCount);

    QMessageBox::information(this, tr("Convert folder and file links"), resultText);
}
