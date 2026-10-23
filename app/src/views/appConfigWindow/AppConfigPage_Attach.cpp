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
#include "libraries/helpers/ObjectHelper.h"

extern AppConfig mytetraConfig;


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
    enableRecordWithAttachHighlight->setChecked( mytetraConfig.getEnableRecordWithAttachHighlight() );

    // Выбор цвета для записи с прикрепленными файлами
    labelHighlightColor=new QLabel(this);
    labelHighlightColor->setText( tr("Highlight color: ") );

    buttonHighlightColor=new QToolButton(this);
    highlightColor=new QColor();
    this->setColorForButtonHighlightColor( QColor( mytetraConfig.getRecordWithAttachHighlightColor() ) );

    // Начальный выбор цвета активируется или отключается в зависимости от галки
    this->onEnableRecordWithAttachHighlight( mytetraConfig.getEnableRecordWithAttachHighlight() );

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
    attachNameModeCombo->setCurrentIndex( attachNameModeCombo->findData(mytetraConfig.get_attachNameMode()) );

    // Кнопка приведения всех вложений к выбранному виду
    convertAttachesButton=new QPushButton(this);
    convertAttachesButton->setText( tr("Convert attachments to the selected form") );

    // Предупреждение: действие опасное и пока в бета-версии
    fileNamesWarning=new QLabel(this);
    fileNamesWarning->setWordWrap(true);
    fileNamesWarning->setText( tr("Danger, beta version: back up your database directory before converting. If the database is encrypted, unlock it first. With original names, files with identical names in one note conflict and are skipped.") );
}


void AppConfigPage_Attach::setupSignals(void)
{
    connect( enableRecordWithAttachHighlight, &QCheckBox::toggled, this, &AppConfigPage_Attach::onEnableRecordWithAttachHighlight);
    connect( buttonHighlightColor, &QToolButton::clicked, this, &AppConfigPage_Attach::onClickedHighlightColor);
    connect( convertAttachesButton, &QPushButton::clicked, this, &AppConfigPage_Attach::onClickedConvertAttaches);
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

    // Собирается основной слой
    QVBoxLayout *centralLayout=new QVBoxLayout();
    centralLayout->addWidget( decorBox );
    centralLayout->addWidget( fileNamesBox );
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
}


// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Attach::applyChanges(void)
{
    qDebug() << "Apply changes attach";

    int result=0;

    // Сохраняется настройка разрешения/запрещения подсветки записи с прикрепленными файлами
    if(mytetraConfig.getEnableRecordWithAttachHighlight()!=enableRecordWithAttachHighlight->isChecked())
        mytetraConfig.setEnableRecordWithAttachHighlight(enableRecordWithAttachHighlight->isChecked());

    // Сохраняется цвет подсветки
    if(mytetraConfig.getRecordWithAttachHighlightColor()!=highlightColor->name())
        mytetraConfig.setRecordWithAttachHighlightColor(highlightColor->name());

    // Сохраняется режим имен новых вложений (без переименования
    // существующих, это делает только кнопка конвертации)
    int selectedMode=attachNameModeCombo->currentData().toInt();
    if(mytetraConfig.get_attachNameMode()!=selectedMode)
        mytetraConfig.set_attachNameMode(selectedMode);

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
        mytetraConfig.set_attachNameMode(targetMode);
        mytetraConfig.sync();
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
