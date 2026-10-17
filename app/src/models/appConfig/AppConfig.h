#ifndef __APPCONFIG_H__
#define __APPCONFIG_H__

#include <QObject>
#include <QDir>
#include <QSettings>
#include <QDebug>

class QRect;
class QStringList;

// Класс, хранящий информацию о структуре конфига программы
// и позволяющий получить или записать значения в конфиг программы

// В структуре можно создавать новые значения и удалять устаревшие значения
// Внимание! Методики переименования значений пока не предусмотрено


class AppConfig : public QObject
{
    Q_OBJECT

public:
    AppConfig(QObject *pobj=nullptr);
    ~AppConfig();

    void init(void);
    bool is_init(void);

    void sync(void);

    // Получение имени файла, в котором хранится текущий конфиг
    // Данный метод нужен не для раскрытия значения, а только для того, чтобы
    // выводить в окне интерфейса информацию об используемой конфигурации
    QString getConfigFileName();

    // Версия формата конфигфайла
    int get_config_version(void);
    void set_config_version(int i);

    // Директория с базой знаний
    QString get_tetradir(void);
    bool set_tetradir(QString dirName);

    // Директория с корзиной
    QString get_trashdir(void);
    bool set_trashdir(QString dirName);
    
    // Максимальный размер директории корзины в мегабайтах
    unsigned int get_trashsize(void);
    bool set_trashsize(unsigned int mbSize);
    
    // Максимально допустимое число файлов в корзине
    int get_trashmaxfilecount(void);
    bool set_trashmaxfilecount(int count);

    // Нужно ли показывать подтверждение при выполнении действия "cut на ветке
    bool get_cutbranchconfirm(void);
    bool set_cutbranchconfirm(bool confirm);

    // Нужно ли печатать отладочные сообщения в консоль
    bool get_printdebugmessages(void);
    bool set_printdebugmessages(bool isPrint);

    // Язык интерфейса
    QString get_interfacelanguage(void);
    bool set_interfacelanguage(QString language);

    // Номер последнего префикса в виде строки с ведущими нулями
    QString get_addnewrecord_expand_info(void);
    void set_addnewrecord_expand_info(QString);

    // Геометрия основного окна
    QByteArray get_mainwingeometry(void);
    void set_mainwingeometry(QByteArray dataGeometry);
    

    QList<int> get_vspl_size_list(void);
    void set_vspl_size_list(QList<int> list);

    QList<int> get_hspl_size_list(void);
    void set_hspl_size_list(QList<int> list);

    QList<int> get_findsplitter_size_list(void);
    void set_findsplitter_size_list(QList<int> list);

    QList<int> get_splitter_size_list(QString name);
    void set_splitter_size_list(QString name, QList<int> list);

    
    QStringList get_tree_position(void);
    void set_tree_position(QStringList list);


    // ID записи в таблице конечных записей, которая выделена на экране
    QString get_recordtable_selected_record_id(void);
    void set_recordtable_selected_record_id(QString id);


    // Настройки виджета поиска по базе
    int get_findscreen_wordregard(void);
    void set_findscreen_wordregard(int pos);

    int get_findscreen_howextract(void);
    void set_findscreen_howextract(int pos);
    
    int getFindScreenTreeSearchArea(void);
    void setFindScreenTreeSearchArea(int pos);

    bool get_findscreen_find_in_field(QString fieldName);
    void set_findscreen_find_in_field(QString fieldName, bool isChecked);

    bool get_findscreen_show(void);
    void set_findscreen_show(bool isShow);

    QString get_howpassrequest(void);
    void set_howpassrequest(QString mode);

    bool get_runinminimizedwindow(void);
    void set_runinminimizedwindow(bool flag);

    bool get_autoClosePasswordEnable(void);
    void set_autoClosePasswordEnable(bool flag);

    // Умное автосохранение: только при реальных правках,
    // без изменений ни файла ни коммита. По умолчанию вкл, 10 мин
    bool get_autosaveEnable(void);
    void set_autosaveEnable(bool flag);

    int  get_autosaveInterval(void);
    void set_autosaveInterval(int minutes);

    // Сквош истории изменений: автосжатие снизу по лимиту.
    // По умолчанию включено, хранить 100 (genesis плюс свежие)
    bool get_historySquashEnable(void);
    void set_historySquashEnable(bool flag);

    int  get_historySquashKeep(void);
    void set_historySquashKeep(int count);

    // Стратегия сжатия (GitHelper::SquashMode).
    // По умолчанию удалять старые
    int  get_historySquashMode(void);
    void set_historySquashMode(int mode);

    // Мастер-тумблер ведения истории заметок. Выключенная история
    // не снимается и диалог не открывается. По умолчанию выключена:
    // нужен установленный git, проверяется при включении
    bool get_historyEnable(void);
    void set_historyEnable(bool flag);

    int  get_autoClosePasswordDelay(void);
    void set_autoClosePasswordDelay(int delay);

    QString get_synchrocommand(void);
    void set_synchrocommand(QString command);

    bool get_synchroonstartup(void);
    void set_synchroonstartup(bool flag);

    bool get_synchroonexit(void);
    void set_synchroonexit(bool flag);

    bool getSyncroConsoleDetails(void);
    void setSyncroConsoleDetails(bool flag);
    
    int  getEditorCursorPosition(void);
    void setEditorCursorPosition(int n);

    int  getEditorScrollBarPosition(void);
    void setEditorScrollBarPosition(int n);

    QString getPasswordMiddleHash(void);
    void setPasswordMiddleHash(QString hash);

    bool getPasswordSaveFlag(void);
    void setPasswordSaveFlag(bool flag);


    bool getRememberCursorAtHistoryNavigation(void);
    void setRememberCursorAtHistoryNavigation(bool flag);

    bool getRememberCursorAtOrdinarySelection(void);
    void setRememberCursorAtOrdinarySelection(bool flag);

    int  getUglyQssReplaceHeightForTableView(void);
    void setUglyQssReplaceHeightForTableView(int n);


    // Перечень полей, отображаемых в таблице конечных записей
    QStringList getRecordTableShowFields(void);
    void setRecordTableShowFields(QStringList fields);

    // Нужно ли отображать горизонтальные заголовки в таблице конечных записей
    bool getRecordTableShowHorizontalHeaders(void);
    void setRecordTableShowHorizontalHeaders(bool flag);

    // Нужно ли отображать вертикальные заголовки в таблице конечных записей
    bool getRecordTableShowVerticalHeaders(void);
    void setRecordTableShowVerticalHeaders(bool flag);

    // Ширина полей, отображаемых в таблице конечных записей
    QStringList getRecordTableFieldsWidth(void);
    void setRecordTableFieldsWidth(QStringList fieldsWidth);

    // Показывать ли сплешскрин при старте программы
    bool getShowSplashScreen(void);
    void setShowSplashScreen(bool isShow);

    // Режим интерфейса, возможные значения "desktop" и "mobile"
    QString getInterfaceMode(void);
    void setInterfaceMode(QString mode);

    QString getFocusWidget(void);
    void setFocusWidget(QString widgetName);

    QStringList getHideEditorTools(void);
    void setHideEditorTools(QStringList toolsNames);

    // Флаг, показывающий распахнуты ли все инструметы управления в окне поиска по базе
    bool getFindInBaseExpand(void);
    void setFindInBaseExpand(bool state);

    // Режим поиска в заметке: встраиваемая полоска (true) или отдельное окно
    // (false). По умолчанию полоска, переключается кнопками в окне,
    // на полоске и радиокнопками на странице настроек Search
    bool get_editorFindEmbedded(void);
    void set_editorFindEmbedded(bool state);

    // Панель избранного видна (по умолчанию да). Намеренно вне
    // версионных таблиц, как editorFindEmbedded
    bool get_favoritesEnabled(void);
    void set_favoritesEnabled(bool state);

    // Хранилище избранного: "sidecar" (data/favorites.xml, по умолчанию)
    // или "xml" (поля favorite в mytetra.xml). Тоже без версий
    QString get_favoritesStorage(void);
    void set_favoritesStorage(const QString &mode);

    // Разрешено ли использовать собственный формат вывода даты и времени
    bool getEnableCustomDateTimeFormat(void);
    void setEnableCustomDateTimeFormat(bool state);

    // Строка собственного формата вывода даты и времени
    QString getCustomDateTimeFormat(void);
    void setCustomDateTimeFormat(QString format);

    // Путь на диске по которому пользователь открывал файлы чтобы приаттачить их к записи
    QString getAttachAppendDir(void);
    void setAttachAppendDir(QString dir);

    // Путь на диске по которому пользователь сохранял (Сохранить как...) приаттаченные файлы
    QString getAttachSaveAsDir(void);
    void setAttachSaveAsDir(QString dir);

    // Разрешать ли для просмотра расшифровывать зашифрованные файлы в директорию корзины MyTetra
    bool getEnableDecryptFileToTrashDirectory(void);
    void setEnableDecryptFileToTrashDirectory(bool state);

    // Размер файла лога дейсвтвий
    unsigned int getActionLogMaximumSize();
    void setActionLogMaximumSize(unsigned int mbSize);

    // Разрешено ли логирование
    bool getEnableLogging(void);
    void setEnableLogging(bool state);

    // Разрешена ли подсветка записей с прикрепленными файлами
    bool getEnableRecordWithAttachHighlight(void);
    void setEnableRecordWithAttachHighlight(bool state);

    // Цвет подсветки записей с прикрепленными файлами
    QString getRecordWithAttachHighlightColor(void);
    void setRecordWithAttachHighlightColor(QString color); // Формат цвета - "#0099FF"
    // Цвет закраски секрета в формате "#77767b"
    QString get_secretColor(void);
    void set_secretColor(QString color);

    // Цвет фона поля заметки. Пустая строка означает цвет темы
    // по умолчанию, иначе формат цвета - "#B0BAC7"
    QString get_notePaperColor(void);
    void set_notePaperColor(QString color);

    // Цвет шрифта поля заметки. Пустая строка означает цвет темы
    // по умолчанию, иначе формат цвета - "#000000"
    QString get_noteFontColor(void);
    void set_noteFontColor(QString color);

    // Кастомный дизайн поверх темы. Пусто/-1 означает цвет/размер темы,
    // иначе значение пользователя. Применяется CssHelper::applyCustomDesign
    QString get_themeCustomTreeBackground(void);
    void set_themeCustomTreeBackground(QString color);
    QString get_themeCustomTreeText(void);
    void set_themeCustomTreeText(QString color);
    QString get_themeCustomTableBackground(void);
    void set_themeCustomTableBackground(QString color);
    QString get_themeCustomTableText(void);
    void set_themeCustomTableText(QString color);
    QString get_themeCustomTableAlternate(void);
    void set_themeCustomTableAlternate(QString color);
    QString get_themeCustomInterfaceFontFamily(void);
    void set_themeCustomInterfaceFontFamily(QString family);
    int get_themeCustomInterfaceFontSize(void);
    void set_themeCustomInterfaceFontSize(int size);
    int get_themeCustomTreeFontSize(void);
    void set_themeCustomTreeFontSize(int size);
    int get_themeCustomTableFontSize(void);
    void set_themeCustomTableFontSize(int size);
    int get_themeCustomTreeIndentation(void);
    void set_themeCustomTreeIndentation(int pixels);
    int get_themeCustomTableRowHeight(void);
    void set_themeCustomTableRowHeight(int pixels);

    // Мозаичный фон заметок: тайл повторяется по обеим осям.
    // Пусто означает без картинки. Повторяется по обеим осям
    QString get_themeCustomNotePaperImage(void);
    void set_themeCustomNotePaperImage(QString path);

    // Индекс встроенного тайла (0 - tx_fabric.jpg). Свой файл выше
    // имеет приоритет над индексом
    int get_themeCustomNotePaperBuiltin(void);
    void set_themeCustomNotePaperBuiltin(int index);

    // Режим фона заметок: "color" или "texture". Текстура без своего
    // файла означает встроенный tx_fabric.jpg
    QString get_themeCustomNotePaperMode(void);
    void set_themeCustomNotePaperMode(QString mode);

    // Базовая тема для Custom: одна из четырех (default, dark,
    // modern, midnight). Сами базовые темы не меняются никогда
    QString get_themeCustomBaseTheme(void);
    void set_themeCustomBaseTheme(QString name);

    // Человеко-читаемые имена вложенных файлов на диске (схема id_имя.ext
    // вместо id.ext). По умолчанию выключено. Включается кнопкой миграции
    // в настройках прикрепленных файлов, после чего новые вложения
    // сразу сохраняются по человеко-читаемой схеме.
    // Устаревший флаг: теперь схема задается режимом attachNameMode,
    // методы оставлены для совместимости (true = гибридный режим)
    bool get_attachHumanReadableNames(void);
    void set_attachHumanReadableNames(bool state);

    // Режим имен вложенных файлов на диске:
    // 0 - системные (id.ext), 1 - гибридные (id_имя.ext),
    // 2 - оригинальные (имя как при загрузке). По умолчанию 0.
    // Если ключ режима отсутствует, а устаревший флаг выставлен,
    // считается выбранным гибридный режим
    enum AttachNameMode { AttachNameSystem=0, AttachNameHybrid=1, AttachNameOriginal=2 };
    int get_attachNameMode(void);
    void set_attachNameMode(int mode);

    // Шапка заметки (название и автор) над редактором.
    // По умолчанию показана
    bool get_recordHeaderVisible(void);
    void set_recordHeaderVisible(bool state);

    // Глобальный режим только чтения: запрещены любые изменения базы,
    // разрешены навигация и просмотр. По умолчанию выключен
    bool get_readOnly(void);
    void set_readOnly(bool state);

    // Сырой глобальный флаг без учёта сессии. Им смотрят только
    // MCP-записи: они продолжают писать при заблокированном UI
    bool isGlobalReadOnly(void);

    // Блокировка UI на время MCP-сессии: только в памяти, никогда
    // не сохраняется. get_readOnly() учитывает её для интерфейса
    bool isMcpUiLocked(void);
    void setMcpUiLock(bool locked);

    // Лимиты клиппера: сколько картинок забирать в одну заметку
    // и максимальный размер одной картинки в мегабайтах
    int get_clipperMaxImages(void);
    bool set_clipperMaxImages(int count);

    int get_clipperMaxImageSizeMb(void);
    bool set_clipperMaxImageSizeMb(int mbSize);

    // Веб-клиппер: включение, глобальный хоткей, запомненная ветка
    bool get_clipperenable(void);
    void set_clipperenable(bool flag);

    QString get_clipperhotkey(void);
    void set_clipperhotkey(QString sequence);

    QString get_clipperbranchid(void);
    void set_clipperbranchid(QString id);

    // Разрешена ли периодическая проверка файла базы на предмет изменения сторонней программой
    bool getEnablePeriodicCheckBase(void);
    void setEnablePeriodicCheckBase(bool state);

    // Период проверки файла базы на предмет изменения сторонней программой
    int getCheckBasePeriod();
    void setCheckBasePeriod(int period);

    // Необходимо ли выводить сообщение если база была изменена другой программой
    bool getEnablePeriodicCheckMessage(void);
    void setEnablePeriodicCheckMessage(bool state);

    // Размер иконок в окне выбора (иконки, прикрепляемые к веткам)
    int  getPreviewIconSize(void);
    void setPreviewIconSize(int n);

    // Последняя выбранная секция в окошке выбора иконок
    QString getIconCurrentSectionName(void);
    void setIconCurrentSectionName(QString name);

    // Разрешена ли периодическая синхронизация
    bool getEnablePeriodicSyncro(void);
    void setEnablePeriodicSyncro(bool state);

    // Период автоматической периодической синхронизации
    int  getPeriodicSyncroPeriod(void);
    void setPeriodicSyncroPeriod(int period);

    // Разрешено ли создавать запись без текста
    bool getEnableCreateEmptyRecord(void);
    void setEnableCreateEmptyRecord(bool state);

    // Разрешен ли запуск MCP-сервера внешним AI-клиентом
    bool get_mcpAllow(void);
    void set_mcpAllow(bool state);

    // Показывать ли в дереве общий итог записей с учетом подветок.
    // По умолчанию выключено: только свои [n]. Страница Branch tree
    bool get_showSubtreeCount(void);
    void set_showSubtreeCount(bool state);

    // Раскладка импорта Telegram: "month" (заметка на месяц,
    // по умолчанию), "day" (заметка на день, ветки чат-год-месяц)
    // или "message" (заметка на сообщение, ветки чат-год-месяц-день).
    // Страница Telegram
    QString get_telegramImportMode(void);
    void set_telegramImportMode(const QString &mode);

    // Формат голосовых заметок по умолчанию: opus или mp3.
    // Выбирается радио на странице Voice notes, в диалоге
    // записи переключается. Неподдержанный бэкендом сбрасывается
    QString get_voiceNoteFormat(void);
    void set_voiceNoteFormat(QString format);

    // Качество голосовых заметок по умолчанию: 0 low, 1 medium, 2 high
    int get_voiceNoteQuality(void);
    void set_voiceNoteQuality(int quality);

    // Список открепляемых окон с координатами окон
    QString getDockableWindowsState(void);
    void setDockableWindowsState(QString state);

    QString getDockableWindowsBehavior(void);
    void setDockableWindowsBehavior(QString mode);

    // Тема оформления интерфейса
    QString getInterfaceTheme();
    bool setInterfaceTheme(QString themeName);

    QString getInterfaceIconSize();
    bool setInterfaceIconSize(QString sizeName);

private:

    void createFirstAtStartConfigCopy();

    QString get_parameter(QString name);

    QStringList removeParameterFromTable(QString removeName, QStringList table);
    QString getParameterTypeFromTable(QString parameterName, QStringList table);
    QString getParameterDefaultValueFromTable(QString parameterName, QStringList table);
    QStringList replaceParameterInTable(QString replaceName,
                                        QString replaceType,
                                        QString replaceDefaultValue,
                                        QStringList table);


    void update_version_process(void);

    QStringList get_parameter_table_1 (bool withEndSignature=true);
    QStringList get_parameter_table_2 (bool withEndSignature=true);
    QStringList get_parameter_table_3 (bool withEndSignature=true);
    QStringList get_parameter_table_4 (bool withEndSignature=true);
    QStringList get_parameter_table_5 (bool withEndSignature=true);
    QStringList get_parameter_table_6 (bool withEndSignature=true);
    QStringList get_parameter_table_7 (bool withEndSignature=true);
    QStringList get_parameter_table_8 (bool withEndSignature=true);
    QStringList get_parameter_table_9 (bool withEndSignature=true);
    QStringList get_parameter_table_10(bool withEndSignature=true);
    QStringList get_parameter_table_11(bool withEndSignature=true);
    QStringList get_parameter_table_12(bool withEndSignature=true);
    QStringList get_parameter_table_13(bool withEndSignature=true);
    QStringList get_parameter_table_14(bool withEndSignature=true);
    QStringList get_parameter_table_15(bool withEndSignature=true);
    QStringList get_parameter_table_16(bool withEndSignature=true);
    QStringList get_parameter_table_17(bool withEndSignature=true);
    QStringList get_parameter_table_18(bool withEndSignature=true);
    QStringList get_parameter_table_19(bool withEndSignature=true);
    QStringList get_parameter_table_20(bool withEndSignature=true);
    QStringList get_parameter_table_21(bool withEndSignature=true);
    QStringList get_parameter_table_22(bool withEndSignature=true);
    QStringList get_parameter_table_23(bool withEndSignature=true);
    QStringList get_parameter_table_24(bool withEndSignature=true);
    QStringList get_parameter_table_25(bool withEndSignature=true);
    QStringList get_parameter_table_26(bool withEndSignature=true);
    QStringList get_parameter_table_27(bool withEndSignature=true);
    QStringList get_parameter_table_28(bool withEndSignature=true);
    QStringList get_parameter_table_29(bool withEndSignature=true);
    QStringList get_parameter_table_30(bool withEndSignature=true);
    QStringList get_parameter_table_31(bool withEndSignature=true);
    QStringList get_parameter_table_32(bool withEndSignature=true);
    QStringList get_parameter_table_33(bool withEndSignature=true);
    QStringList get_parameter_table_34(bool withEndSignature=true);
    QStringList get_parameter_table_35(bool withEndSignature=true);
    QStringList get_parameter_table_36(bool withEndSignature=true);
    QStringList get_parameter_table_37(bool withEndSignature=true);
    QStringList get_parameter_table_38(bool withEndSignature=true);
    QStringList get_parameter_table_39(bool withEndSignature=true);
    QStringList get_parameter_table_40(bool withEndSignature=true);
    QStringList get_parameter_table_41(bool withEndSignature=true);
    QStringList get_parameter_table_42(bool withEndSignature=true);
    QStringList get_parameter_table_43(bool withEndSignature=true);
    QStringList get_parameter_table_44(bool withEndSignature=true);
    QStringList get_parameter_table_45(bool withEndSignature=true);
    QStringList get_parameter_table_46(bool withEndSignature=true);
    QStringList get_parameter_table_47(bool withEndSignature=true);
    QStringList get_parameter_table_48(bool withEndSignature=true);
    QStringList get_parameter_table_49(bool withEndSignature=true);
    QStringList get_parameter_table_50(bool withEndSignature=true);
    QStringList get_parameter_table_51(bool withEndSignature=true);

    QSettings *m_conf = nullptr; // было static
    bool m_isInit;

    // Блокировка UI на время MCP-сессии. Только рантайм: в QSettings
    // не пишется, после перезапуска всегда false
    bool m_mcpUiLock=false;
};

#endif // __APPCONFIG_H__
