#include <QWidget>
#include <QColorDialog>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
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
    fileNamesInfo->setText( tr("Attachments are stored on disk as id.extension. Renaming switches them to human-readable names id_name.extension, so the file can be recognized without opening it. New attachments will use human-readable names too.") );

    // Кнопка запуска переименования
    renameAttachesButton=new QPushButton(this);
    renameAttachesButton->setText( tr("Rename attachments to human-readable names") );

    // Предупреждение: действие опасное и пока в бета-версии
    fileNamesWarning=new QLabel(this);
    fileNamesWarning->setWordWrap(true);
    fileNamesWarning->setText( tr("Danger, beta version: back up your database directory before renaming. If the database is encrypted, unlock it first.") );
}


void AppConfigPage_Attach::setupSignals(void)
{
    connect( enableRecordWithAttachHighlight, &QCheckBox::toggled, this, &AppConfigPage_Attach::onEnableRecordWithAttachHighlight);
    connect( buttonHighlightColor, &QToolButton::clicked, this, &AppConfigPage_Attach::onClickedHighlightColor);
    connect( renameAttachesButton, &QPushButton::clicked, this, &AppConfigPage_Attach::onClickedRenameAttachesToHumanNames);
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
    QVBoxLayout *fileNamesBoxLayout=new QVBoxLayout;
    fileNamesBoxLayout->addWidget( fileNamesInfo );
    fileNamesBoxLayout->addWidget( renameAttachesButton );
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
  fileNamesInfo->setText(tr("Attachments are stored on disk as id.extension. Renaming switches them to human-readable names id_name.extension, so the file can be recognized without opening it. New attachments will use human-readable names too."));
  renameAttachesButton->setText(tr("Rename attachments to human-readable names"));
  fileNamesWarning->setText(tr("Danger, beta version: back up your database directory before renaming. If the database is encrypted, unlock it first."));
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

    // Схема имен файлов переключается только кнопкой переименования,
    // здесь ничего сохранять не нужно

    return result;
}


// Переименование всех вложений базы в человеко-читаемые имена.
// Проход идет только по метаданным (id, видимое имя, тип) и файлам на диске,
// содержимое вложений в память не загружается. XML базы не меняется:
// внутреннее имя вычисляется из id и видимого имени, а не хранится.
// Зашифрованные при закрытой базе записи пропускаются (видимое имя
// недоступно без пароля) и попадают в счетчик пропущенных
int AppConfigPage_Attach::renameAllAttachesToHumanNames(int &missingCount, QStringList &errors)
{
    missingCount=0;
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
            // запись: переименовать нельзя, файл пропускается
            if(visibleName.isEmpty())
            {
                ++missingCount;
                continue;
            }

            QString legacyName=Attach::constructLegacyFileName("file", attachId, visibleName);
            QString humanName=Attach::constructHumanFileName("file", attachId, visibleName);

            // Схемы совпали (имя с точкой в конце, пустая основа):
            // переименовывать нечего
            if(legacyName==humanName)
                continue;

            // Файл уже по человеко-читаемой схеме: переименовывать нечего
            if(QFile::exists(attachDir+"/"+humanName))
                continue;

            // Исходного файла нет на диске: запись-сирота, пропускается
            if(!QFile::exists(attachDir+"/"+legacyName))
            {
                ++missingCount;
                continue;
            }

            if(!QFile::rename(attachDir+"/"+legacyName, attachDir+"/"+humanName))
            {
                errors << tr("Unable to rename file %1 to %2.").arg(attachDir+"/"+legacyName).arg(attachDir+"/"+humanName);
                continue;
            }

            ++renamedCount;
        }
    }

    return renamedCount;
}


void AppConfigPage_Attach::onClickedRenameAttachesToHumanNames()
{
    // Подтверждение: действие меняет много файлов на диске
    QMessageBox::StandardButton answer=QMessageBox::question(
        this,
        tr("Rename attachments"),
        tr("All attachments will be renamed from id.extension to human-readable names id_name.extension. Back up your database directory first. Continue?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if(answer!=QMessageBox::Yes)
        return;

    int missingCount=0;
    QStringList errors;
    int renamedCount=renameAllAttachesToHumanNames(missingCount, errors);

    // Схема переключается только после успешного прохода, чтобы новые
    // вложения сразу сохранялись по человеко-читаемой схеме
    if(errors.isEmpty())
    {
        mytetraConfig.set_attachHumanReadableNames(true);
        mytetraConfig.sync();
    }

    QString resultText=tr("Renamed files: %1.").arg(renamedCount);
    if(missingCount>0)
        resultText+=QString(" ")+tr("Skipped (file not found or encrypted record): %1.").arg(missingCount);
    if(!errors.isEmpty())
        resultText+=QString(" ")+tr("Errors: %1. First error: %2").arg(errors.size()).arg(errors.first());

    if(errors.isEmpty())
        QMessageBox::information(this, tr("Rename attachments"), resultText);
    else
        QMessageBox::warning(this, tr("Rename attachments"), resultText);
}
