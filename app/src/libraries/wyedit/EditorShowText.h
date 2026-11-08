#ifndef EDITORSHOWTEXT_H
#define EDITORSHOWTEXT_H

#include <QDialog>


//! \brief
//! Открепляемое окно записи: тот же движок что в главном окне
//! (полноценный Editor без панелей кнопок), только текст и поиск.
//! Ссылки, голос, оглавление, картинки, контекстное меню - все
//! как в обычной заметке. Правки сохраняются в файл записи,
//! главное окно и индексы подтягиваются после сейва


class Editor;
class QToolButton;
class QAction;
class QVBoxLayout;

class EditorShowText : public QDialog
{
    Q_OBJECT
public:
    explicit EditorShowText(QWidget *parent = nullptr, Qt::WindowFlags f=Qt::WindowFlags());
    virtual ~EditorShowText();

    // Открыть запись: создается внутренний Editor (тот же движок
    // что в главном окне), текст грузится из файла записи.
    // Лениво: до первого вызова редактор не создается и окно
    // пустое (тесты скрепки и бумаги работают без базы)
    void openNote(const QString &noteId);

    // Внутренний редактор. Null пока не вызван openNote/ensureEditor
    Editor *editor(void) const;

    // Создать редактор без привязки к записи (шов для тестов:
    // бумага, скрытые тулбары). Путь к editorconf.ini только
    // для теста, в бою всегда штатный
    void ensureEditor(const QString &configFileName=QString());

    QString noteId(void) const;

    // Состояние скрепки "поверх всех окон". Хранится отдельно от флага
    // окна: флаги сбрасываются пересозданием окна (смена поведения
    // single/together, восстановление из трея), а кнопка должна
    // отражать желание
    bool isPinned(void) const;

    // Есть ли несохраненные правки во внутреннем редакторе
    bool isModified(void) const;

    // Можно ли править (не заблокирована и не глобальный read-only).
    // Пересчитывается в openNote и refreshEditability
    bool isEditable(void) const;

    // Переприменить read-only по блокировке записи и глобальному флагу.
    // Вызывается диспетчером при переключении режима чтения
    void refreshEditability(void);

    // Каталог записи переименован на диске (конвертер имен,
    // переименование вслед за заголовком): перевести внутренний
    // редактор на новый путь из живой таблицы. Без перезапуска:
    // show-in-folder, сейвы и аттачи снова находят файлы
    void refreshWorkDirectory(void);

    int getTextVerticalScroll(void) const;
    void setTextVerticalScroll(int vScroll);

    int cursorPosition(void) const;
    void setCursorPosition(int pos);

    // Действия при закрытии диалога: сейв правок и сигнал диспетчеру
    void closeEvent(QCloseEvent *event);

    // Бумага и шрифт как в основном редакторе: на теме Custom -
    // явные цвета/мозаика из настроек, на базовых темах поле
    // полностью отдается теме (пустой stylesheet)
    void applyNoteColors(void);

signals:

    void editorShowTextClose(const QString &noteId);

public slots:

    // Сохранить правки (Ctrl+S без тулбара). Тихий выход без правок
    void saveDetached(void);

protected slots:

    // Скрепка "поверх всех окон": переключаемый флаг окна
    void onPinToggled(bool checked);

    void onFindRequested(void);

private:

    QString mNoteId;

    // Тот же WyEdit что в главном окне, но без тулбаров:
    // только поле текста и полоска поиска
    Editor *mEditor=nullptr;

    // Заблокирована ли запись (поле block): половина read-only,
    // вторая половина - глобальный флаг (читается живьем)
    bool mBlocked=false;
    bool mEditable=false;

    // Мини-кнопка скрепки в углу окна
    QToolButton *pinButton=nullptr;

    // Хоткеи без тулбара: Ctrl+S сохранить, Ctrl+F поиск
    QAction *saveAction=nullptr;
    QAction *findAction=nullptr;

    bool mPinned=false;

    int mGeomX=0;
    int mGeomY=0;
    int mGeomW=0;
    int mGeomH=0;

    // Слой диалога: сверху полоса скрепки, под ней редактор
    QVBoxLayout *mLayout=nullptr;

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    void hideEvent(QHideEvent *event);
    void showEvent(QShowEvent *event);
};

#endif // EDITORSHOWTEXT_H
