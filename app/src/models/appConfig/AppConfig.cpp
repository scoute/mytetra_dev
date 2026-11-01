#include <functional>
#include <QtGlobal>
#include <QRect>
#include <QVariant>
#include <QTextCodec>

#include "AppConfig.h"
#include "AppConfigUpdater.h"

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
                                << get_tetradir()+"/database.ini";

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
                                  tr("\nTrash directory: %1\n").arg( get_trashdir() )+
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
QString AppConfig::getConfigFileName()
{
    return m_conf->fileName();
}

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
            return defValue;
        }

        criticalError("In config not found parameter " + name);
    }
}

// Установка имени директории с данными (в которой находится mytetra.xml)

bool AppConfig::get_printdebugmessages(void)
{
    return m_conf->value("printdebugmessages").toBool();
}

bool AppConfig::set_printdebugmessages(bool isPrint)
{
    m_conf->setValue("printdebugmessages", isPrint);
    return true;
}

QString AppConfig::get_interfacelanguage(void)
{
    QString language = this->get_parameter("interfacelanguage");
    qDebug() << "Get interface language" << language;
    return language;
}

bool AppConfig::set_interfacelanguage(QString language)
{
    m_conf->setValue("interfacelanguage", language);
    return true;
}

QString AppConfig::get_addnewrecord_expand_info(void)
{
    return this->get_parameter("addnewrecord_expand_info");
}

void AppConfig::set_addnewrecord_expand_info(QString state)
{
    if (state=="0" || state=="1")
    {
        m_conf->setValue("addnewrecord_expand_info", state);
    }
    else
    {
        criticalError("Set unavailable value for addnewrecord_expand_info "+state);
    }
}

void AppConfig::setMcpUiLock(bool locked)
{
    m_mcpUiLock=locked;
}

bool AppConfig::isMcpUiLocked(void)
{
    return m_mcpUiLock;
}

int AppConfig::get_config_version(void)
{
    int v = m_conf->contains("version") ? m_conf->value("version").toInt() : 0;

    return v;
}

void AppConfig::set_config_version(int i)
{
    m_conf->setValue("version", i);
}

// P2: ленивые доступы к секциям. Бэкенд перепривязывается при
// каждом обращении, поэтому фасады работают и без init()
// (пробы ставят m_conf напрямую)

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

AppConfigServices *AppConfig::servicesSection(void)
{
    if(m_services==nullptr)
    {
        m_services=new AppConfigServices(this);
    }

    m_services->setBackend(m_conf, this);

    return m_services;
}

// --------------------
// Секция Storage (реализация в AppConfigStorage.cpp)
// --------------------

bool AppConfig::set_tetradir(QString dirName)
{
    return storageSection()->set_tetradir(dirName);
}

    // Директория с базой знаний
QString AppConfig::get_tetradir(void)
{
    return storageSection()->get_tetradir();
}

bool AppConfig::set_trashdir(QString dirName)
{
    return storageSection()->set_trashdir(dirName);
}

    // Директория с корзиной
QString AppConfig::get_trashdir(void)
{
    return storageSection()->get_trashdir();
}

    // Максимальный размер директории корзины в мегабайтах
unsigned int AppConfig::get_trashsize(void)
{
    return storageSection()->get_trashsize();
}

bool AppConfig::set_trashsize(unsigned int mbSize)
{
    return storageSection()->set_trashsize(mbSize);
}

    // Максимально допустимое число файлов в корзине
int AppConfig::get_trashmaxfilecount(void)
{
    return storageSection()->get_trashmaxfilecount();
}

bool AppConfig::set_trashmaxfilecount(int count)
{
    return storageSection()->set_trashmaxfilecount(count);
}

    // Путь на диске по которому пользователь открывал файлы чтобы приаттачить их к записи
QString AppConfig::getAttachAppendDir(void)
{
    return storageSection()->getAttachAppendDir();
}

void AppConfig::setAttachAppendDir(QString dir)
{
    storageSection()->setAttachAppendDir(dir);
}

    // Путь на диске по которому пользователь сохранял (Сохранить как...) приаттаченные файлы
QString AppConfig::getAttachSaveAsDir(void)
{
    return storageSection()->getAttachSaveAsDir();
}

void AppConfig::setAttachSaveAsDir(QString dir)
{
    storageSection()->setAttachSaveAsDir(dir);
}

    // Разрешать ли для просмотра расшифровывать зашифрованные файлы в директорию корзины MyTetra
bool AppConfig::getEnableDecryptFileToTrashDirectory(void)
{
    return storageSection()->getEnableDecryptFileToTrashDirectory();
}

void AppConfig::setEnableDecryptFileToTrashDirectory(bool state)
{
    storageSection()->setEnableDecryptFileToTrashDirectory(state);
}

    // Размер файла лога дейсвтвий
unsigned int AppConfig::getActionLogMaximumSize()
{
    return storageSection()->getActionLogMaximumSize();
}

void AppConfig::setActionLogMaximumSize(unsigned int mbSize)
{
    storageSection()->setActionLogMaximumSize(mbSize);
}

    // Разрешено ли логирование
bool AppConfig::getEnableLogging(void)
{
    return storageSection()->getEnableLogging();
}

void AppConfig::setEnableLogging(bool state)
{
    storageSection()->setEnableLogging(state);
}

    // Человеко-читаемые имена вложенных файлов на диске (схема id_имя.ext
    // вместо id.ext). По умолчанию выключено. Включается кнопкой миграции
    // в настройках прикрепленных файлов, после чего новые вложения
    // сразу сохраняются по человеко-читаемой схеме.
    // Устаревший флаг: теперь схема задается режимом attachNameMode,
    // методы оставлены для совместимости (true = гибридный режим)
bool AppConfig::get_attachHumanReadableNames(void)
{
    return storageSection()->get_attachHumanReadableNames();
}

void AppConfig::set_attachHumanReadableNames(bool state)
{
    storageSection()->set_attachHumanReadableNames(state);
}

int AppConfig::get_attachNameMode(void)
{
    return storageSection()->get_attachNameMode();
}

void AppConfig::set_attachNameMode(int mode)
{
    storageSection()->set_attachNameMode(mode);
}

// --------------------
// Секция Ui (реализация в AppConfigUi.cpp)
// --------------------

    // Нужно ли показывать подтверждение при выполнении действия "cut на ветке
bool AppConfig::get_cutbranchconfirm(void)
{
    return uiSection()->get_cutbranchconfirm();
}

bool AppConfig::set_cutbranchconfirm(bool confirm)
{
    return uiSection()->set_cutbranchconfirm(confirm);
}

    // Геометрия основного окна
QByteArray AppConfig::get_mainwingeometry(void)
{
    return uiSection()->get_mainwingeometry();
}

void AppConfig::set_mainwingeometry(QByteArray dataGeometry)
{
    uiSection()->set_mainwingeometry(dataGeometry);
}

QList<int> AppConfig::get_vspl_size_list(void)
{
    return uiSection()->get_vspl_size_list();
}

void AppConfig::set_vspl_size_list(QList<int> list)
{
    uiSection()->set_vspl_size_list(list);
}

QList<int> AppConfig::get_hspl_size_list(void)
{
    return uiSection()->get_hspl_size_list();
}

void AppConfig::set_hspl_size_list(QList<int> list)
{
    uiSection()->set_hspl_size_list(list);
}

QList<int> AppConfig::get_findsplitter_size_list(void)
{
    return uiSection()->get_findsplitter_size_list();
}

void AppConfig::set_findsplitter_size_list(QList<int> list)
{
    uiSection()->set_findsplitter_size_list(list);
}

    // Размеры левой колонки (дерево над списком записей), когда список
    // записей перенесён под дерево. Остальные сплиттеры - выше
QList<int> AppConfig::get_leftspl_size_list(void)
{
    return uiSection()->get_leftspl_size_list();
}

void AppConfig::set_leftspl_size_list(QList<int> list)
{
    uiSection()->set_leftspl_size_list(list);
}

QList<int> AppConfig::get_splitter_size_list(QString name)
{
    return uiSection()->get_splitter_size_list(name);
}

void AppConfig::set_splitter_size_list(QString name, QList<int> list)
{
    uiSection()->set_splitter_size_list(name, list);
}

QStringList AppConfig::get_tree_position(void)
{
    return uiSection()->get_tree_position();
}

void AppConfig::set_tree_position(QStringList list)
{
    uiSection()->set_tree_position(list);
}

bool AppConfig::get_runinminimizedwindow(void)
{
    return uiSection()->get_runinminimizedwindow();
}

void AppConfig::set_runinminimizedwindow(bool flag)
{
    uiSection()->set_runinminimizedwindow(flag);
}

int AppConfig::getEditorCursorPosition(void)
{
    return uiSection()->getEditorCursorPosition();
}

void AppConfig::setEditorCursorPosition(int n)
{
    uiSection()->setEditorCursorPosition(n);
}

int AppConfig::getEditorScrollBarPosition(void)
{
    return uiSection()->getEditorScrollBarPosition();
}

void AppConfig::setEditorScrollBarPosition(int n)
{
    uiSection()->setEditorScrollBarPosition(n);
}

int AppConfig::getUglyQssReplaceHeightForTableView(void)
{
    return uiSection()->getUglyQssReplaceHeightForTableView();
}

void AppConfig::setUglyQssReplaceHeightForTableView(int n)
{
    uiSection()->setUglyQssReplaceHeightForTableView(n);
}

    // Показывать ли сплешскрин при старте программы
bool AppConfig::getShowSplashScreen(void)
{
    return uiSection()->getShowSplashScreen();
}

void AppConfig::setShowSplashScreen(bool isShow)
{
    uiSection()->setShowSplashScreen(isShow);
}

    // Режим интерфейса, возможные значения "desktop" и "mobile"
QString AppConfig::getInterfaceMode(void)
{
    return uiSection()->getInterfaceMode();
}

void AppConfig::setInterfaceMode(QString mode)
{
    uiSection()->setInterfaceMode(mode);
}

QString AppConfig::getFocusWidget(void)
{
    return uiSection()->getFocusWidget();
}

void AppConfig::setFocusWidget(QString widgetName)
{
    uiSection()->setFocusWidget(widgetName);
}

QStringList AppConfig::getHideEditorTools(void)
{
    return uiSection()->getHideEditorTools();
}

void AppConfig::setHideEditorTools(QStringList toolsNames)
{
    uiSection()->setHideEditorTools(toolsNames);
}

    // Флаг, показывающий распахнуты ли все инструметы управления в окне поиска по базе
bool AppConfig::getFindInBaseExpand(void)
{
    return uiSection()->getFindInBaseExpand();
}

void AppConfig::setFindInBaseExpand(bool state)
{
    uiSection()->setFindInBaseExpand(state);
}

    // Режим поиска в заметке: встраиваемая полоска (true) или отдельное окно
    // (false). По умолчанию полоска, переключается кнопками в окне,
    // на полоске и радиокнопками на странице настроек Search
bool AppConfig::get_editorFindEmbedded(void)
{
    return uiSection()->get_editorFindEmbedded();
}

void AppConfig::set_editorFindEmbedded(bool state)
{
    uiSection()->set_editorFindEmbedded(state);
}

    // Панель избранного видна (по умолчанию да). Намеренно вне
    // версионных таблиц, как editorFindEmbedded
bool AppConfig::get_favoritesEnabled(void)
{
    return uiSection()->get_favoritesEnabled();
}

    // Строка фильтра над деревом веток (тумблер в меню Вид)
bool AppConfig::get_treeFilterVisible(void)
{
    return uiSection()->get_treeFilterVisible();
}

void AppConfig::set_treeFilterVisible(bool state)
{
    uiSection()->set_treeFilterVisible(state);
}

    // Вид панели прикрепленных файлов: "list" или "tiles"
QString AppConfig::get_attachViewMode(void)
{
    return uiSection()->get_attachViewMode();
}

void AppConfig::set_attachViewMode(QString mode)
{
    uiSection()->set_attachViewMode(mode);
}

void AppConfig::set_favoritesEnabled(bool state)
{
    uiSection()->set_favoritesEnabled(state);
}

    // Хранилище избранного: "sidecar" (data/favorites.xml, по умолчанию)
    // или "xml" (поля favorite в mytetra.xml). Тоже без версий
QString AppConfig::get_favoritesStorage(void)
{
    return uiSection()->get_favoritesStorage();
}

void AppConfig::set_favoritesStorage(const QString &mode)
{
    uiSection()->set_favoritesStorage(mode);
}

    // Разрешено ли использовать собственный формат вывода даты и времени
bool AppConfig::getEnableCustomDateTimeFormat(void)
{
    return uiSection()->getEnableCustomDateTimeFormat();
}

void AppConfig::setEnableCustomDateTimeFormat(bool state)
{
    uiSection()->setEnableCustomDateTimeFormat(state);
}

    // Строка собственного формата вывода даты и времени
QString AppConfig::getCustomDateTimeFormat(void)
{
    return uiSection()->getCustomDateTimeFormat();
}

void AppConfig::setCustomDateTimeFormat(QString format)
{
    uiSection()->setCustomDateTimeFormat(format);
}

    // Разрешена ли подсветка записей с прикрепленными файлами
bool AppConfig::getEnableRecordWithAttachHighlight(void)
{
    return uiSection()->getEnableRecordWithAttachHighlight();
}

void AppConfig::setEnableRecordWithAttachHighlight(bool state)
{
    uiSection()->setEnableRecordWithAttachHighlight(state);
}

    // Цвет подсветки записей с прикрепленными файлами
QString AppConfig::getRecordWithAttachHighlightColor(void)
{
    return uiSection()->getRecordWithAttachHighlightColor();
}

void AppConfig::setRecordWithAttachHighlightColor(QString color)
{
    uiSection()->setRecordWithAttachHighlightColor(color);
}

    // Цвет закраски секрета в формате "#77767b"
QString AppConfig::get_secretColor(void)
{
    return uiSection()->get_secretColor();
}

void AppConfig::set_secretColor(QString color)
{
    uiSection()->set_secretColor(color);
}

    // Цвет фона поля заметки. Пустая строка означает цвет темы
    // по умолчанию, иначе формат цвета - "#B0BAC7"
QString AppConfig::get_notePaperColor(void)
{
    return uiSection()->get_notePaperColor();
}

void AppConfig::set_notePaperColor(QString color)
{
    uiSection()->set_notePaperColor(color);
}

    // Цвет шрифта поля заметки. Пустая строка означает цвет темы
    // по умолчанию, иначе формат цвета - "#000000"
QString AppConfig::get_noteFontColor(void)
{
    return uiSection()->get_noteFontColor();
}

void AppConfig::set_noteFontColor(QString color)
{
    uiSection()->set_noteFontColor(color);
}

    // Кастомный дизайн поверх темы. Пусто/-1 означает цвет/размер темы,
    // иначе значение пользователя. Применяется CssHelper::applyCustomDesign
QString AppConfig::get_themeCustomTreeBackground(void)
{
    return uiSection()->get_themeCustomTreeBackground();
}

void AppConfig::set_themeCustomTreeBackground(QString color)
{
    uiSection()->set_themeCustomTreeBackground(color);
}

QString AppConfig::get_themeCustomTreeText(void)
{
    return uiSection()->get_themeCustomTreeText();
}

void AppConfig::set_themeCustomTreeText(QString color)
{
    uiSection()->set_themeCustomTreeText(color);
}

QString AppConfig::get_themeCustomTableBackground(void)
{
    return uiSection()->get_themeCustomTableBackground();
}

void AppConfig::set_themeCustomTableBackground(QString color)
{
    uiSection()->set_themeCustomTableBackground(color);
}

QString AppConfig::get_themeCustomTableText(void)
{
    return uiSection()->get_themeCustomTableText();
}

void AppConfig::set_themeCustomTableText(QString color)
{
    uiSection()->set_themeCustomTableText(color);
}

QString AppConfig::get_themeCustomTableAlternate(void)
{
    return uiSection()->get_themeCustomTableAlternate();
}

void AppConfig::set_themeCustomTableAlternate(QString color)
{
    uiSection()->set_themeCustomTableAlternate(color);
}

QString AppConfig::get_themeCustomInterfaceFontFamily(void)
{
    return uiSection()->get_themeCustomInterfaceFontFamily();
}

void AppConfig::set_themeCustomInterfaceFontFamily(QString family)
{
    uiSection()->set_themeCustomInterfaceFontFamily(family);
}

int AppConfig::get_themeCustomInterfaceFontSize(void)
{
    return uiSection()->get_themeCustomInterfaceFontSize();
}

void AppConfig::set_themeCustomInterfaceFontSize(int size)
{
    uiSection()->set_themeCustomInterfaceFontSize(size);
}

int AppConfig::get_themeCustomTreeFontSize(void)
{
    return uiSection()->get_themeCustomTreeFontSize();
}

void AppConfig::set_themeCustomTreeFontSize(int size)
{
    uiSection()->set_themeCustomTreeFontSize(size);
}

int AppConfig::get_themeCustomTableFontSize(void)
{
    return uiSection()->get_themeCustomTableFontSize();
}

void AppConfig::set_themeCustomTableFontSize(int size)
{
    uiSection()->set_themeCustomTableFontSize(size);
}

int AppConfig::get_themeCustomTreeIndentation(void)
{
    return uiSection()->get_themeCustomTreeIndentation();
}

void AppConfig::set_themeCustomTreeIndentation(int pixels)
{
    uiSection()->set_themeCustomTreeIndentation(pixels);
}

int AppConfig::get_themeCustomTableRowHeight(void)
{
    return uiSection()->get_themeCustomTableRowHeight();
}

void AppConfig::set_themeCustomTableRowHeight(int pixels)
{
    uiSection()->set_themeCustomTableRowHeight(pixels);
}

    // Мозаичный фон заметок: тайл повторяется по обеим осям.
    // Пусто означает без картинки. Повторяется по обеим осям
QString AppConfig::get_themeCustomNotePaperImage(void)
{
    return uiSection()->get_themeCustomNotePaperImage();
}

void AppConfig::set_themeCustomNotePaperImage(QString path)
{
    uiSection()->set_themeCustomNotePaperImage(path);
}

    // Индекс встроенного тайла (0 - tx_fabric.jpg). Свой файл выше
    // имеет приоритет над индексом
int AppConfig::get_themeCustomNotePaperBuiltin(void)
{
    return uiSection()->get_themeCustomNotePaperBuiltin();
}

void AppConfig::set_themeCustomNotePaperBuiltin(int index)
{
    uiSection()->set_themeCustomNotePaperBuiltin(index);
}

    // Режим фона заметок: "color" или "texture". Текстура без своего
    // файла означает встроенный tx_fabric.jpg
QString AppConfig::get_themeCustomNotePaperMode(void)
{
    return uiSection()->get_themeCustomNotePaperMode();
}

void AppConfig::set_themeCustomNotePaperMode(QString mode)
{
    uiSection()->set_themeCustomNotePaperMode(mode);
}

    // Базовая тема для Custom: одна из четырех (default, dark,
    // modern, midnight). Сами базовые темы не меняются никогда
QString AppConfig::get_themeCustomBaseTheme(void)
{
    return uiSection()->get_themeCustomBaseTheme();
}

void AppConfig::set_themeCustomBaseTheme(QString name)
{
    uiSection()->set_themeCustomBaseTheme(name);
}

    // Шапка заметки (название и автор) над редактором.
    // По умолчанию показана
bool AppConfig::get_recordHeaderVisible(void)
{
    return uiSection()->get_recordHeaderVisible();
}

void AppConfig::set_recordHeaderVisible(bool state)
{
    uiSection()->set_recordHeaderVisible(state);
}

    // Положение списка записей: false - справа от дерева (по умолчанию),
    // true - под деревом в левой колонке. Страница Notes area
bool AppConfig::get_recordTableUnderTree(void)
{
    return uiSection()->get_recordTableUnderTree();
}

void AppConfig::set_recordTableUnderTree(bool state)
{
    uiSection()->set_recordTableUnderTree(state);
}

    // Порядок кнопок тулбаров в виде списков через запятую.
    // Пусто - не настраивалось, экраны строят порядок по умолчанию.
    // (Пустой настроенный список невозможен: в задействованных
    // обязана остаться хотя бы одна команда, как в редакторе)
QStringList AppConfig::getTreeToolbarOrder(void)
{
    return uiSection()->getTreeToolbarOrder();
}

void AppConfig::setTreeToolbarOrder(QStringList order)
{
    uiSection()->setTreeToolbarOrder(order);
}

QStringList AppConfig::getRecordToolbarOrder(void)
{
    return uiSection()->getRecordToolbarOrder();
}

void AppConfig::setRecordToolbarOrder(QStringList order)
{
    uiSection()->setRecordToolbarOrder(order);
}

QStringList AppConfig::getRecordExtraToolbarOrder(void)
{
    return uiSection()->getRecordExtraToolbarOrder();
}

void AppConfig::setRecordExtraToolbarOrder(QStringList order)
{
    uiSection()->setRecordExtraToolbarOrder(order);
}

    // Размер иконок в окне выбора (иконки, прикрепляемые к веткам)
int AppConfig::getPreviewIconSize(void)
{
    return uiSection()->getPreviewIconSize();
}

void AppConfig::setPreviewIconSize(int n)
{
    uiSection()->setPreviewIconSize(n);
}

    // Последняя выбранная секция в окошке выбора иконок
QString AppConfig::getIconCurrentSectionName(void)
{
    return uiSection()->getIconCurrentSectionName();
}

void AppConfig::setIconCurrentSectionName(QString name)
{
    uiSection()->setIconCurrentSectionName(name);
}

    // Показывать ли в дереве общий итог записей с учетом подветок.
    // По умолчанию выключено: только свои [n]. Страница Branch tree
bool AppConfig::get_showSubtreeCount(void)
{
    return uiSection()->get_showSubtreeCount();
}

void AppConfig::set_showSubtreeCount(bool state)
{
    uiSection()->set_showSubtreeCount(state);
}

    // Список открепляемых окон с координатами окон
QString AppConfig::getDockableWindowsState(void)
{
    return uiSection()->getDockableWindowsState();
}

void AppConfig::setDockableWindowsState(QString state)
{
    uiSection()->setDockableWindowsState(state);
}

QString AppConfig::getDockableWindowsBehavior(void)
{
    return uiSection()->getDockableWindowsBehavior();
}

void AppConfig::setDockableWindowsBehavior(QString mode)
{
    uiSection()->setDockableWindowsBehavior(mode);
}

    // Тема оформления интерфейса
QString AppConfig::getInterfaceTheme()
{
    return uiSection()->getInterfaceTheme();
}

bool AppConfig::setInterfaceTheme(QString themeName)
{
    return uiSection()->setInterfaceTheme(themeName);
}

QString AppConfig::getInterfaceIconSize()
{
    return uiSection()->getInterfaceIconSize();
}

bool AppConfig::setInterfaceIconSize(QString sizeName)
{
    return uiSection()->setInterfaceIconSize(sizeName);
}

// --------------------
// Секция Find (реализация в AppConfigFind.cpp)
// --------------------

    // Настройки виджета поиска по базе
int AppConfig::get_findscreen_wordregard(void)
{
    return findSection()->get_findscreen_wordregard();
}

void AppConfig::set_findscreen_wordregard(int pos)
{
    findSection()->set_findscreen_wordregard(pos);
}

int AppConfig::get_findscreen_howextract(void)
{
    return findSection()->get_findscreen_howextract();
}

void AppConfig::set_findscreen_howextract(int pos)
{
    findSection()->set_findscreen_howextract(pos);
}

int AppConfig::getFindScreenTreeSearchArea(void)
{
    return findSection()->getFindScreenTreeSearchArea();
}

void AppConfig::setFindScreenTreeSearchArea(int pos)
{
    findSection()->setFindScreenTreeSearchArea(pos);
}

bool AppConfig::get_findscreen_find_in_field(QString fieldName)
{
    return findSection()->get_findscreen_find_in_field(fieldName);
}

void AppConfig::set_findscreen_find_in_field(QString fieldName, bool isChecked)
{
    findSection()->set_findscreen_find_in_field(fieldName, isChecked);
}

bool AppConfig::get_findscreen_show(void)
{
    return findSection()->get_findscreen_show();
}

void AppConfig::set_findscreen_show(bool isShow)
{
    findSection()->set_findscreen_show(isShow);
}

// --------------------
// Секция Sync (реализация в AppConfigSync.cpp)
// --------------------

    // Умное автосохранение: только при реальных правках,
    // без изменений ни файла ни коммита. По умолчанию вкл, 10 мин
bool AppConfig::get_autosaveEnable(void)
{
    return syncSection()->get_autosaveEnable();
}

void AppConfig::set_autosaveEnable(bool flag)
{
    syncSection()->set_autosaveEnable(flag);
}

int AppConfig::get_autosaveInterval(void)
{
    return syncSection()->get_autosaveInterval();
}

void AppConfig::set_autosaveInterval(int minutes)
{
    syncSection()->set_autosaveInterval(minutes);
}

    // Сквош истории изменений: автосжатие снизу по лимиту.
    // По умолчанию включено, хранить 100 (genesis плюс свежие)
bool AppConfig::get_historySquashEnable(void)
{
    return syncSection()->get_historySquashEnable();
}

void AppConfig::set_historySquashEnable(bool flag)
{
    syncSection()->set_historySquashEnable(flag);
}

int AppConfig::get_historySquashKeep(void)
{
    return syncSection()->get_historySquashKeep();
}

void AppConfig::set_historySquashKeep(int count)
{
    syncSection()->set_historySquashKeep(count);
}

    // Стратегия сжатия (GitHelper::SquashMode).
    // По умолчанию удалять старые
int AppConfig::get_historySquashMode(void)
{
    return syncSection()->get_historySquashMode();
}

void AppConfig::set_historySquashMode(int mode)
{
    syncSection()->set_historySquashMode(mode);
}

    // Мастер-тумблер ведения истории заметок. Выключенная история
    // не снимается и диалог не открывается. По умолчанию выключена:
    // нужен установленный git, проверяется при включении
bool AppConfig::get_historyEnable(void)
{
    return syncSection()->get_historyEnable();
}

void AppConfig::set_historyEnable(bool flag)
{
    syncSection()->set_historyEnable(flag);
}

QString AppConfig::get_synchrocommand(void)
{
    return syncSection()->get_synchrocommand();
}

void AppConfig::set_synchrocommand(QString command)
{
    syncSection()->set_synchrocommand(command);
}

bool AppConfig::get_synchroonstartup(void)
{
    return syncSection()->get_synchroonstartup();
}

void AppConfig::set_synchroonstartup(bool flag)
{
    syncSection()->set_synchroonstartup(flag);
}

bool AppConfig::get_synchroonexit(void)
{
    return syncSection()->get_synchroonexit();
}

void AppConfig::set_synchroonexit(bool flag)
{
    syncSection()->set_synchroonexit(flag);
}

bool AppConfig::getSyncroConsoleDetails(void)
{
    return syncSection()->getSyncroConsoleDetails();
}

void AppConfig::setSyncroConsoleDetails(bool flag)
{
    syncSection()->setSyncroConsoleDetails(flag);
}

    // Разрешена ли периодическая проверка файла базы на предмет изменения сторонней программой
bool AppConfig::getEnablePeriodicCheckBase(void)
{
    return syncSection()->getEnablePeriodicCheckBase();
}

void AppConfig::setEnablePeriodicCheckBase(bool state)
{
    syncSection()->setEnablePeriodicCheckBase(state);
}

    // Период проверки файла базы на предмет изменения сторонней программой
int AppConfig::getCheckBasePeriod()
{
    return syncSection()->getCheckBasePeriod();
}

void AppConfig::setCheckBasePeriod(int period)
{
    syncSection()->setCheckBasePeriod(period);
}

    // Необходимо ли выводить сообщение если база была изменена другой программой
bool AppConfig::getEnablePeriodicCheckMessage(void)
{
    return syncSection()->getEnablePeriodicCheckMessage();
}

void AppConfig::setEnablePeriodicCheckMessage(bool state)
{
    syncSection()->setEnablePeriodicCheckMessage(state);
}

    // Разрешена ли периодическая синхронизация
bool AppConfig::getEnablePeriodicSyncro(void)
{
    return syncSection()->getEnablePeriodicSyncro();
}

void AppConfig::setEnablePeriodicSyncro(bool state)
{
    syncSection()->setEnablePeriodicSyncro(state);
}

    // Период автоматической периодической синхронизации
int AppConfig::getPeriodicSyncroPeriod(void)
{
    return syncSection()->getPeriodicSyncroPeriod();
}

void AppConfig::setPeriodicSyncroPeriod(int period)
{
    syncSection()->setPeriodicSyncroPeriod(period);
}

// --------------------
// Секция Crypto (реализация в AppConfigCrypto.cpp)
// --------------------

QString AppConfig::get_howpassrequest(void)
{
    return cryptoSection()->get_howpassrequest();
}

void AppConfig::set_howpassrequest(QString mode)
{
    cryptoSection()->set_howpassrequest(mode);
}

bool AppConfig::get_autoClosePasswordEnable(void)
{
    return cryptoSection()->get_autoClosePasswordEnable();
}

void AppConfig::set_autoClosePasswordEnable(bool flag)
{
    cryptoSection()->set_autoClosePasswordEnable(flag);
}

int AppConfig::get_autoClosePasswordDelay(void)
{
    return cryptoSection()->get_autoClosePasswordDelay();
}

void AppConfig::set_autoClosePasswordDelay(int delay)
{
    cryptoSection()->set_autoClosePasswordDelay(delay);
}

QString AppConfig::getPasswordMiddleHash(void)
{
    return cryptoSection()->getPasswordMiddleHash();
}

void AppConfig::setPasswordMiddleHash(QString hash)
{
    cryptoSection()->setPasswordMiddleHash(hash);
}

bool AppConfig::getPasswordSaveFlag(void)
{
    return cryptoSection()->getPasswordSaveFlag();
}

void AppConfig::setPasswordSaveFlag(bool flag)
{
    cryptoSection()->setPasswordSaveFlag(flag);
}

bool AppConfig::getRememberCursorAtHistoryNavigation(void)
{
    return cryptoSection()->getRememberCursorAtHistoryNavigation();
}

void AppConfig::setRememberCursorAtHistoryNavigation(bool flag)
{
    cryptoSection()->setRememberCursorAtHistoryNavigation(flag);
}

bool AppConfig::getRememberCursorAtOrdinarySelection(void)
{
    return cryptoSection()->getRememberCursorAtOrdinarySelection();
}

void AppConfig::setRememberCursorAtOrdinarySelection(bool flag)
{
    cryptoSection()->setRememberCursorAtOrdinarySelection(flag);
}

    // Метод шифрования: rc5 (совместимый) или xchacha (современный)
QString AppConfig::get_cryptMethod(void)
{
    return cryptoSection()->get_cryptMethod();
}

void AppConfig::set_cryptMethod(const QString &method)
{
    cryptoSection()->set_cryptMethod(method);
}

// --------------------
// Секция RecordTable (реализация в AppConfigRecordTable.cpp)
// --------------------

    // ID записи в таблице конечных записей, которая выделена на экране
QString AppConfig::get_recordtable_selected_record_id(void)
{
    return recordTableSection()->get_recordtable_selected_record_id();
}

void AppConfig::set_recordtable_selected_record_id(QString id)
{
    recordTableSection()->set_recordtable_selected_record_id(id);
}

    // Перечень полей, отображаемых в таблице конечных записей
QStringList AppConfig::getRecordTableShowFields(void)
{
    return recordTableSection()->getRecordTableShowFields();
}

void AppConfig::setRecordTableShowFields(QStringList fields)
{
    recordTableSection()->setRecordTableShowFields(fields);
}

    // Нужно ли отображать горизонтальные заголовки в таблице конечных записей
bool AppConfig::getRecordTableShowHorizontalHeaders(void)
{
    return recordTableSection()->getRecordTableShowHorizontalHeaders();
}

void AppConfig::setRecordTableShowHorizontalHeaders(bool flag)
{
    recordTableSection()->setRecordTableShowHorizontalHeaders(flag);
}

    // Нужно ли отображать вертикальные заголовки в таблице конечных записей
bool AppConfig::getRecordTableShowVerticalHeaders(void)
{
    return recordTableSection()->getRecordTableShowVerticalHeaders();
}

void AppConfig::setRecordTableShowVerticalHeaders(bool flag)
{
    recordTableSection()->setRecordTableShowVerticalHeaders(flag);
}

    // Ширина полей, отображаемых в таблице конечных записей
QStringList AppConfig::getRecordTableFieldsWidth(void)
{
    return recordTableSection()->getRecordTableFieldsWidth();
}

void AppConfig::setRecordTableFieldsWidth(QStringList fieldsWidth)
{
    recordTableSection()->setRecordTableFieldsWidth(fieldsWidth);
}

    // Глобальный режим только чтения: запрещены любые изменения базы,
    // разрешены навигация и просмотр. По умолчанию выключен
bool AppConfig::get_readOnly(void)
{
    return recordTableSection()->get_readOnly();
}

    // Сырой глобальный флаг без учёта сессии. Им смотрят только
    // MCP-записи: они продолжают писать при заблокированном UI
bool AppConfig::isGlobalReadOnly(void)
{
    return recordTableSection()->isGlobalReadOnly();
}

void AppConfig::set_readOnly(bool state)
{
    recordTableSection()->set_readOnly(state);
}

    // Разрешено ли создавать запись без текста
bool AppConfig::getEnableCreateEmptyRecord(void)
{
    return recordTableSection()->getEnableCreateEmptyRecord();
}

void AppConfig::setEnableCreateEmptyRecord(bool state)
{
    recordTableSection()->setEnableCreateEmptyRecord(state);
}

// --------------------
// Секция Clipper (реализация в AppConfigClipper.cpp)
// --------------------

    // Лимиты клиппера: сколько картинок забирать в одну заметку
    // и максимальный размер одной картинки в мегабайтах
int AppConfig::get_clipperMaxImages(void)
{
    return clipperSection()->get_clipperMaxImages();
}

bool AppConfig::set_clipperMaxImages(int count)
{
    return clipperSection()->set_clipperMaxImages(count);
}

int AppConfig::get_clipperMaxImageSizeMb(void)
{
    return clipperSection()->get_clipperMaxImageSizeMb();
}

bool AppConfig::set_clipperMaxImageSizeMb(int mbSize)
{
    return clipperSection()->set_clipperMaxImageSizeMb(mbSize);
}

    // Веб-клиппер: включение, глобальный хоткей, запомненная ветка
bool AppConfig::get_clipperenable(void)
{
    return clipperSection()->get_clipperenable();
}

void AppConfig::set_clipperenable(bool flag)
{
    clipperSection()->set_clipperenable(flag);
}

QString AppConfig::get_clipperhotkey(void)
{
    return clipperSection()->get_clipperhotkey();
}

void AppConfig::set_clipperhotkey(QString sequence)
{
    clipperSection()->set_clipperhotkey(sequence);
}

QString AppConfig::get_clipperbranchid(void)
{
    return clipperSection()->get_clipperbranchid();
}

void AppConfig::set_clipperbranchid(QString id)
{
    clipperSection()->set_clipperbranchid(id);
}

    // Режим целевой ветки клипа: 0 — по умолчанию (unsorted_notes),
    // 1 — выбранная ветка (clipperbranchid), 2 — спрашивать каждый раз
int AppConfig::get_clipperbranchmode(void)
{
    return clipperSection()->get_clipperbranchmode();
}

void AppConfig::set_clipperbranchmode(int mode)
{
    clipperSection()->set_clipperbranchmode(mode);
}

    // Частые ветки клипов: пары id:count через ";", частые впереди,
    // лимит 16. Нижние плитки опроса показывают их вместо полного списка
QString AppConfig::get_clipperrecent(void)
{
    return clipperSection()->get_clipperrecent();
}

void AppConfig::set_clipperrecent(const QString &ids)
{
    clipperSection()->set_clipperrecent(ids);
}

    // Какие 16 плиток внизу опроса: 0 — последние, 1 — частые
int AppConfig::get_clippertilemode(void)
{
    return clipperSection()->get_clippertilemode();
}

void AppConfig::set_clippertilemode(int mode)
{
    clipperSection()->set_clippertilemode(mode);
}

    // При попытке вставить дубль открывать оригинал заметки
bool AppConfig::get_clipperopendup(void)
{
    return clipperSection()->get_clipperopendup();
}

void AppConfig::set_clipperopendup(bool open)
{
    clipperSection()->set_clipperopendup(open);
}

// --------------------
// Секция Services (реализация в AppConfigServices.cpp)
// --------------------

    // Разрешен ли запуск MCP-сервера внешним AI-клиентом
bool AppConfig::get_mcpAllow(void)
{
    return servicesSection()->get_mcpAllow();
}

void AppConfig::set_mcpAllow(bool state)
{
    servicesSection()->set_mcpAllow(state);
}

    // Игнорировать ошибки проверки TLS при загрузке картинок/файлов.
    // По умолчанию выключено: невалидный/самоподписанный сертификат
    // отклоняется. Включение снижает защиту от MITM
bool AppConfig::get_tlsIgnoreInvalidCertificates(void)
{
    return servicesSection()->get_tlsIgnoreInvalidCertificates();
}

void AppConfig::set_tlsIgnoreInvalidCertificates(bool state)
{
    servicesSection()->set_tlsIgnoreInvalidCertificates(state);
}

    // Раскладка импорта Telegram: "month" (заметка на месяц,
    // по умолчанию), "year" (заметка на год, прямо в чате),
    // "day" (заметка на день, ветки чат-год-месяц)
    // или "message" (заметка на сообщение, ветки чат-год-месяц-день)
QString AppConfig::get_telegramImportMode(void)
{
    return servicesSection()->get_telegramImportMode();
}

void AppConfig::set_telegramImportMode(const QString &mode)
{
    servicesSection()->set_telegramImportMode(mode);
}

    // Формат голосовых заметок по умолчанию: opus или mp3.
    // Выбирается радио на странице Voice notes, в диалоге
    // записи переключается. Неподдержанный бэкендом сбрасывается
QString AppConfig::get_voiceNoteFormat(void)
{
    return servicesSection()->get_voiceNoteFormat();
}

void AppConfig::set_voiceNoteFormat(QString format)
{
    servicesSection()->set_voiceNoteFormat(format);
}

    // Подавление фонового шума в диалоге записи голосовых заметок.
    // По умолчанию включено: цепочка HP+гейт наTurn очищает гул
bool AppConfig::get_voiceNoteNoiseSuppress(void)
{
    return servicesSection()->get_voiceNoteNoiseSuppress();
}

void AppConfig::set_voiceNoteNoiseSuppress(bool value)
{
    servicesSection()->set_voiceNoteNoiseSuppress(value);
}

    // Качество голосовых заметок по умолчанию: 0 low, 1 medium, 2 high
int AppConfig::get_voiceNoteQuality(void)
{
    return servicesSection()->get_voiceNoteQuality();
}

void AppConfig::set_voiceNoteQuality(int quality)
{
    servicesSection()->set_voiceNoteQuality(quality);
}
