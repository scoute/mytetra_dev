#include "AppConfigUi.h"
#include "AppConfig.h"
#include "libraries/FixedParameters.h"
#include "libraries/helpers/DebugHelper.h"
#include "libraries/AppContext.h"

AppConfigUi::AppConfigUi(QObject *parent) : QObject(parent)
{

}

void AppConfigUi::setBackend(QSettings *conf, AppConfig *host)
{
    m_conf=conf;
    m_host=host;
}

bool AppConfigUi::get_cutbranchconfirm(void)
{
    return m_conf->value("cutbranchconfirm").toBool();
}

bool AppConfigUi::set_cutbranchconfirm(bool confirm)
{
    m_conf->setValue("cutbranchconfirm", confirm);
    return true;
}

QByteArray AppConfigUi::get_mainwingeometry(void)
{
    return QByteArray::fromBase64( m_conf->value("mainwingeometry", "").toString().toLatin1() );
}

void AppConfigUi::set_mainwingeometry(QByteArray dataGeometry)
{
    qDebug() << "Save main window geometry";

    m_conf->setValue( "mainwingeometry", QString( dataGeometry.toBase64().data() ) );
}

QList<int> AppConfigUi::get_vspl_size_list(void)
{
    return get_splitter_size_list("vspl");
}

void AppConfigUi::set_vspl_size_list(QList<int> list)
{
    set_splitter_size_list("vspl", list);
}

QList<int> AppConfigUi::get_hspl_size_list(void)
{
    return get_splitter_size_list("hspl");
}

void AppConfigUi::set_hspl_size_list(QList<int> list)
{
    set_splitter_size_list("hspl", list);
}

QList<int> AppConfigUi::get_findsplitter_size_list(void)
{
    return get_splitter_size_list("findsplitter");
}

void AppConfigUi::set_findsplitter_size_list(QList<int> list)
{
    // qDebug() << "Config set find splitter list to " << list;
    set_splitter_size_list("findsplitter", list);
}

QList<int> AppConfigUi::get_leftspl_size_list(void)
{
    return get_splitter_size_list("leftspl");
}

void AppConfigUi::set_leftspl_size_list(QList<int> list)
{
    set_splitter_size_list("leftspl", list);
}

QList<int> AppConfigUi::get_splitter_size_list(QString name)
{
    QStringList line_list;
    QList<int> list;

    line_list = (m_conf->value(name+"_size_list", "100,100")).toString().split(",");

    for (int i=0;i < line_list.size(); ++i)
    {
        list.append( line_list.at(i).toInt() );
    }

    return list;
}

void AppConfigUi::set_splitter_size_list(QString name, QList<int> list)
{
    QStringList line_list;

    for (int i=0;i < list.size(); ++i)
    {
        line_list.append( QString::number(list.at(i)) );
    }

    m_conf->setValue(name+"_size_list", line_list.join(","));
}

QStringList AppConfigUi::get_tree_position(void)
{
    return (m_conf->value("tree_position","1")).toString().split(",");
}

void AppConfigUi::set_tree_position(QStringList list)
{
    qDebug() << "AppConfigUi::set_tree_position() : " << list;
    m_conf->setValue("tree_position", list.join(","));
}

// ID записи в таблице конечных записей, которая выделена на экране

bool AppConfigUi::get_runinminimizedwindow(void)
{
    return m_conf->value("runinminimizedwindow").toBool();
}

void AppConfigUi::set_runinminimizedwindow(bool flag)
{
    m_conf->setValue("runinminimizedwindow", flag);
}

int AppConfigUi::getEditorCursorPosition(void)
{
    return m_conf->value("editorCursorPosition",0).toInt();
}

void AppConfigUi::setEditorCursorPosition(int n)
{
    m_conf->setValue("editorCursorPosition", n);
}

int AppConfigUi::getEditorScrollBarPosition(void)
{
    return m_conf->value("editorScrollBarPosition",0).toInt();
}

void AppConfigUi::setEditorScrollBarPosition(int n)
{
    m_conf->setValue("editorScrollBarPosition", n);
}

int AppConfigUi::get_printFooterAlign(void)
{
    const int align=m_conf->value("printFooterAlign", 1).toInt();

    if(align<0 || align>2)
        return 1;

    return align;
}

void AppConfigUi::set_printFooterAlign(int mode)
{
    if(mode<0 || mode>2)
        mode=1;

    m_conf->setValue("printFooterAlign", mode);
}

bool AppConfigUi::get_printFooterEnabled(void)
{
    return m_conf->value("printFooterEnabled", true).toBool();
}

void AppConfigUi::set_printFooterEnabled(bool state)
{
    m_conf->setValue("printFooterEnabled", state);
}

int AppConfigUi::getUglyQssReplaceHeightForTableView(void)
{
    return m_conf->value("uglyQssReplaceHeightForTableView", 0).toInt();
}

void AppConfigUi::setUglyQssReplaceHeightForTableView(int n)
{
    m_conf->setValue("uglyQssReplaceHeightForTableView", n);
}

// Перечень полей, отображаемых в таблице конечных записей.
// Служебные поля без подписи (favorite) из старых конфигов
// отрезаются здесь: колонками они не являются, пустой столбец
// с пустым заголовком не нужен. При следующем apply список
// перезапишется уже чистым

// Показывать ли сплешскрин при старте программы
bool AppConfigUi::getShowSplashScreen(void)
{
    return m_conf->value("showSplashScreen").toBool();
}

void AppConfigUi::setShowSplashScreen(bool isShow)
{
    m_conf->setValue("showSplashScreen", isShow);
}

// Режим интерфейса, возможные значения "desktop" и "mobile"

// Режим интерфейса, возможные значения "desktop" и "mobile"
QString AppConfigUi::getInterfaceMode(void)
{
    QString mode = m_host->get_parameter("interfaceMode");
    return mode;
}

void AppConfigUi::setInterfaceMode(QString mode)
{
    m_conf->setValue("interfaceMode", mode);
}

// Имя последнего активного виджета

// Имя последнего активного виджета
QString AppConfigUi::getFocusWidget(void)
{
    QString widgetName = m_host->get_parameter("focusWidget");
    return widgetName;
}

void AppConfigUi::setFocusWidget(QString widgetName)
{
    m_conf->setValue("focusWidget", widgetName);
}

QStringList AppConfigUi::getHideEditorTools(void)
{
    return (m_conf->value("hideEditorTools", "")).toString().split(",");
}

void AppConfigUi::setHideEditorTools(QStringList toolsNames)
{
    m_conf->setValue("hideEditorTools", toolsNames.join(","));
}

bool AppConfigUi::getFindInBaseExpand(void)
{
    return m_conf->value("findInBaseExpand").toBool();
}

void AppConfigUi::setFindInBaseExpand(bool state)
{
    m_conf->setValue("findInBaseExpand", state);
}

bool AppConfigUi::get_editorFindEmbedded(void)
{
    return m_conf->value("editorFindEmbedded", true).toBool();
}

void AppConfigUi::set_editorFindEmbedded(bool state)
{
    m_conf->setValue("editorFindEmbedded", state);
}

bool AppConfigUi::get_favoritesEnabled(void)
{
    return m_conf->value("favoritesEnabled", true).toBool();
}

// Строка фильтра над деревом веток (тумблер в меню Вид)

// Строка фильтра над деревом веток (тумблер в меню Вид)
bool AppConfigUi::get_treeFilterVisible(void)
{
    return m_conf->value("treeFilterVisible", true).toBool();
}

void AppConfigUi::set_treeFilterVisible(bool state)
{
    m_conf->setValue("treeFilterVisible", state);
}

// Вид панели прикрепленных файлов: "list" или "tiles".
// Неизвестное значение читается как "list"

// Вид панели прикрепленных файлов: "list" или "tiles".
// Неизвестное значение читается как "list"
QString AppConfigUi::get_attachViewMode(void)
{
    const QString mode=m_conf->value("attachViewMode", "list").toString();

    if(mode==QStringLiteral("tiles"))
        return mode;

    return QStringLiteral("list");
}

void AppConfigUi::set_attachViewMode(QString mode)
{
    if(mode!=QStringLiteral("tiles"))
        mode=QStringLiteral("list");

    m_conf->setValue("attachViewMode", mode);
}

void AppConfigUi::set_favoritesEnabled(bool state)
{
    m_conf->setValue("favoritesEnabled", state);
}

QString AppConfigUi::get_favoritesStorage(void)
{
    return m_conf->value("favoritesStorage", "sidecar").toString();
}

void AppConfigUi::set_favoritesStorage(const QString &mode)
{
    m_conf->setValue("favoritesStorage", mode);
}

// Разрешено ли использовать собственный формат вывода даты и времени

// Разрешено ли использовать собственный формат вывода даты и времени
bool AppConfigUi::getEnableCustomDateTimeFormat(void)
{
    return m_conf->value("enableCustomDateTimeFormat").toBool();
}

void AppConfigUi::setEnableCustomDateTimeFormat(bool state)
{
    m_conf->setValue("enableCustomDateTimeFormat", state);
}

// Строка собственного формата вывода даты и времени

// Строка собственного формата вывода даты и времени
QString AppConfigUi::getCustomDateTimeFormat(void)
{
    return m_host->get_parameter("customDateTimeFormat");
}

void AppConfigUi::setCustomDateTimeFormat(QString format)
{
    m_conf->setValue("customDateTimeFormat", format);
}

// Путь на диске по которому пользователь открывал файлы чтобы приаттачить их к записи

// Разрешена ли подсветка записей с прикрепленными файлами
bool AppConfigUi::getEnableRecordWithAttachHighlight(void)
{
    return m_conf->value("enableRecordWithAttachHighlight").toBool();
}

void AppConfigUi::setEnableRecordWithAttachHighlight(bool state)
{
    m_conf->setValue("enableRecordWithAttachHighlight", state);
}

// Цвет подсветки записей с прикрепленными файлами

// Цвет подсветки записей с прикрепленными файлами
QString AppConfigUi::getRecordWithAttachHighlightColor(void)
{
    return m_host->get_parameter("recordWithAttachHighlightColor");
}

void AppConfigUi::setRecordWithAttachHighlightColor(QString color)
{
    QColor saveColor(color);

    // Если сохраняема строка действительно содержит закодированный цвет
    if (saveColor.isValid())
    {
        m_conf->setValue("recordWithAttachHighlightColor", color);
    }
}

// Цвет закраски секрета в формате "#77767b"

// Цвет закраски секрета в формате "#77767b"
QString AppConfigUi::get_secretColor(void)
{
    return m_host->get_parameter("secretColor");
}

void AppConfigUi::set_secretColor(QString color)
{
    QColor saveColor(color);

    // Если сохраняема строка действительно содержит закодированный цвет
    if (saveColor.isValid())
    {
        m_conf->setValue("secretColor", color);
    }
}

// Цвет фона поля заметки. Пустая строка означает цвет темы по умолчанию.
// Параметр намеренно не входит в версионные таблицы (как editorFindEmbedded):
// отсутствующий ключ покрывается значением по умолчанию, миграция conf не нужна

// Цвет фона поля заметки. Пустая строка означает цвет темы по умолчанию.
// Параметр намеренно не входит в версионные таблицы (как editorFindEmbedded):
// отсутствующий ключ покрывается значением по умолчанию, миграция conf не нужна
QString AppConfigUi::get_notePaperColor(void)
{
    return m_conf->value("notePaperColor", "").toString();
}

void AppConfigUi::set_notePaperColor(QString color)
{
    // Пустая строка разрешена: это возврат к цвету темы
    if(color.isEmpty())
    {
        m_conf->setValue("notePaperColor", "");
        return;
    }

    QColor saveColor(color);

    // Если сохраняема строка действительно содержит закодированный цвет
    if (saveColor.isValid())
    {
        m_conf->setValue("notePaperColor", color);
    }
}

// Цвет шрифта поля заметки. Пустая строка означает цвет темы по умолчанию.
// Параметр намеренно не входит в версионные таблицы (как notePaperColor):
// отсутствующий ключ покрывается значением по умолчанию, миграция conf не нужна

// Цвет шрифта поля заметки. Пустая строка означает цвет темы по умолчанию.
// Параметр намеренно не входит в версионные таблицы (как notePaperColor):
// отсутствующий ключ покрывается значением по умолчанию, миграция conf не нужна
QString AppConfigUi::get_noteFontColor(void)
{
    return m_conf->value("noteFontColor", "").toString();
}

void AppConfigUi::set_noteFontColor(QString color)
{
    // Пустая строка разрешена: это возврат к цвету темы
    if(color.isEmpty())
    {
        m_conf->setValue("noteFontColor", "");
        return;
    }

    QColor saveColor(color);

    // Если сохраняема строка действительно содержит закодированный цвет
    if (saveColor.isValid())
    {
        m_conf->setValue("noteFontColor", color);
    }
}

// Кастомный дизайн поверх темы. Параметры намеренно не входят
// в версионные таблицы (как noteFontColor): отсутствующий ключ
// покрывается значением по умолчанию, миграция conf не нужна

// Кастомный дизайн поверх темы. Параметры намеренно не входят
// в версионные таблицы (как noteFontColor): отсутствующий ключ
// покрывается значением по умолчанию, миграция conf не нужна
QString AppConfigUi::get_themeCustomTreeBackground(void)
{
    return m_conf->value("themeCustomTreeBackground", "").toString();
}

void AppConfigUi::set_themeCustomTreeBackground(QString color)
{
    m_conf->setValue("themeCustomTreeBackground", color);
}

QString AppConfigUi::get_themeCustomTreeText(void)
{
    return m_conf->value("themeCustomTreeText", "").toString();
}

void AppConfigUi::set_themeCustomTreeText(QString color)
{
    m_conf->setValue("themeCustomTreeText", color);
}

QString AppConfigUi::get_themeCustomTableBackground(void)
{
    return m_conf->value("themeCustomTableBackground", "").toString();
}

void AppConfigUi::set_themeCustomTableBackground(QString color)
{
    m_conf->setValue("themeCustomTableBackground", color);
}

QString AppConfigUi::get_themeCustomTableText(void)
{
    return m_conf->value("themeCustomTableText", "").toString();
}

void AppConfigUi::set_themeCustomTableText(QString color)
{
    m_conf->setValue("themeCustomTableText", color);
}

QString AppConfigUi::get_themeCustomTableAlternate(void)
{
    return m_conf->value("themeCustomTableAlternate", "").toString();
}

void AppConfigUi::set_themeCustomTableAlternate(QString color)
{
    m_conf->setValue("themeCustomTableAlternate", color);
}

QString AppConfigUi::get_themeCustomInterfaceFontFamily(void)
{
    return m_conf->value("themeCustomInterfaceFontFamily", "").toString();
}

void AppConfigUi::set_themeCustomInterfaceFontFamily(QString family)
{
    m_conf->setValue("themeCustomInterfaceFontFamily", family);
}

int AppConfigUi::get_themeCustomInterfaceFontSize(void)
{
    return m_conf->value("themeCustomInterfaceFontSize", -1).toInt();
}

void AppConfigUi::set_themeCustomInterfaceFontSize(int size)
{
    m_conf->setValue("themeCustomInterfaceFontSize", size);
}

int AppConfigUi::get_themeCustomTreeFontSize(void)
{
    return m_conf->value("themeCustomTreeFontSize", -1).toInt();
}

void AppConfigUi::set_themeCustomTreeFontSize(int size)
{
    m_conf->setValue("themeCustomTreeFontSize", size);
}

int AppConfigUi::get_themeCustomTableFontSize(void)
{
    return m_conf->value("themeCustomTableFontSize", -1).toInt();
}

void AppConfigUi::set_themeCustomTableFontSize(int size)
{
    m_conf->setValue("themeCustomTableFontSize", size);
}

int AppConfigUi::get_themeCustomTreeIndentation(void)
{
    return m_conf->value("themeCustomTreeIndentation", -1).toInt();
}

void AppConfigUi::set_themeCustomTreeIndentation(int pixels)
{
    m_conf->setValue("themeCustomTreeIndentation", pixels);
}

int AppConfigUi::get_themeCustomTableRowHeight(void)
{
    return m_conf->value("themeCustomTableRowHeight", -1).toInt();
}

void AppConfigUi::set_themeCustomTableRowHeight(int pixels)
{
    m_conf->setValue("themeCustomTableRowHeight", pixels);
}

QString AppConfigUi::get_themeCustomNotePaperImage(void)
{
    return m_conf->value("themeCustomNotePaperImage", "").toString();
}

void AppConfigUi::set_themeCustomNotePaperImage(QString path)
{
    m_conf->setValue("themeCustomNotePaperImage", path);
}

int AppConfigUi::get_themeCustomNotePaperBuiltin(void)
{
    return m_conf->value("themeCustomNotePaperBuiltin", 0).toInt();
}

void AppConfigUi::set_themeCustomNotePaperBuiltin(int index)
{
    // Верхняя граница не проверяется здесь: чтение идёт через
    // value() с дефолтом, запись только из выпадашки на четыре пункта
    if(index<0)
        index=0;

    m_conf->setValue("themeCustomNotePaperBuiltin", index);
}

QString AppConfigUi::get_themeCustomNotePaperMode(void)
{
    return m_conf->value("themeCustomNotePaperMode", "texture").toString();
}

void AppConfigUi::set_themeCustomNotePaperMode(QString mode)
{
    if(mode!="color" && mode!="texture")
        mode="texture";

    m_conf->setValue("themeCustomNotePaperMode", mode);
}

QString AppConfigUi::get_themeCustomBaseTheme(void)
{
    return m_conf->value("themeCustomBaseTheme", "default").toString();
}

void AppConfigUi::set_themeCustomBaseTheme(QString name)
{
    if(name!="default" && name!="dark" &&
       name!="modern" && name!="midnight")
        name="default";

    m_conf->setValue("themeCustomBaseTheme", name);
}

// Человеко-читаемые имена вложенных файлов на диске.
// Параметр намеренно не входит в версионные таблицы (как noteFontColor):
// отсутствующий ключ покрывается значением по умолчанию, миграция conf не нужна

bool AppConfigUi::get_recordHeaderVisible(void)
{
    return m_conf->value("recordHeaderVisible", true).toBool();
}

void AppConfigUi::set_recordHeaderVisible(bool state)
{
    m_conf->setValue("recordHeaderVisible", state);
}

bool AppConfigUi::get_recordTableUnderTree(void)
{
    return m_conf->value("recordTableUnderTree", false).toBool();
}

void AppConfigUi::set_recordTableUnderTree(bool state)
{
    m_conf->setValue("recordTableUnderTree", state);
}

// QSettings хранит строку с запятыми как QStringList, поэтому
// value.toString() для такого ключа дает пустоту. Разбираются оба
// варианта: и список, и одиночная строка. Заодно срезаются пробелы
// после запятых, которые мог оставить ручной редактор conf.ini
static QStringList readToolbarOrder(const QVariant &value)
{
    QStringList result;

    foreach(const QString &part, value.toStringList())
        foreach(const QString &item, part.split(","))
            if(!item.trimmed().isEmpty())
                result.append(item.trimmed());

    return result;
}

QStringList AppConfigUi::getTreeToolbarOrder(void)
{
    return readToolbarOrder(m_conf->value("treeToolbarOrder", ""));
}

void AppConfigUi::setTreeToolbarOrder(QStringList order)
{
    m_conf->setValue("treeToolbarOrder", order.join(","));
}

QStringList AppConfigUi::getRecordToolbarOrder(void)
{
    return readToolbarOrder(m_conf->value("recordToolbarOrder", ""));
}

void AppConfigUi::setRecordToolbarOrder(QStringList order)
{
    m_conf->setValue("recordToolbarOrder", order.join(","));
}

QStringList AppConfigUi::getRecordExtraToolbarOrder(void)
{
    return readToolbarOrder(m_conf->value("recordExtraToolbarOrder", ""));
}

void AppConfigUi::setRecordExtraToolbarOrder(QStringList order)
{
    m_conf->setValue("recordExtraToolbarOrder", order.join(","));
}

int AppConfigUi::getPreviewIconSize(void)
{
    return m_conf->value("previewIconSize",0).toInt();
}

void AppConfigUi::setPreviewIconSize(int n)
{
    m_conf->setValue("previewIconSize", n);
}

int AppConfigUi::get_statsMode(void)
{
    const int mode=m_conf->value("statsMode", 0).toInt();

    return qBound(0, mode, 2);
}

void AppConfigUi::set_statsMode(int mode)
{
    m_conf->setValue("statsMode", qBound(0, mode, 2));
}

int AppConfigUi::get_statsLimit(void)
{
    const int limit=m_conf->value("statsLimit", 1000).toInt();

    return qBound(0, limit, 1000000);
}

void AppConfigUi::set_statsLimit(int limit)
{
    m_conf->setValue("statsLimit", qBound(0, limit, 1000000));
}

QString AppConfigUi::getIconCurrentSectionName(void)
{
    return m_host->get_parameter("iconCurrentSectionName");
}

void AppConfigUi::setIconCurrentSectionName(QString name)
{
    m_conf->setValue("iconCurrentSectionName", name);
}

// Разрешена ли периодическая синхронизация

// Показывать ли общий итог записей с учетом подветок.
// По умолчанию выключено: только свои [n]
bool AppConfigUi::get_showSubtreeCount(void)
{
    return m_conf->value("showSubtreeCount", false).toBool();
}

void AppConfigUi::set_showSubtreeCount(bool state)
{
    m_conf->setValue("showSubtreeCount", state);
}

// Раскладка импорта Telegram. Неизвестное значение лечится
// в дефолт: импорт всегда рабочий

QString AppConfigUi::getDockableWindowsState(void)
{
    return m_host->get_parameter("dockableWindowsState");
}

void AppConfigUi::setDockableWindowsState(QString state)
{
    m_conf->setValue("dockableWindowsState", state);
}

QString AppConfigUi::getDockableWindowsBehavior(void)
{
    return m_host->get_parameter("dockableWindowsBehavior");
}

void AppConfigUi::setDockableWindowsBehavior(QString mode)
{
    if (mode=="single" || mode=="together")
    {
        m_conf->setValue("dockableWindowsBehavior", mode);
    }
    else
    {
        criticalError("Unavailable value for dockableWindowsBehavior "+mode);
    }
}

// Получение имени темы оформления

// Получение имени темы оформления
QString AppConfigUi::getInterfaceTheme()
{
    return m_host->get_parameter("interfaceTheme");
}

bool AppConfigUi::setInterfaceTheme(QString themeName)
{
    if ( !AppContext::instance().fixedParameters()->themesAvailableList.contains(themeName) )
    {
        return false;
    }

    m_conf->setValue("interfaceTheme", themeName);

    return true;
}

QString AppConfigUi::getInterfaceIconSize()
{
    return m_host->get_parameter("interfaceIconSize");
}

bool AppConfigUi::setInterfaceIconSize(QString sizeName)
{
    // Если установлено какое-то имя размера иконок,
    // и оно не входит в список разрешенных
    if ( sizeName!="" and
         !AppContext::instance().fixedParameters()->interfaceIconSizeAvailableMap.keys().contains(sizeName) )
    {
        return false;
    }

    // Запоминается имя размера иконок. Оно может быть пустым или
    // в виде строки из списка разрешенных размеров
    m_conf->setValue("interfaceIconSize", sizeName);

    return true;
}

// --------------------
// Номер версии конфига
// --------------------
