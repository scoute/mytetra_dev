#ifndef _CLIPPER_H_
#define _CLIPPER_H_

#include <QImage>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QString>

class QSocketNotifier;
class QTextDocument;
class KnowTreeModel;
class TreeItem;
#if defined(Q_OS_WIN)
class ClipperWinFilter;
#endif

// Веб-клиппер: вставка содержимого буфера обмена в ветку unsorted_notes
// по глобальному хоткею (выделил -> Ctrl+C -> хоткей).
//
// Движок тот же, что при ручной вставке: HTML нормализуется через
// QTextDocument, картинки скачиваются и сохраняются как image<id>.png,
// запись создаётся штатными средствами модели. Защита от повторной
// вставки — по sha256 нормализованного HTML.
//
// Глобальный хоткей работает под X11 (XGrabKey через dlopen libX11,
// без новых зависимостей сборки) и под Windows (RegisterHotKey).
// Под Wayland/macOS хоткей недоступен — остаётся кнопка «Вставить сейчас»
// в настройках и вызов clipNow() из кода.

class Clipper : public QObject
{
    Q_OBJECT

public:
    explicit Clipper(QObject *parent=nullptr);
    virtual ~Clipper();

    // Запуск/останов перехвата (читает настройки). Безопасно вызывать повторно
    void start(void);
    void stop(void);
    void rereadSettings(void);

    // Доступен ли глобальный хоткей на этой платформе/сессии
    bool isHotkeyAvailable(void) const;

    // Человекочитаемый статус для настроек ("X11 hotkey active: ...",
    // либо причина недоступности)
    QString backendStatus(void) const;

    // Тестовый хук: подмена модели дерева вместо GlobalParameters::getTreeScreen().
    // В боевом коде не используется (тесты не линкуют TreeScreen.cpp)
    void setTestModel(KnowTreeModel *model);

    // Заголовок из плоского текста (первая непустая строка, до 80 символов).
    // Чистая функция — покрыта тестами
    static QString makeTitle(const QString &plainText);

    // sha256 нормализованного HTML для дедупликации. Внутренние имена
    // картинок каноникализируются, иначе повторный клип той же страницы
    // с картинками никогда не совпадёт (имена случайны). Чистая функция
    static QByteArray contentHash(const QString &normalizedHtml);

    // Id записи из хеша содержимого: тот же формат, что getUniqueId()
    // (10 цифр + 10 символов 0-9a-z). Одинаковый контент даёт одинаковый
    // id — дедуплика превращается в поиск по id вместо чтения файлов.
    // Чистая функция — покрыта тестами
    static QString recordIdForHash(const QByteArray &hash);

    // Похожа ли строка на http(s) ссылку. Чистая функция
    static bool looksLikeUrl(const QString &value);

    // Первая http(s) ссылка в тексте. Чистая функция
    static QString extractUrl(const QString &text);

    // Ссылка на источник для поля url: только явный --url или одинокий
    // URL в буфере. Первая ссылка из текста не берётся: в скопированной
    // странице это обычно чужой URL. Чистая функция — покрыта тестами
    static QString resolveUrl(const QString &urlHint, const QString &plainText);

    // Картинка из data: URL. Пустая если разобрать не удалось. Чистая функция
    static QImage imageFromDataUrl(const QString &url);

    // Лимиты картинок из настроек. Сеттеры для тестов
    static void reloadLimits(void);
    static void setMaxImages(int count);
    static void setMaxImageSizeBytes(qint64 bytes);

public slots:
    // Главная точка входа: забрать буфер обмена в unsorted_notes
    void clipNow(void);

    // То же с явной ссылкой на источник (--control=--clipboard --url).
    // Ссылка пишется в поле url записи, иначе ищется в тексте
    void clipNowWithUrl(const QString &urlHint);

#if defined(Q_OS_WIN)
    // Нажатие глобального хоткея Windows: скопировать выделение и забрать
    void onWinHotkey(void);
#endif

signals:
    void clipFinished(bool ok, const QString &message);

private slots:
    void onX11Activity(void);

private:
    // Собранный для вставки материал
    struct ClipData
    {
        bool valid=false;
        QString html;                    // Нормализованный HTML для записи
        QMap<QString, QImage> images;    // Внутреннее имя -> картинка
        QString title;
        QString url;                     // Ссылка на источник (может быть пустой)
        int skippedImages=0;             // Картинок пропущено по лимитам/ошибкам
        QString errorMessage;
    };

    bool collectFromClipboard(ClipData &clipData);

    // Замена внешних картинок документа на внутренние с докачкой.
    // alreadyHave — картинок уже в наборе (прямая из буфера), лимит общий.
    // Чистая от буфера обмена часть — покрыта тестами без него.
    // Заполняет clipData.html/images/skippedImages
    bool processDocument(QTextDocument &document, ClipData &clipData, int alreadyHave=0);
    QByteArray fetchUrl(const QString &url, bool *ok);
    bool ensureBranch(TreeItem* &branchItem, QString *errorMessage=nullptr);
    bool isDuplicate(TreeItem *branchItem, const QString &recordId, const QByteArray &hash);
    bool storeRecord(TreeItem *branchItem, const QString &recordId, const QString &title,
                     const QString &url, const QString &html,
                     const QMap<QString, QImage> &images,
                     QString *errorMessage);
    void notify(const QString &title, const QString &text);

    // --- Глобальный хоткей (X11, dlopen) ---
    bool grabHotkey(const QString &sequence);
    void ungrabHotkey(void);
    static unsigned int qtModsToX11(int qtModifiers);
    static unsigned long qtKeyToKeysym(int qtKey);

    void *x11Lib=nullptr;         // handle dlopen("libX11.so.1")
    void *xDisplay=nullptr;       // Display* (opaque, без Xlib-заголовков)
    unsigned long xRootWindow=0;
    int xKeycode=0;
    unsigned int xModifiers=0;
    QSocketNotifier *xNotifier=nullptr;

#if defined(Q_OS_WIN)
    ClipperWinFilter *winFilter=nullptr; // Приёмник WM_HOTKEY
#endif

    bool hotkeyActive=false;
    QString hotkeyActiveSequence;

    KnowTreeModel *testModel=nullptr; // Подмена модели для тестов, см. setTestModel()

    KnowTreeModel *treeModel(void) const;
    void saveTree(void) const;

    // Ссылка на источник для текущего клипа (из --url). Пусто для хоткея
    QString m_urlHint;
};

#endif // _CLIPPER_H_
