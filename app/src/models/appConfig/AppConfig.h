#ifndef __APPCONFIG_H__
#define __APPCONFIG_H__

#include <QObject>
#include <QDir>
#include <QMap>
#include <QSettings>
#include <QDebug>

class QRect;
class QStringList;

class AppConfigStorage;
class AppConfigUi;
class AppConfigFind;
class AppConfigSync;
class AppConfigCrypto;
class AppConfigRecordTable;
class AppConfigClipper;
class AppConfigServices;
class AppConfigGeneral;

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

    // Секции конфига. Ленивые доступы, бэкенд перепривязывается
    // при каждом обращении (работают и без init(), как фасады раньше).
    // Прямой доступ вместо 243 фасадов-однострочников
    AppConfigStorage *storageSection(void);
    AppConfigUi *uiSection(void);
    AppConfigFind *findSection(void);
    AppConfigSync *syncSection(void);
    AppConfigCrypto *cryptoSection(void);
    AppConfigRecordTable *recordTableSection(void);
    AppConfigClipper *clipperSection(void);
    AppConfigServices *servicesSection(void);
    AppConfigGeneral *generalSection(void);

    // Версия формата конфигфайла

    // Директория с базой знаний

    // Директория с корзиной
    
    // Максимальный размер директории корзины в мегабайтах
    
    // Максимально допустимое число файлов в корзине

    // Нужно ли показывать подтверждение при выполнении действия "cut на ветке

    // Нужно ли печатать отладочные сообщения в консоль

    // Язык интерфейса

    // Номер последнего префикса в виде строки с ведущими нулями

    // Геометрия основного окна
    




    // Размеры левой колонки (дерево над списком записей), когда список
    // записей перенесён под дерево. Остальные сплиттеры - выше


    


    // ID записи в таблице конечных записей, которая выделена на экране


    // Настройки виджета поиска по базе

    






    // Умное автосохранение: только при реальных правках,
    // без изменений ни файла ни коммита. По умолчанию вкл, 10 мин


    // Сквош истории изменений: автосжатие снизу по лимиту.
    // По умолчанию включено, хранить 100 (genesis плюс свежие)


    // Стратегия сжатия (GitHelper::SquashMode).
    // По умолчанию удалять старые

    // Мастер-тумблер ведения истории заметок. Выключенная история
    // не снимается и диалог не открывается. По умолчанию выключена:
    // нужен установленный git, проверяется при включении





    









    // Перечень полей, отображаемых в таблице конечных записей

    // Нужно ли отображать горизонтальные заголовки в таблице конечных записей

    // Нужно ли отображать вертикальные заголовки в таблице конечных записей

    // Ширина полей, отображаемых в таблице конечных записей

    // Показывать ли сплешскрин при старте программы

    // Режим интерфейса, возможные значения "desktop" и "mobile"



    // Флаг, показывающий распахнуты ли все инструметы управления в окне поиска по базе

    // Режим поиска в заметке: встраиваемая полоска (true) или отдельное окно
    // (false). По умолчанию полоска, переключается кнопками в окне,
    // на полоске и радиокнопками на странице настроек Search

    // Панель избранного видна (по умолчанию да). Намеренно вне
    // версионных таблиц, как editorFindEmbedded

    // Строка фильтра над деревом веток (тумблер в меню Вид)

    // Вид панели прикрепленных файлов: "list" или "tiles"

    // Хранилище избранного: "sidecar" (data/favorites.xml, по умолчанию)
    // или "xml" (поля favorite в mytetra.xml). Тоже без версий

    // Разрешено ли использовать собственный формат вывода даты и времени

    // Строка собственного формата вывода даты и времени

    // Путь на диске по которому пользователь открывал файлы чтобы приаттачить их к записи

    // Путь на диске по которому пользователь сохранял (Сохранить как...) приаттаченные файлы

    // Разрешать ли для просмотра расшифровывать зашифрованные файлы в директорию корзины MyTetra

    // Размер файла лога дейсвтвий

    // Разрешено ли логирование

    // Разрешена ли подсветка записей с прикрепленными файлами

    // Цвет подсветки записей с прикрепленными файлами
    // Цвет закраски секрета в формате "#77767b"

    // Цвет фона поля заметки. Пустая строка означает цвет темы
    // по умолчанию, иначе формат цвета - "#B0BAC7"

    // Цвет шрифта поля заметки. Пустая строка означает цвет темы
    // по умолчанию, иначе формат цвета - "#000000"

    // Кастомный дизайн поверх темы. Пусто/-1 означает цвет/размер темы,
    // иначе значение пользователя. Применяется CssHelper::applyCustomDesign

    // Мозаичный фон заметок: тайл повторяется по обеим осям.
    // Пусто означает без картинки. Повторяется по обеим осям

    // Индекс встроенного тайла (0 - tx_fabric.jpg). Свой файл выше
    // имеет приоритет над индексом

    // Режим фона заметок: "color" или "texture". Текстура без своего
    // файла означает встроенный tx_fabric.jpg

    // Базовая тема для Custom: одна из четырех (default, dark,
    // modern, midnight). Сами базовые темы не меняются никогда

    // Человеко-читаемые имена вложенных файлов на диске (схема id_имя.ext
    // вместо id.ext). По умолчанию выключено. Включается кнопкой миграции
    // в настройках прикрепленных файлов, после чего новые вложения
    // сразу сохраняются по человеко-читаемой схеме.
    // Устаревший флаг: теперь схема задается режимом attachNameMode,
    // методы оставлены для совместимости (true = гибридный режим)

    // Режим имен вложенных файлов на диске:
    // 0 - системные (id.ext), 1 - гибридные (id_имя.ext),
    // 2 - оригинальные (имя как при загрузке). По умолчанию 0.
    // Если ключ режима отсутствует, а устаревший флаг выставлен,
    // считается выбранным гибридный режим
    enum AttachNameMode { AttachNameSystem=0, AttachNameHybrid=1, AttachNameOriginal=2 };

    // Шапка заметки (название и автор) над редактором.
    // По умолчанию показана

    // Положение списка записей: false - справа от дерева (по умолчанию),
    // true - под деревом в левой колонке. Страница Notes area

    // Порядок кнопок тулбаров в виде списков через запятую.
    // Пусто - не настраивалось, экраны строят порядок по умолчанию.
    // (Пустой настроенный список невозможен: в задействованных
    // обязана остаться хотя бы одна команда, как в редакторе)



    // Глобальный режим только чтения: запрещены любые изменения базы,
    // разрешены навигация и просмотр. По умолчанию выключен

    // Сырой глобальный флаг без учёта сессии. Им смотрят только
    // MCP-записи: они продолжают писать при заблокированном UI

    // Блокировка UI на время MCP-сессии: только в памяти, никогда
    // не сохраняется. get_readOnly() учитывает её для интерфейса
    bool isMcpUiLocked(void);
    void setMcpUiLock(bool locked);

    // Лимиты клиппера: сколько картинок забирать в одну заметку
    // и максимальный размер одной картинки в мегабайтах


    // Веб-клиппер: включение, глобальный хоткей, запомненная ветка



    // Режим целевой ветки клипа: 0 — по умолчанию (unsorted_notes),
    // 1 — выбранная ветка (clipperbranchid), 2 — спрашивать каждый раз

    // Частые ветки клипов: пары id:count через ";", частые впереди,
    // лимит 16. Нижние плитки опроса показывают их вместо полного списка

    // Какие 16 плиток внизу опроса: 0 — последние, 1 — частые

    // При попытке вставить дубль открывать оригинал заметки

    // Разрешена ли периодическая проверка файла базы на предмет изменения сторонней программой

    // Период проверки файла базы на предмет изменения сторонней программой

    // Необходимо ли выводить сообщение если база была изменена другой программой

    // Размер иконок в окне выбора (иконки, прикрепляемые к веткам)

    // Последняя выбранная секция в окошке выбора иконок

    // Разрешена ли периодическая синхронизация

    // Период автоматической периодической синхронизации

    // Разрешено ли создавать запись без текста

    // Разрешен ли запуск MCP-сервера внешним AI-клиентом

    // Игнорировать ошибки проверки TLS при загрузке картинок/файлов.
    // По умолчанию выключено: невалидный/самоподписанный сертификат
    // отклоняется. Включение снижает защиту от MITM

    // Показывать ли в дереве общий итог записей с учетом подветок.
    // По умолчанию выключено: только свои [n]. Страница Branch tree

    // Раскладка импорта Telegram: "month" (заметка на месяц,
    // по умолчанию), "year" (заметка на год, прямо в чате),
    // "day" (заметка на день, ветки чат-год-месяц)
    // или "message" (заметка на сообщение, ветки чат-год-месяц-день)

    // Метод шифрования: rc5 (совместимый) или xchacha (современный)

    // Формат голосовых заметок по умолчанию: opus или mp3.
    // Выбирается радио на странице Voice notes, в диалоге
    // записи переключается. Неподдержанный бэкендом сбрасывается

    // Подавление фонового шума в диалоге записи голосовых заметок.
    // По умолчанию включено: цепочка HP+гейт наTurn очищает гул

    // Качество голосовых заметок по умолчанию: 0 low, 1 medium, 2 high

    // Список открепляемых окон с координатами окон


    // Тема оформления интерфейса


private:

    void createFirstAtStartConfigCopy(void);

public:

    // P2: чтение сырого ключа с фолбэком на дефолт актуальной таблицы.
    // Публичный чтобы секции конфига ходили через хост
    QString get_parameter(QString name);

    // Счетчик срабатываний фолбэка get_parameter (утраченные ключи).
    // Растет только на известных таблице ключах; неизвестные имена
    // по-прежнему роняют программу через criticalError
    int get_configFallbackCount(void) const;
    QMap<QString, int> get_configFallbackHits(void) const;

private:

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
    QStringList get_parameter_table_52(bool withEndSignature=true);
    QStringList get_parameter_table_53(bool withEndSignature=true);
    QStringList get_parameter_table_54(bool withEndSignature=true);
    QStringList get_parameter_table_55(bool withEndSignature=true);
    QStringList get_parameter_table_56(bool withEndSignature=true);
    QStringList get_parameter_table_57(bool withEndSignature=true);
    QStringList get_parameter_table_58(bool withEndSignature=true);
    QStringList get_parameter_table_59(bool withEndSignature=true);
    QStringList get_parameter_table_60(bool withEndSignature=true);
    QStringList get_parameter_table_61(bool withEndSignature=true);

    QSettings *m_conf = nullptr; // было static
    bool m_isInit;

    AppConfigStorage *m_storage=nullptr;
    AppConfigUi *m_ui=nullptr;
    AppConfigFind *m_find=nullptr;
    AppConfigSync *m_sync=nullptr;
    AppConfigCrypto *m_crypto=nullptr;
    AppConfigRecordTable *m_recordtable=nullptr;
    AppConfigClipper *m_clipper=nullptr;
    AppConfigServices *m_services=nullptr;
    AppConfigGeneral *m_general=nullptr;

    // Блокировка UI на время MCP-сессии. Только рантайм: в QSettings
    // не пишется, после перезапуска всегда false
    bool m_mcpUiLock=false;

    // Счетчик срабатываний фолбэка get_parameter: всего и по именам.
    // Только рантайм, никуда не сохраняется
    int m_fallbackTotal=0;
    QMap<QString, int> m_fallbackHits;
};

#endif // __APPCONFIG_H__
