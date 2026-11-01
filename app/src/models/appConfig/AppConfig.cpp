#include <functional>
#include <QtGlobal>
#include <QRect>
#include <QVariant>
#include <QTextCodec>

#include "AppConfig.h"
#include "AppConfigUpdater.h"
#include "AppConfigGeneral.h"

#include "AppConfigStorage.h"
#include "AppConfigUi.h"
#include "AppConfigFind.h"
#include "AppConfigSync.h"
#include "AppConfigCrypto.h"
#include "AppConfigRecordTable.h"
#include "AppConfigClipper.h"
#include "AppConfigServices.h"

#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "libraries/helpers/DebugHelper.h"
#include "libraries/helpers/DiskHelper.h"
#include "libraries/AppContext.h"
#include "libraries/ActionLogger.h"

// Конструктор объекта настройки программы
AppConfig::AppConfig(QObject *pobj)
{
    Q_UNUSED(pobj)

    m_isInit=false;
}

// Деструктор объекта настройки программы
AppConfig::~AppConfig()
{
    if (m_isInit)
    {
        qDebug() << "Save mytetra config file";
        m_conf->sync();
    }
}


void AppConfig::init(void)
{
    // Имя файла конфигурации
    QString configFileName = AppContext::instance().globalParameters()->getWorkDirectory()+"/conf.ini";

    // Проверяется, есть ли файл конфигурации
    QFile confFile(configFileName);
    if (!confFile.exists())
    {
        criticalError("File "+configFileName+" not found.");
    }

    // Создается указатель на объект хранилища конфигурации
    m_conf = new QSettings(configFileName, QSettings::IniFormat, this);
    m_conf->setIniCodec( QTextCodec::codecForName("UTF-8") );

    // Здесь конфиг считан, и в нем уже есть начальные настройки, включая директорию корзины
    // В этот момент еще не происходило записи конфига на диск, и следует проверить,
    // имеется ли возможность копировать данные в директорию корзины, включая и сам конфиг.
    // Если в корзину копирование невозможно, это может означать что диск полностью заполнен,
    // и если сделать sync() для конфига, конфиг не сможет записаться,
    // и файл конфига будет полностью обнулен, что недопустимо
    createFirstAtStartConfigCopy();

    // Обновление конфига с более старых версий
    update_version_process();

    // Запись конфига на диск
    sync();

    m_isInit = true;
}

// Создание первой при старте программы копии конфигов MyTetra в корзине
// Это необходимо чтобы в корзине лежали актуальные копии конфигов при каждом старте программы
// А так же если копии сделать невозможно, это значит что переписывать
// или запускать sync() для конфигов нельзя, так как они могут обнулиться,
// например если закончилось свободное место на диске, поэтому программу надо завершить

// Создание первой при старте программы копии конфигов MyTetra в корзине
// Это необходимо чтобы в корзине лежали актуальные копии конфигов при каждом старте программы
// А так же если копии сделать невозможно, это значит что переписывать
// или запускать sync() для конфигов нельзя, так как они могут обнулиться,
// например если закончилось свободное место на диске, поэтому программу надо завершить


// Создание первой при старте программы копии конфигов MyTetra в корзине
// Это необходимо чтобы в корзине лежали актуальные копии конфигов при каждом старте программы
// А так же если копии сделать невозможно, это значит что переписывать
// или запускать sync() для конфигов нельзя, так как они могут обнулиться,
// например если закончилось свободное место на диске, поэтому программу надо завершить
void AppConfig::createFirstAtStartConfigCopy()
{
    // Если директория корзины существует, происходит проверка копированием в корзину
    // Если корзины нет, то это значит что происходит установка MyTetra, и корзины,
    // базы данных или конфиг-файла пока у пользователя нет или они первичные,
    // а значит защищать конфиг-файл от обнуления нет необходимости
    if (DiskHelper::isTrashDirectoryExists() )
    {
        QStringList fileNames = QStringList()
                                << AppContext::instance().globalParameters()->getWorkDirectory()+"/conf.ini"
                                << AppContext::instance().globalParameters()->getWorkDirectory()+"/editorconf.ini"
                                << AppContext::instance().globalParameters()->getWorkDirectory()+"/shortcut.ini"
                                << AppContext::instance().globalParameters()->getWorkDirectory()+"/knownbases.ini"
                                << storageSection()->get_tetradir()+"/database.ini";

        for (const auto& fileName : fileNames)
        {
            // Обязательно надо проверять существование файла, так как разные конфигурирующие файлы
            // появлялись и должны появляться в разных версиях MyTetra, и могут отсутсвовать,
            // например если было обновление бинарника программы со старой версии на новую
            if ( QFile::exists(fileName) )
            {
                QString copyResult = DiskHelper::copyFileToTrash(fileName, false);

                if ( copyResult.isEmpty() )
                {
                    criticalError(tr("Can not create config backup copy to trash directory\n")+
                                  tr("\nSource file: %1").arg( fileName )+
                                  tr("\nTrash directory: %1\n").arg( storageSection()->get_trashdir() )+
                                  tr("\nThis can happen if there is no free space left on the hard disk or it is impossible to write to the trash directory."));
                }
            }
        }
    }
}


bool AppConfig::is_init(void)
{
    return m_isInit;
}


void AppConfig::sync(void)
{
    m_conf->sync();
}

// Получение имени файла конфига, с которым происходит работа

// Получение имени файла конфига, с которым происходит работа


// Получение имени файла конфига, с которым происходит работа
QString AppConfig::getConfigFileName()
{
    return m_conf->fileName();
}

// Получение параметра по имени в виде строки с проверкой его существования

// Получение параметра по имени в виде строки с проверкой его существования


// Получение параметра по имени в виде строки с проверкой его существования
QString AppConfig::get_parameter(QString name)
{
    if (m_conf->contains(name))
    {
        return m_conf->value(name).toString();
    }
    else
    {
        // Страховка вместо жёсткого падения: если параметр известен
        // актуальной таблице, вернуть его дефолт (ключ мог потеряться
        // из-за усечённых лимитом миграций или чужого conf.ini).
        // Неизвестное имя по-прежнему роняет программу — это ошибка программиста
        QStringList actualTable=get_parameter_table_60(true);
        if(!getParameterTypeFromTable(name, actualTable).isEmpty())
        {
            QString defValue=getParameterDefaultValueFromTable(name, actualTable);
            qWarning() << "In config not found parameter" << name << ", using default value:" << defValue;

            // Счетчик гниения конфига: первое срабатывание по имени уходит
            // в actionLog (дальше только счетчик, чтобы не спамить).
            // addAction безопасен до init() логгера: молча ничего не пишет
            ++m_fallbackTotal;
            const bool firstHit=!m_fallbackHits.contains(name);
            m_fallbackHits[name]=m_fallbackHits.value(name, 0)+1;
            if(firstHit)
                AppContext::instance().actionLogger()->addAction(
                    QStringLiteral("configFallback"),
                    {{"name", name}, {"defaultValue", defValue}});

            return defValue;
        }

        criticalError("In config not found parameter " + name);
    }
}


int AppConfig::get_configFallbackCount(void) const
{
    return m_fallbackTotal;
}


QMap<QString, int> AppConfig::get_configFallbackHits(void) const
{
    return m_fallbackHits;
}

// Установка имени директории с данными (в которой находится mytetra.xml)


void AppConfig::setMcpUiLock(bool locked)
{
    m_mcpUiLock=locked;
}


bool AppConfig::isMcpUiLocked(void)
{
    return m_mcpUiLock;
}


AppConfigStorage *AppConfig::storageSection(void)
{
    if(m_storage==nullptr)
    {
        m_storage=new AppConfigStorage(this);
    }

    m_storage->setBackend(m_conf, this);

    return m_storage;
}


AppConfigUi *AppConfig::uiSection(void)
{
    if(m_ui==nullptr)
    {
        m_ui=new AppConfigUi(this);
    }

    m_ui->setBackend(m_conf, this);

    return m_ui;
}


AppConfigFind *AppConfig::findSection(void)
{
    if(m_find==nullptr)
    {
        m_find=new AppConfigFind(this);
    }

    m_find->setBackend(m_conf, this);

    return m_find;
}


AppConfigSync *AppConfig::syncSection(void)
{
    if(m_sync==nullptr)
    {
        m_sync=new AppConfigSync(this);
    }

    m_sync->setBackend(m_conf, this);

    return m_sync;
}


AppConfigCrypto *AppConfig::cryptoSection(void)
{
    if(m_crypto==nullptr)
    {
        m_crypto=new AppConfigCrypto(this);
    }

    m_crypto->setBackend(m_conf, this);

    return m_crypto;
}


AppConfigRecordTable *AppConfig::recordTableSection(void)
{
    if(m_recordtable==nullptr)
    {
        m_recordtable=new AppConfigRecordTable(this);
    }

    m_recordtable->setBackend(m_conf, this);

    return m_recordtable;
}


AppConfigClipper *AppConfig::clipperSection(void)
{
    if(m_clipper==nullptr)
    {
        m_clipper=new AppConfigClipper(this);
    }

    m_clipper->setBackend(m_conf, this);

    return m_clipper;
}


AppConfigGeneral *AppConfig::generalSection(void)
{
    if(m_general==nullptr)
    {
        m_general=new AppConfigGeneral(this);
    }

    m_general->setBackend(m_conf, this);

    return m_general;
}


AppConfigServices *AppConfig::servicesSection(void)
{
    if(m_services==nullptr)
    {
        m_services=new AppConfigServices(this);
    }

    m_services->setBackend(m_conf, this);

    return m_services;
}

