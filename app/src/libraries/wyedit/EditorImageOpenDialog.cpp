#include <QListWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QTextStream>

#include "EditorImageOpenDialog.h"


EditorImageOpenDialog::EditorImageOpenDialog(const QString &fileName, QWidget *parent) : QDialog(parent),
    imageFileName(fileName)
{
    setupUi();
    setupSignals();
    assembly();
}


EditorImageOpenDialog::~EditorImageOpenDialog(void)
{

}


void EditorImageOpenDialog::setupUi(void)
{
    setWindowTitle(tr("Open image with"));

    programsList=new QListWidget(this);
    programsList->setSelectionMode(QAbstractItemView::SingleSelection);

    // Программы из системы первыми, затем ручной выбор через обзор
    QMap<QString, QString> programs=availableImagePrograms();
    QString defaultProgram=defaultImageProgram();

    QMapIterator<QString, QString> i(programs);

    while(i.hasNext())
    {
        i.next();

        QString displayName=i.key();

        if(i.value()==defaultProgram)
            displayName=tr("%1 (default)").arg(displayName);

        addProgram(displayName, i.value());
    }

    if(programsList->count()>0)
        programsList->setCurrentRow(0);

    browseButton=new QPushButton(tr("Browse..."), this);

    buttonBox=new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Open"));
}


void EditorImageOpenDialog::setupSignals(void)
{
    connect(browseButton, &QPushButton::clicked,
            this,         &EditorImageOpenDialog::onBrowseClicked);

    connect(programsList, &QListWidget::itemDoubleClicked,
            this,         &EditorImageOpenDialog::onItemDoubleClicked);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &EditorImageOpenDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &EditorImageOpenDialog::reject);
}


void EditorImageOpenDialog::assembly(void)
{
    QVBoxLayout *layout=new QVBoxLayout();
    layout->addWidget(new QLabel(tr("Program to open %1:").arg(QFileInfo(imageFileName).fileName())));
    layout->addWidget(programsList);

    QHBoxLayout *bottomLayout=new QHBoxLayout();
    bottomLayout->addWidget(browseButton);
    bottomLayout->addStretch();
    bottomLayout->addWidget(buttonBox);

    layout->addLayout(bottomLayout);

    setLayout(layout);
    resize(420, 320);
}


QString EditorImageOpenDialog::selectedProgram(void) const
{
    return chosenProgram;
}


void EditorImageOpenDialog::onBrowseClicked(void)
{
    QString program=QFileDialog::getOpenFileName(this,
                                                 tr("Select program"),
                                                 QDir::homePath());

    if(program.isEmpty())
        return;

    // Ручной выбор добавляется в общий список и сразу выделяется
    addProgram(tr("%1 (custom)").arg(QFileInfo(program).fileName()), program);
    programsList->setCurrentRow(programsList->count()-1);
}


void EditorImageOpenDialog::onItemDoubleClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);

    accept();
}


void EditorImageOpenDialog::addProgram(const QString &displayName, const QString &command)
{
    QListWidgetItem *item=new QListWidgetItem(displayName, programsList);
    item->setData(Qt::UserRole, command);
}


void EditorImageOpenDialog::accept(void)
{
    QListWidgetItem *item=programsList->currentItem();

    if(item!=nullptr)
        chosenProgram=item->data(Qt::UserRole).toString();

    QDialog::accept();
}


// Убрать из Exec .desktop коды полей: %f %F %u %U %d %D %n %N %i %c %k %v %m,
// двойной %% превращается в одинарный. Файл подставляется при запуске
QString EditorImageOpenDialog::cleanExecCommand(const QString &exec)
{
    QString cleaned=exec;

    cleaned.replace("%%", "\x01");

    int pos=0;

    while((pos=cleaned.indexOf('%', pos))>=0)
    {
        if(pos+1<cleaned.length() && cleaned.at(pos+1)!='%')
            cleaned.remove(pos, 2);
        else
            pos++;
    }

    cleaned.replace("\x01", "%");

    return cleaned.trimmed();
}


// Программы из .desktop-файлов, заявившие поддержку картинок.
// Сканируются пользовательские и системные каталоги приложений
QMap<QString, QString> EditorImageOpenDialog::availableImagePrograms(void)
{
    QMap<QString, QString> programs;

#ifdef Q_OS_LINUX
    QStringList applicationsDirs;
    applicationsDirs << QDir::homePath()+"/.local/share/applications";

    QString xdgDataDirs=qgetenv("XDG_DATA_DIRS");

    if(xdgDataDirs.isEmpty())
        xdgDataDirs="/usr/local/share:/usr/share";

    QStringList dataDirs=xdgDataDirs.split(':', QString::SkipEmptyParts);

    for(int i=0; i<dataDirs.size(); i++)
        applicationsDirs << dataDirs.at(i)+"/applications";

    QStringList seenFiles;

    for(int d=0; d<applicationsDirs.size(); d++)
    {
        QDir dir(applicationsDirs.at(d));

        if(!dir.exists())
            continue;

        QStringList desktopFiles=dir.entryList(QStringList() << "*.desktop", QDir::Files);

        for(int f=0; f<desktopFiles.size(); f++)
        {
            // Файл с тем же именем в приоритетном каталоге уже разобран
            if(seenFiles.contains(desktopFiles.at(f)))
                continue;

            seenFiles.append(desktopFiles.at(f));

            QFile file(dir.absoluteFilePath(desktopFiles.at(f)));

            if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QTextStream stream(&file);
            stream.setCodec("UTF-8");

            bool inDesktopEntry=false;
            QString entryType;
            QString entryName;
            QString entryExec;
            QString entryMime;
            bool entryNoDisplay=false;
            bool entryHidden=false;

            while(!stream.atEnd())
            {
                QString line=stream.readLine().trimmed();

                if(line.startsWith('['))
                {
                    inDesktopEntry=(line=="[Desktop Entry]");
                    continue;
                }

                if(!inDesktopEntry)
                    continue;

                if(line.startsWith("Type="))
                    entryType=line.mid(5);
                else if(line.startsWith("Name=") && entryName.isEmpty())
                    entryName=line.mid(5);
                else if(line.startsWith("Exec="))
                    entryExec=line.mid(5);
                else if(line.startsWith("MimeType="))
                    entryMime=line.mid(9);
                else if(line=="NoDisplay=true" || line=="Hidden=true")
                {
                    entryNoDisplay=true;
                }
            }

            file.close();

            if(entryType!="Application")
                continue;

            if(entryNoDisplay || entryHidden)
                continue;

            if(entryName.isEmpty() || entryExec.isEmpty())
                continue;

            // Нужны только умеющие картинки: image/png, image/jpeg, image/*...
            bool handlesImages=false;
            QStringList mimeTypes=entryMime.split(';', QString::SkipEmptyParts);

            for(int m=0; m<mimeTypes.size(); m++)
            {
                if(mimeTypes.at(m).startsWith("image/"))
                {
                    handlesImages=true;
                    break;
                }
            }

            if(!handlesImages)
                continue;

            programs[entryName]=cleanExecCommand(entryExec);
        }
    }
#endif

    return programs;
}


// Программа по умолчанию для картинок через xdg-mime.
// Возвращается команда запуска если она есть в списке, иначе пусто
QString EditorImageOpenDialog::defaultImageProgram(void)
{
    QString defaultCommand;

#ifdef Q_OS_LINUX
    QProcess mimeQuery;
    mimeQuery.start("xdg-mime", QStringList() << "query" << "default" << "image/png");
    mimeQuery.waitForFinished(3000);

    QString desktopFile=QString(mimeQuery.readAllStandardOutput()).trimmed();

    if(desktopFile.isEmpty())
        return QString();

    QStringList locations;
    locations << QDir::homePath()+"/.local/share/applications/"+desktopFile;
    locations << "/usr/local/share/applications/"+desktopFile;
    locations << "/usr/share/applications/"+desktopFile;

    for(int i=0; i<locations.size(); i++)
    {
        QFile file(locations.at(i));

        if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QTextStream stream(&file);
        stream.setCodec("UTF-8");

        while(!stream.atEnd())
        {
            QString line=stream.readLine().trimmed();

            if(line.startsWith("Exec="))
            {
                defaultCommand=cleanExecCommand(line.mid(5));
                break;
            }
        }

        file.close();

        if(!defaultCommand.isEmpty())
            break;
    }
#endif

    return defaultCommand;
}
