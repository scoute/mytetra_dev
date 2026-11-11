#ifndef __APPCONFIGUI_H__
#define __APPCONFIGUI_H__

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QList>
#include <QByteArray>

class AppConfig;

// P2: секция конфига. Владелец - AppConfig, бэкенд QSettings
// общий (перепривязывается при каждом обращении фасада,
// поэтому работает и в пробах с прямой установкой m_conf)

class AppConfigUi : public QObject
{
    Q_OBJECT

public:

    explicit AppConfigUi(QObject *parent=nullptr);
    void setBackend(QSettings *conf, AppConfig *host);

    // Нужно ли показывать подтверждение при выполнении действия "cut на ветке
    bool get_cutbranchconfirm(void);
    bool set_cutbranchconfirm(bool confirm);
    // Геометрия основного окна
    QByteArray get_mainwingeometry(void);
    void set_mainwingeometry(QByteArray dataGeometry);
    QList<int> get_vspl_size_list(void);
    void set_vspl_size_list(QList<int> list);
    QList<int> get_hspl_size_list(void);
    void set_hspl_size_list(QList<int> list);
    QList<int> get_findsplitter_size_list(void);
    void set_findsplitter_size_list(QList<int> list);
    // Размеры левой колонки (дерево над списком записей), когда список
    // записей перенесён под дерево. Остальные сплиттеры - выше
    QList<int> get_leftspl_size_list(void);
    void set_leftspl_size_list(QList<int> list);
    QList<int> get_splitter_size_list(QString name);
    void set_splitter_size_list(QString name, QList<int> list);
    QStringList get_tree_position(void);
    void set_tree_position(QStringList list);
    bool get_runinminimizedwindow(void);
    void set_runinminimizedwindow(bool flag);
    int getEditorCursorPosition(void);
    void setEditorCursorPosition(int n);
    int getEditorScrollBarPosition(void);
    void setEditorScrollBarPosition(int n);
    int getUglyQssReplaceHeightForTableView(void);
    void setUglyQssReplaceHeightForTableView(int n);
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
    // Строка фильтра над деревом веток (тумблер в меню Вид)
    bool get_treeFilterVisible(void);
    void set_treeFilterVisible(bool state);
    // Положение номеров страниц в подвале предпросмотра и печати:
    // 0 слева, 1 по центру, 2 справа. Намеренно вне версионных таблиц:
    // отсутствующий ключ покрывается дефолтом (справа)
    int get_printFooterAlign(void);
    void set_printFooterAlign(int mode);
    // Печатать ли номера страниц вообще (превью и бумага).
    // Тоже unversioned, дефолт - показывать
    bool get_printFooterEnabled(void);
    void set_printFooterEnabled(bool state);
    // Вид панели прикрепленных файлов: "list" или "tiles"
    QString get_attachViewMode(void);
    void set_attachViewMode(QString mode);
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
    // Разрешена ли подсветка записей с прикрепленными файлами
    bool getEnableRecordWithAttachHighlight(void);
    void setEnableRecordWithAttachHighlight(bool state);
    // Цвет подсветки записей с прикрепленными файлами
    QString getRecordWithAttachHighlightColor(void);
    void setRecordWithAttachHighlightColor(QString color);
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
    // Шапка заметки (название и автор) над редактором.
    // По умолчанию показана
    bool get_recordHeaderVisible(void);
    void set_recordHeaderVisible(bool state);
    // Положение списка записей: false - справа от дерева (по умолчанию),
    // true - под деревом в левой колонке. Страница Notes area
    bool get_recordTableUnderTree(void);
    void set_recordTableUnderTree(bool state);
    // Порядок кнопок тулбаров в виде списков через запятую.
    // Пусто - не настраивалось, экраны строят порядок по умолчанию.
    // (Пустой настроенный список невозможен: в задействованных
    // обязана остаться хотя бы одна команда, как в редакторе)
    QStringList getTreeToolbarOrder(void);
    void setTreeToolbarOrder(QStringList order);
    QStringList getRecordToolbarOrder(void);
    void setRecordToolbarOrder(QStringList order);
    QStringList getRecordExtraToolbarOrder(void);
    void setRecordExtraToolbarOrder(QStringList order);
    // Размер иконок в окне выбора (иконки, прикрепляемые к веткам)
    int getPreviewIconSize(void);
    void setPreviewIconSize(int n);

    // Режим полной статистики: 0 - спрашивать каждый раз,
    // 1 - всегда считать, 2 - никогда (только быстрая)
    int get_statsMode(void);
    void set_statsMode(int mode);

    // Лимит записей для автосчета полной статистики без вопросов
    int get_statsLimit(void);
    void set_statsLimit(int limit);
    // Последняя выбранная секция в окошке выбора иконок
    QString getIconCurrentSectionName(void);
    void setIconCurrentSectionName(QString name);
    // Показывать ли в дереве общий итог записей с учетом подветок.
    // По умолчанию выключено: только свои [n]. Страница Branch tree
    bool get_showSubtreeCount(void);
    void set_showSubtreeCount(bool state);
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

    QSettings *m_conf=nullptr;
    AppConfig *m_host=nullptr;
};

#endif // __APPCONFIGUI_H__
