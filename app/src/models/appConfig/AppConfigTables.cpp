#include <functional>
#include <QtGlobal>
#include <QRect>
#include <QVariant>
#include <QTextCodec>

#include "AppConfig.h"
#include "AppConfigUpdater.h"
#include "AppConfigGeneral.h"

#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "libraries/helpers/DebugHelper.h"
#include "libraries/helpers/DiskHelper.h"
#include "libraries/AppContext.h"

QStringList AppConfig::removeParameterFromTable(QString removeName, QStringList table)
{
    // Перебираются параметры в таблице. Граница - размер списка,
    // а не MYTETRA_CONFIG_PARAM_NUM: таблица без сигнатуры конца
    // короче лимита, а отсутствующая сигнатура раньше давала
    // выход за границы в table.at() (см. fix/537)
    for(int i=0; (i+1)*MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD<=table.size(); i++)
    {
        // Выясняется имя параметра
        QString name=table.at(i*MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD);

        // Сигнатура конца таблицы
        if(name=="0")
            break;

        // Если имя совпадает с удаляемым
        if (name==removeName)
        {
            // Удаляются данные об элементе (удаляется ячейка с одним и тем же номером
            // столько раз, сколько полей в таблице)
            for (int j=0; j<MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD; j++)
            {
                table.removeAt(i*MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD);
            }

            break;
        }
    }

    return table;
}

// Получение типа параметра в виде строки

// Получение типа параметра в виде строки
QString AppConfig::getParameterTypeFromTable(QString parameterName, QStringList table)
{
    // Перебираются параметры в таблице. Граница - размер списка
    // (см. removeParameterFromTable)
    for(int i=0; (i+1)*MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD<=table.size(); i++)
    {
        // Выясняется имя параметра
        QString name=table.at(i*MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD);

        // Сигнатура конца таблицы
        if(name=="0")
            break;

        if (name==parameterName)
        {
            return table.at(i*MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD+1);
        }
    }

    return "";
}

// Получение значения параметра по-умолчнию, в виде строки

// Получение значения параметра по-умолчнию, в виде строки
QString AppConfig::getParameterDefaultValueFromTable(QString parameterName, QStringList table)
{
    // Перебираются параметры в таблице. Граница - размер списка
    // (см. removeParameterFromTable)
    for(int i=0; (i+1)*MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD<=table.size(); i++)
    {
        // Выясняется имя параметра
        QString name=table.at(i*MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD);

        // Сигнатура конца таблицы
        if(name=="0")
            break;

        if (name==parameterName)
        {
            return table.at(i*MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD+2);
        }
    }

    return "";
}

// Замена типа и значения параметра по-умолчанию

// Замена типа и значения параметра по-умолчанию
QStringList AppConfig::replaceParameterInTable(QString replaceName,
                                               QString replaceType,
                                               QString replaceDefaultValue,
                                               QStringList table)
{
    // Перебираются параметры в таблице. Граница - размер списка
    // (см. removeParameterFromTable)
    for(int i=0; (i+1)*MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD<=table.size(); i++)
    {
        // Выясняется имя параметра
        QString name=table.at(i*MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD);

        // Сигнатура конца таблицы
        if(name=="0")
            break;

        // Если имя совпадает с заменяемым
        if (name==replaceName)
        {
            table[i*MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD+1]=replaceType;
            table[i*MYTETRA_CONFIG_PARAM_FIELDS_AT_RECORD+2]=replaceDefaultValue;

            break;
        }
    }

    return table;
}

// ------------------------------------
// Методы для обновления версии конфига
// ------------------------------------

void AppConfig::update_version_process(void)
{
    QString configFileName=AppContext::instance().globalParameters()->getWorkDirectory()+"/conf.ini";

    AppConfigUpdater updater;
    updater.setConfigFile(configFileName);

    // Номер версии текущего конфига, который есть на начало обновления
    int fromVersion=generalSection()->get_config_version();

    // Эта строка компилируется нормально, сделать на ее основе список указателей на функцию
    // std::function< QStringList(AppConfig&, bool) > pFunction=&AppConfig::get_parameter_table_1;

    // Перечень функций, выдающих список параметров для конкретной версии конфига
    QList< std::function< QStringList(AppConfig&, bool) > > parameterFunctions;

    // Исторически счет версий идет с 1, поэтому, чтобы не запутаться,
    // создается пустой нуливой элемент
    parameterFunctions << nullptr;

    parameterFunctions << &AppConfig::get_parameter_table_1;
    parameterFunctions << &AppConfig::get_parameter_table_2;
    parameterFunctions << &AppConfig::get_parameter_table_3;
    parameterFunctions << &AppConfig::get_parameter_table_4;
    parameterFunctions << &AppConfig::get_parameter_table_5;
    parameterFunctions << &AppConfig::get_parameter_table_6;
    parameterFunctions << &AppConfig::get_parameter_table_7;
    parameterFunctions << &AppConfig::get_parameter_table_8;
    parameterFunctions << &AppConfig::get_parameter_table_9;
    parameterFunctions << &AppConfig::get_parameter_table_10;
    parameterFunctions << &AppConfig::get_parameter_table_11;
    parameterFunctions << &AppConfig::get_parameter_table_12;
    parameterFunctions << &AppConfig::get_parameter_table_13;
    parameterFunctions << &AppConfig::get_parameter_table_14;
    parameterFunctions << &AppConfig::get_parameter_table_15;
    parameterFunctions << &AppConfig::get_parameter_table_16;
    parameterFunctions << &AppConfig::get_parameter_table_17;
    parameterFunctions << &AppConfig::get_parameter_table_18;
    parameterFunctions << &AppConfig::get_parameter_table_19;
    parameterFunctions << &AppConfig::get_parameter_table_20;
    parameterFunctions << &AppConfig::get_parameter_table_21;
    parameterFunctions << &AppConfig::get_parameter_table_22;
    parameterFunctions << &AppConfig::get_parameter_table_23;
    parameterFunctions << &AppConfig::get_parameter_table_24;
    parameterFunctions << &AppConfig::get_parameter_table_25;
    parameterFunctions << &AppConfig::get_parameter_table_26;
    parameterFunctions << &AppConfig::get_parameter_table_27;
    parameterFunctions << &AppConfig::get_parameter_table_28;
    parameterFunctions << &AppConfig::get_parameter_table_29;
    parameterFunctions << &AppConfig::get_parameter_table_30;
    parameterFunctions << &AppConfig::get_parameter_table_31;
    parameterFunctions << &AppConfig::get_parameter_table_32;
    parameterFunctions << &AppConfig::get_parameter_table_33;
    parameterFunctions << &AppConfig::get_parameter_table_34;
    parameterFunctions << &AppConfig::get_parameter_table_35;
    parameterFunctions << &AppConfig::get_parameter_table_36;
    parameterFunctions << &AppConfig::get_parameter_table_37;
    parameterFunctions << &AppConfig::get_parameter_table_38;
    parameterFunctions << &AppConfig::get_parameter_table_39;
    parameterFunctions << &AppConfig::get_parameter_table_40;
    parameterFunctions << &AppConfig::get_parameter_table_41;
    parameterFunctions << &AppConfig::get_parameter_table_42;
    parameterFunctions << &AppConfig::get_parameter_table_43;
    parameterFunctions << &AppConfig::get_parameter_table_44;
    parameterFunctions << &AppConfig::get_parameter_table_45;
    parameterFunctions << &AppConfig::get_parameter_table_46;
    parameterFunctions << &AppConfig::get_parameter_table_47;
    parameterFunctions << &AppConfig::get_parameter_table_48;
    parameterFunctions << &AppConfig::get_parameter_table_49;
    parameterFunctions << &AppConfig::get_parameter_table_50;
    parameterFunctions << &AppConfig::get_parameter_table_51;
    parameterFunctions << &AppConfig::get_parameter_table_52;
    parameterFunctions << &AppConfig::get_parameter_table_53;
    parameterFunctions << &AppConfig::get_parameter_table_54;
    parameterFunctions << &AppConfig::get_parameter_table_55;
    parameterFunctions << &AppConfig::get_parameter_table_56;
    parameterFunctions << &AppConfig::get_parameter_table_57;
    parameterFunctions << &AppConfig::get_parameter_table_58;
    parameterFunctions << &AppConfig::get_parameter_table_59;
    parameterFunctions << &AppConfig::get_parameter_table_60;
    parameterFunctions << &AppConfig::get_parameter_table_61;
    parameterFunctions << &AppConfig::get_parameter_table_62;

    for (int i=1; i<parameterFunctions.count()-1; ++i)
    {
        // Обновление идет только для версий, которые более новые чем версия текущего конфига
        if (i>=fromVersion)
        {
            // В вызове метода: *this - это объект, а true - это первый параметр метода.
            // Именно так работает вызов метода для std::function
            updater.updateVersion(i,
                                  i+1,
                                  (parameterFunctions.at(i))(*this, true),
                                  (parameterFunctions.at(i+1))(*this, true) );
        }
    }
}

QStringList AppConfig::get_parameter_table_1(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    table << "addnewrecord_expand_info" << "int" << "1";
    table << "findscreen_find_inname"   << "bool" << "true";
    table << "findscreen_find_intags"   << "bool" << "true";
    table << "findscreen_find_intext"   << "bool" << "true";
    table << "findscreen_howextract"    << "int" << "1";
    table << "findscreen_show"          << "bool" << "false";
    table << "findscreen_wordregard"    << "int" << "1";
    table << "findsplitter_size_list"   << "QString" << "517,141";
    table << "hspl_size_list"           << "QString" << "237,621";
    table << "lastidnum"                << "int" << "3537";
    table << "lastnotenum"              << "int" << "3119";
    table << "lastprefixnum"            << "int" << "7540";
    table << "mainwingeometry"          << "QString" << "155,24,864,711)";
    table << "recordtable_position"     << "int" << "0";
    table << "tetradir"                 << "QString" << "/opt/mytetra/data";
    table << "trashdir"                 << "QString" << "/opt/mytetra/trash";
    table << "trashmaxfilecount"        << "int" << "200";
    table << "trashsize"                << "int" << "5";
    table << "tree_position"            << "QString" << "0,1818,1819";
    table << "vspl_size_list"           << "QString" << "171,487";
    table << "findscreen_find_inauthor" << "bool" << "true";
    table << "findscreen_find_inurl"    << "bool" << "false";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_2(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 1
    table << get_parameter_table_1(false);

    // Новый параметр
    table << "cutbranchconfirm"         << "bool" << "true";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_3(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 2
    table << get_parameter_table_2(false);

    // Новый параметр
    table << "printdebugmessages"         << "bool" << "false";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_4(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 3
    table << get_parameter_table_3(false);

    // Новые параметры
    table << "interfacelanguage" << "QString" << "en";
    table << "programm" << "QString" << "mytetra";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_5(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 4
    table << get_parameter_table_4(false);

    // Исключаются ненужные в новой версии параметры
    table=removeParameterFromTable("lastidnum", table);
    table=removeParameterFromTable("lastnotenum", table);
    table=removeParameterFromTable("lastprefixnum", table);

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_6(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 5
    table << get_parameter_table_5(false);

    // Новые параметры
    table << "howpassrequest" << "QString" << "atClickOnCryptBranch";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_7(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 6
    table << get_parameter_table_6(false);

    // Новые параметры
    table << "runinminimizedwindow" << "bool" << "false";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_8(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 7
    table << get_parameter_table_7(false);

    // Новые параметры
    table << "synchrocommand" << "QString" << "";
    table << "synchroonstartup" << "bool" << "false";
    table << "synchroonexit" << "bool" << "false";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_9(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 8
    table << get_parameter_table_8(false);

    // Новые параметры
    table << "syncroConsoleDetails" << "bool" << "false";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_10(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 9
    table << get_parameter_table_9(false);

    // Новые параметры
    table << "autoClosePasswordEnable" << "bool" << "false";
    table << "autoClosePasswordDelay" << "int" << "10";
    table << "autosaveEnable" << "bool" << "true";
    table << "autosaveInterval" << "int" << "10";
    table << "historySquashEnable" << "bool" << "true";
    table << "historySquashKeep" << "int" << "100";
    table << "historySquashMode" << "int" << "0";
    table << "historyEnable" << "bool" << "false";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_11(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 10
    table << get_parameter_table_10(false);

    // Новые параметры
    table << "editorCursorPosition" << "int" << "0";
    table << "editorScrollBarPosition" << "int" << "0";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_12(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 11
    table << get_parameter_table_11(false);

    // Новые параметры
    table << "passwordMiddleHash" << "QString" << "";
    table << "passwordSaveFlag" << "bool" << "false";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_13(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 12
    table << get_parameter_table_12(false);

    // Новые параметры
    table << "rememberCursorAtHistoryNavigation" << "bool" << "true";
    table << "rememberCursorAtOrdinarySelection" << "bool" << "true";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_14(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 13
    table << get_parameter_table_13(false);

    // Новые параметры
    table << "findScreenTreeSearchArea" << "int" << "0"; // Область поиска. Искать во всем дереве - 0, искать в текущей ветке - 1

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_15(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 14
    table << get_parameter_table_14(false);

    // Новые параметры
    if (AppContext::instance().globalParameters()->getTargetOs()=="android")
        table << "uglyQssReplaceHeightForTableView" << "int" << "35"; // Так как не все параметры можно стилизовать через QSS, здесь задается высота ячейки таблицы
    else
        table << "uglyQssReplaceHeightForTableView" << "int" << "0";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_16(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 15
    table << get_parameter_table_15(false);

    // Новые параметры
    table << "recordTableShowFields" << "QString" << "name,tags";
    table << "recordTableFieldsWidth" << "QString" << "256,128";
    table << "recordTableShowHorizontalHeaders" << "bool" << "true";
    table << "recordTableShowVerticalHeaders" << "bool" << "false";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_17(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 16
    table << get_parameter_table_16(false);

    // Новые параметры
    if (AppContext::instance().globalParameters()->getTargetOs()=="android")
        table << "showSplashScreen" << "bool" << "true"; // В Андроид долгий запуск, нужно показывать сплешскрин
    else
        table << "showSplashScreen" << "bool" << "false"; // На десктопе быстрый запуск, сплешскрин только мешает

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_18(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 17
    table << get_parameter_table_17(false);

    // Новые параметры
    if (AppContext::instance().globalParameters()->getTargetOs()=="android")
        table << "interfaceMode" << "QString" << "mobile"; // В Андроид должен быть мобильный интерфейс
    else
        table << "interfaceMode" << "QString" << "desktop"; // На десктопе должен быть интерфейс адаптированный для работы на рабочем столе

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_19(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 18
    table << get_parameter_table_18(false);

    // Новые параметры
    table << "focusWidget" << "QString" << "";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_20(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 19
    table << get_parameter_table_19(false);

    // Новые параметры
    if (AppContext::instance().globalParameters()->getTargetOs()=="android")
        table << "hideEditorTools" << "QString" << "italic,underline,monospace,alignleft,aligncenter,alignright,alignwidth,numericlist,dotlist,indentplus,indentminus,showformatting,showhtml,fontcolor,backgroundcolor,expand_edit_area,save,createtable,table_add_row,table_remove_row,table_add_col,table_remove_col,table_merge_cells,table_split_cell"; // В Андроид прячутся инструменты сложного форматирования текста
    else
        table << "hideEditorTools" << "QString" << ""; // На десктопе скрываемых кнопок редактора нет

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_21(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 20
    table << get_parameter_table_20(false);

    table << "findInBaseExpand" << "bool" << "true";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_22(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 21
    table << get_parameter_table_21(false);

    table << "recordtableSelectedRecordId" << "QString" << "";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_23(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 22
    table << get_parameter_table_22(false);

    // Исключаются ненужные в новой версии параметры
    table=removeParameterFromTable("recordtable_position", table);

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_24(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 23
    table << get_parameter_table_23(false);

    table << "enableCustomDateTimeFormat" << "bool" << "false";
    table << "customDateTimeFormat" << "QString" << "";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_25(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 24
    table << get_parameter_table_24(false);

    table << "attachAppendDir" << "QString" << "";
    table << "attachSaveAsDir" << "QString" << "";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_26(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 25
    table << get_parameter_table_25(false);

    table << "enableDecryptFileToTrashDirectory" << "bool" << "false";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_27(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 26
    table << get_parameter_table_26(false);

    table << "actionLogMaximumSize" << "int" << "1"; // 1 Мб

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_28(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 27
    table << get_parameter_table_27(false);

    table << "enableLogging" << "bool" << "false";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_29(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 28
    table << get_parameter_table_28(false);

    table << "enableRecordWithAttachHighlight" << "bool" << "true";
    table << "recordWithAttachHighlightColor" << "QString" << "#d1e3c5";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_30(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 29
    table << get_parameter_table_29(false);

    table << "enablePeriodicCheckBase" << "bool" << "false";
    table << "checkBasePeriod" << "int" << "20";
    table << "enablePeriodicCheckMessage" << "bool" << "false";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_31(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 30
    table << get_parameter_table_30(false);

    table << "previewIconSize" << "int" << "24";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_32(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 31
    table << get_parameter_table_31(false);

    table << "iconCurrentSectionName" << "QString" << "Essential";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_33(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 32
    table << get_parameter_table_32(false);

    table << "enablePeriodicSyncro" << "bool" << "false";
    table << "periodicSyncroPeriod" << "int" << "300";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_34(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 33
    table << get_parameter_table_33(false);

    table << "enableCreateEmptyRecord" << "bool" << "false";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_35(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 34
    table << get_parameter_table_34(false);

    // Новый флаг, поиск по названию ветки
    table << "findscreen_find_innameItem" << "bool" << "false";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_36(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 35
    table << get_parameter_table_35(false);

    // Состояние окрепляемых окон
    // Предполагаемый формат - "IDзаписи1,x1,y1,w1,h1;IDзаписи2,x2,y2,w2,h2..."
    table << "dockableWindowsState" << "QString" << "";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_37(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 36
    table << get_parameter_table_36(false);

    // Поведение окрепляемых окон
    table << "dockableWindowsBehavior" << "QString" << "single";

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_38(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 37
    table << get_parameter_table_37(false);

    // Обновление формата конфига не происходит, обновляется только
    // представление параметра dockableWindowsState
    // см. AppConfigUpdater::updateValueRepresentation

    if (withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_39(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 38
    table << get_parameter_table_38(false);

    // Стандартная тема оформления
    table << "theme" << "QString" << "light";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_40(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 39
    table << get_parameter_table_39(false);

    // Обновление формата конфига не происходит, обновляется только
    // представление параметра theme
    // см. AppConfigUpdater::updateValueRepresentation

    // Теперь допустимые значения theme - default и dark, темы light нет

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_41(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 40
    table << get_parameter_table_40(false);

    // Пример переименования значения theme в themeName
    // Пока такое решение рабочее, но нерекомендовано

    // Значение старого параметра theme
    // QString themeName = this->m_conf->value("theme").toString();

    // Запоминается тема оформления в новом параметре
    // table << "interfaceTheme" << "QString" << themeName;

    // Исключаются ненужные в новой версии параметры
    table=removeParameterFromTable("theme", table);

    // Новый параметр, хранящий тему оформления
    table << "interfaceTheme" << "QString" << "default";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_42(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 41
    table << get_parameter_table_41(false);

    // Размер иконок.
    // Размер обозначается специальными строками, начинающимися на "META_ICON_"
    // Если размер пустой, используется системный размер иконок
    table << "interfaceIconSize" << "QString" << "";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_43(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 42
    table << get_parameter_table_42(false);

    // Цвет закраски секрета в формате "#77767b"
    table << "secretColor" << "QString" << "#77767b";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_44(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 43
    table << get_parameter_table_43(false);

    // Шапка заметки показана
    table << "recordHeaderVisible" << "bool" << "true";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_45(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 44
    table << get_parameter_table_44(false);

    // Веб-клиппер: лимиты картинок, включение, глобальный хоткей (X11),
    // запомненная ветка unsorted_notes
    table << "clipperMaxImages" << "int" << "20";
    table << "clipperMaxImageSizeMb" << "int" << "5";
    table << "clipperenable" << "bool" << "true";
    table << "clipperhotkey" << "QString" << "Ctrl+Alt+V";
    table << "clipperbranchid" << "QString" << "";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_46(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Ремонтная версия без новых ключей: миграция 44->45 могла усечься
    // лимитом апдейтера, и conf.ini мог остаться без клиппер-ключей.
    // Перепрогон добавляет недостающее дефолтами, имеющиеся не трогает
    table << get_parameter_table_45(false);

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_47(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Второй ремонтный перепрогон: conf.ini со штампом версии 46 из других
    // веток может не содержать клиппер-ключей. Состав тот же, что в 45
    table << get_parameter_table_46(false);

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_48(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 47
    table << get_parameter_table_47(false);

    // Глобальный режим только чтения (без изменений базы)
    table << "readOnly" << "bool" << "false";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_49(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 48
    table << get_parameter_table_48(false);

    // Разрешение доступа внешнего AI-клиента через MCP, по умолчанию закрыт
    table << "mcpAllow" << "bool" << "false";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_50(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 49
    table << get_parameter_table_49(false);

    // Общий итог записей с учетом подветок, по умолчанию выключен
    table << "showSubtreeCount" << "bool" << "false";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_51(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 50
    table << get_parameter_table_50(false);

    // Раскладка импорта Telegram, по умолчанию заметка на месяц
    table << "telegramImportMode" << "QString" << "month";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_52(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 51
    table << get_parameter_table_51(false);

    // Метод шифрования, по умолчанию RC5 ради совместимости.
    // Новое значение применяется только к новым шифрованиям,
    // расшифровка отличает шифры по магии блоба

    table << "cryptMethod" << "QString" << "rc5";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_53(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 52
    table << get_parameter_table_52(false);

    // Игнорировать ошибки проверки TLS при загрузке картинок/файлов.
    // По умолчанию false: небезопасный режим только по явному согласию
    table << "tlsIgnoreInvalidCertificates" << "bool" << "false";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_54(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 53
    table << get_parameter_table_53(false);

    // Прыжок в текст заметки из глобального поиска: двойной клик
    // по строке открывает поиск внутри заметки с переходом к совпадению.
    // По умолчанию включен. Зависимая галка прыжка только по первому
    // слову по умолчанию выключена (ищется раннее из всех слов)
    table << "findscreen_find_injumpToNote"   << "bool" << "true";
    table << "findscreen_find_injumpFirstWord" << "bool" << "false";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_55(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 54
    table << get_parameter_table_54(false);

    // Клиппер: режим целевой ветки (0 — по умолчанию unsorted_notes,
    // 1 — выбранная ветка, 2 — спрашивать каждый раз). Дефолт 0
    // сохраняет прежнее поведение на старых конфигах
    table << "clipperbranchmode" << "int" << "0";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_56(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 55
    table << get_parameter_table_55(false);

    // Клиппер: вид опроса "куда вставить" (0 — плитка, 1 — дерево).
    // Дефолт 0 — прежний вид на старых конфигах
    table << "clipperaskview" << "int" << "0";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_57(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 56, кроме вида опроса:
    // дефолтом становится дерево
    table << removeParameterFromTable("clipperaskview", get_parameter_table_56(false));

    // Клиппер: вид опроса "куда вставить" (0 — плитка, 1 — дерево).
    // Дефолт 1 — дерево. Сохраненный выбор не трогается: апдейтер
    // хранит имеющееся значение, дефолт только для новых конфигов
    table << "clipperaskview" << "int" << "1";

    // Клиппер: недавние ветки вставок, id через ";", свежие впереди
    table << "clipperrecent" << "QString" << "";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_58(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 57, кроме вида опроса:
    // переключалка плитка/дерево убрана (единый экран: дерево с
    // фильтром сверху, частые плитки снизу). Апдейтер сам выкинет
    // ключ из конфигов: пишет только ключи финальной таблицы
    table << removeParameterFromTable("clipperaskview", get_parameter_table_57(false));

    // Клиппер: какие 16 плиток внизу опроса (0 — последние, 1 — частые)
    table << "clippertilemode" << "int" << "0";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_59(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 58
    table << get_parameter_table_58(false);

    // Клиппер: при попытке вставить дубль открывать оригинал заметки.
    // Дефолт true — поведение нужно сразу, галочка для отключения
    table << "clipperopendup" << "bool" << "true";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_60(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 59
    table << get_parameter_table_59(false);

    // Строка фильтра над деревом веток (тумблер в меню Вид).
    // Дефолт true — фильтр виден сразу
    table << "treeFilterVisible" << "bool" << "true";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}

QStringList AppConfig::get_parameter_table_61(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 60
    table << get_parameter_table_60(false);

    // Вид панели прикрепленных файлов: список или плитка.
    // Чистое добавление ключа
    table << "attachViewMode" << "QString" << "list";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}


QStringList AppConfig::get_parameter_table_62(bool withEndSignature)
{
    // Таблица параметров
    // Имя, Тип, Значение на случай когда в конфиге параметра прочему-то нет
    QStringList table;

    // Старые параметры, аналогичные версии 61
    table << get_parameter_table_61(false);

    // Режим полной статистики: 0 - спрашивать, 1 - всегда считать,
    // 2 - никогда (только быстрая). Лимит записей для автосчета
    // полной статистики без вопросов. Чистое добавление ключей
    table << "statsMode" << "int" << "0";
    table << "statsLimit" << "int" << "1000";

    if(withEndSignature)
        table << "0" << "0" << "0";

    return table;
}


QStringList AppConfig::get_actual_parameter_table(bool withEndSignature)
{
    // Актуальная таблица = последняя по номеру версии.
    // При добавлении get_parameter_table_N поменять здесь один вызов
    return get_parameter_table_62(withEndSignature);
}
