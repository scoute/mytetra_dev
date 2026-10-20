#ifndef EDITORFONTFAMILYCOMBOBOX_H
#define EDITORFONTFAMILYCOMBOBOX_H

#include <QObject>
#include <QFontComboBox>

#include "EditorToolPseudoButton.h"


class EditorFontFamilyComboBox : public QFontComboBox
{
    Q_OBJECT

public:
    EditorFontFamilyComboBox(QWidget *parent=nullptr);
    virtual ~EditorFontFamilyComboBox();

    void setIsProgrammChanged(bool flag);

    // Перестройка группы недавних шрифтов сверху списка (после ""-маркера):
    // новые сверху, разделитель, пропуски отсутствующих в системе.
    // Текущий шрифт currentFamily выставляется обратно без сигналов
    void reloadRecentFonts(const QStringList &recent, const QString &currentFamily);

    EditorToolPseudoButton toolPseudoButton;

protected slots:
    void onCurrentIndexChanged(int index);

protected:

    void setupSignals();

    bool isProgrammChanged;

    int previousIndex;

    // Сколько строк занимает MRU-группа (шрифты без разделителя):
    // сносится перед перестройкой
    int recentRows;
};

#endif // EDITORFONTFAMILYCOMBOBOX_H
