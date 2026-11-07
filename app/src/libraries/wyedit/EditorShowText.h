#ifndef EDITORSHOWTEXT_H
#define EDITORSHOWTEXT_H

#include <QDialog>
#include <QTextEdit>


//! \brief
//! Класс открепляемого окна для просмотра текста записи без возможности редактирования


class EditorShowTextContextMenu;
class QToolButton;

class EditorShowText : public QDialog
{
    Q_OBJECT
public:
    explicit EditorShowText(QWidget *parent = nullptr, Qt::WindowFlags f=Qt::WindowFlags());
    virtual ~EditorShowText();

    void setNoteId(const QString &noteId);
    void setHtml(QString text);
    void setDocument(QSharedPointer<QTextDocument> pDocument);

    // Состояние скрепки "поверх всех окон". Хранится отдельно от флага
    // окна: флаги сбрасываются пересозданием окна (смена поведения
    // single/together, восстановление из трея), а кнопка должна
    // отражать желание
    bool isPinned(void) const;

    bool hasTextSelection();

    int getTextVerticalScroll();
    void setTextVerticalScroll(int vScroll);

    // Действия при закрытии диалога
    void closeEvent(QCloseEvent *event);

signals:

    void editorShowTextClose(const QString &noteId);

protected slots:

    void onCustomContextMenuRequested(const QPoint &pos);
    void onGotoNote();

    // Скрепка "поверх всех окон": переключаемый флаг окна
    void onPinToggled(bool checked);

private:

    QString mNoteId;
    QSharedPointer<QTextEdit> mTextArea;
    QSharedPointer<QTextDocument> mTextDocument;
    QSharedPointer<EditorShowTextContextMenu> mContextMenu;

    // Мини-кнопка скрепки в углу окна
    QToolButton *pinButton=nullptr;

    bool mPinned=false;

    int mGeomX=0;
    int mGeomY=0;
    int mGeomW=0;
    int mGeomH=0;

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    void hideEvent(QHideEvent *event);
    void showEvent(QShowEvent *event);
};

#endif // EDITORSHOWTEXT_H
