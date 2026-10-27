#ifndef EDITORCONFIG_H
#define EDITORCONFIG_H

#include <QObject>
#include <QWidget>
#include <QSettings>
#include <QImage>
#include <QColor>


class EditorConfig : public QWidget
{
    Q_OBJECT

public:
    EditorConfig(QString config_file_name, QWidget *parent=nullptr);
    ~EditorConfig();

    void sync(void);

    // Настройка стандартного шрифта
    QString get_default_font(void);
    void set_default_font(QString fontName);

    int get_default_font_size(void);
    void set_default_font_size(int size);

    // Настройка моноширинного шрифта
    QString get_monospace_font(void);
    void set_monospace_font(QString fontName);

    int  get_monospace_font_size(void);
    void set_monospace_font_size(int size);
    bool get_monospace_font_size_apply(void);
    void set_monospace_font_size_apply(bool i);

    
    // Настройка шрифта форматирования кода
    QString get_code_font(void);
    void    set_code_font(QString fontName);

    int  get_code_font_size(void);
    void set_code_font_size(int size);
    bool get_code_font_size_apply(void);
    void set_code_font_size_apply(bool i);
    QString get_code_font_color(void);
    void set_code_font_color(QString color);

    // Цвета внешних и внутренних ссылок (дефолты в таблице 44)
    // и ссылок на разделы оглавления (дефолт в таблице 45)
    QString get_linkColorExternal(void);
    void set_linkColorExternal(QString color);
    QString get_linkColorInternal(void);
    void set_linkColorInternal(QString color);
    QString get_linkColorTocGo(void);
    void set_linkColorTocGo(QString color);

    // Цвет ссылки по адресу: внешние, mytetra://note/, mytetra://toc-go/.
    // Единая точка для диалогы/оглавления/вставки: расхождений
    // синий/зеленый больше нет. Служебные href (copy-code, toc-метка,
    // голос) сюда не попадают, их красит вызывающий код сам
    QColor linkColorForHref(const QString &href);

    int  get_code_indent_size(void);
    void set_code_indent_size(int size);
    bool get_code_indent_size_apply(void);
    void set_code_indent_size_apply(bool i);

    bool get_structure_panel_visible(void);
    void set_structure_panel_visible(bool i);

    // Шаг изменения отступа
    int get_indent_step(void);
    void set_indent_step(int i);

    // Размер табуляции для клавиши Tab
    int get_tab_size(void);
    void set_tab_size(int i);

    // Счетчик для таймера обновления картинки формулы в редакторе формулы
    int getMathExpressionUpdateTime(void);
    void setMathExpressionUpdateTime(int i);

    QString get_finddialog_geometry(void);
    void set_finddialog_geometry(QString geometry);

    // Настройка первой линии с кнопками инструментов
    QString get_tools_line_1(void);
    void set_tools_line_1(QString geometry);

    // Настройка второй линии с кнопками инструментов
    QString get_tools_line_2(void);
    void set_tools_line_2(QString geometry);

    // Раскрыты или сомкнуты панели инструментов
    bool get_expand_tools_lines(void);
    void set_expand_tools_lines(bool i);
    bool get_hide_tools_lines(void);
    void set_hide_tools_lines(bool i);

    // Формат сохранения новых картинок: "png" или "jpg".
    // Действует только на вновь вставляемые картинки
    QString get_imageSaveFormat(void);
    void set_imageSaveFormat(const QString &format);

    // Расширение для новой картинки: формат из настройки, но картинки
    // с прозрачностью всегда PNG (у JPG нет альфа-канала)
    QString imageExtensionFor(const QImage &image);

    // Лимит длинной стороны новой картинки в пикселях, 0 — без ограничения
    int get_imageMaxSize(void);
    void set_imageMaxSize(int size);

    // Мастер-тумблер деструктивной операции: разрешить менять истинный
    // размер файла картинки из диалога свойств. Выключено по умолчанию
    bool get_allowTrueImageResize(void);
    void set_allowTrueImageResize(bool allow);

    // Последние выбранные пользователем цвета текста и фона:
    // клик по кнопке красит в них без пикера (как в LibreOffice)
    QString get_lastFontColor(void);
    void set_lastFontColor(const QString &color);
    QString get_lastBackgroundColor(void);
    void set_lastBackgroundColor(const QString &color);

    // Клик по кнопке цвета применяет последний цвет (иначе как раньше:
    // клик ничего не делает, цвет на кнопке — под курсором)
    bool get_applyLastColorOnClick(void);
    void set_applyLastColorOnClick(bool apply);

    // Недавние шрифты: топ-10 по частоте использования (выбор
    // из списка и шрифт под курсором считаются использованием).
    // Хранение одной строкой через ";" как списки инструментов.
    // Счетчики — в recentFontsUsage ("Family=count;..."), порядок —
    // по убыванию частоты, равные — свежие выше
    QStringList get_recentFonts(void);
    void add_recentFont(const QString &family);
    void noteFontUsed(const QString &family);
    void set_recentFonts(const QStringList &families);
    QString get_recentFontsUsage(void);
    void set_recentFontsUsage(const QString &usage);

    // Недавние спецсимволы, новые сверху, максимум 12.
    // Хранение одной строкой через "\n" (символы могут содержать ";" и ",")
    QStringList get_recentSymbols(void);
    void add_recentSymbol(const QString &symbol);
    void set_recentSymbols(const QStringList &symbols);

    // Ужать картинку до лимита длинной стороны из настройки.
    // Возвращает исходник если ужатие не нужно или лимит выключен
    QImage limitImageSize(const QImage &image);

private:

    QSettings *conf;

    QString get_parameter(QString name);

    int get_config_version(void);
    void set_config_version(int i);

    void update_version_process(void);

    static QStringList get_parameter_table_1(bool withEndSignature=true);
    static QStringList get_parameter_table_2(bool withEndSignature=true);
    static QStringList get_parameter_table_3(bool withEndSignature=true);
    static QStringList get_parameter_table_4(bool withEndSignature=true);
    static QStringList get_parameter_table_5(bool withEndSignature=true);
    static QStringList get_parameter_table_6(bool withEndSignature=true);
    static QStringList get_parameter_table_7(bool withEndSignature=true);
    static QStringList get_parameter_table_8(bool withEndSignature=true);
    static QStringList get_parameter_table_9(bool withEndSignature=true);
    static QStringList get_parameter_table_10(bool withEndSignature=true);
    static QStringList get_parameter_table_11(bool withEndSignature=true);
    static QStringList get_parameter_table_12(bool withEndSignature=true);
    static QStringList get_parameter_table_13(bool withEndSignature=true);
    static QStringList get_parameter_table_14(bool withEndSignature=true);
    static QStringList get_parameter_table_15(bool withEndSignature=true);
    static QStringList get_parameter_table_16(bool withEndSignature=true);
    static QStringList get_parameter_table_17(bool withEndSignature=true);
    static QStringList get_parameter_table_18(bool withEndSignature=true);
    static QStringList get_parameter_table_19(bool withEndSignature=true);
    static QStringList get_parameter_table_20(bool withEndSignature=true);
    static QStringList get_parameter_table_21(bool withEndSignature=true);
    static QStringList get_parameter_table_22(bool withEndSignature=true);
    static QStringList get_parameter_table_23(bool withEndSignature=true);
    static QStringList get_parameter_table_24(bool withEndSignature=true);
    static QStringList get_parameter_table_25(bool withEndSignature=true);
    static QStringList get_parameter_table_26(bool withEndSignature=true);
    static QStringList get_parameter_table_27(bool withEndSignature=true);
    static QStringList get_parameter_table_28(bool withEndSignature=true);
    static QStringList get_parameter_table_29(bool withEndSignature=true);
    static QStringList get_parameter_table_30(bool withEndSignature=true);
    static QStringList get_parameter_table_31(bool withEndSignature=true);
    static QStringList get_parameter_table_32(bool withEndSignature=true);
    static QStringList get_parameter_table_33(bool withEndSignature=true);
    static QStringList get_parameter_table_34(bool withEndSignature=true);
    static QStringList get_parameter_table_35(bool withEndSignature=true);
    static QStringList get_parameter_table_36(bool withEndSignature=true);
    static QStringList get_parameter_table_37(bool withEndSignature=true);
    static QStringList get_parameter_table_38(bool withEndSignature=true);
    static QStringList get_parameter_table_39(bool withEndSignature=true);
    static QStringList get_parameter_table_40(bool withEndSignature=true);
    static QStringList get_parameter_table_41(bool withEndSignature=true);
    static QStringList get_parameter_table_42(bool withEndSignature=true);
    static QStringList get_parameter_table_43(bool withEndSignature=true);
    static QStringList get_parameter_table_44(bool withEndSignature=true);
    static QStringList get_parameter_table_45(bool withEndSignature=true);

    static QStringList remove_option(QStringList table, QString optionName);

    QString update_version_allowcollision(int versionFrom,
                                          int versionTo,
                                          QString name,
                                          QString fromType,
                                          QString fromValue,
                                          QString toType,
                                          QString toValue);

    QString update_version_change_value(int versionFrom,
                                        int versionTo,
                                        QString name,
                                        QString fromValue,
                                        QString toValue);

    void update_version(int versionFrom,
                        int versionTo,
                        QStringList baseTable,
                        QStringList finalTable);

};

#endif // EDITORCONFIG_H
