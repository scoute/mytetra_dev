#ifndef APPCONFIGPAGE_THEME_H
#define APPCONFIGPAGE_THEME_H

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QCheckBox;
class QLabel;
class QToolButton;
class QPushButton;
class QSpinBox;
class QRadioButton;
class QFont;
class MtComboBox;

// Страница кастомной темы: базовая тема + 14 настраиваемых пунктов
// дизайна (цвета, шрифты, размеры). Пусто/-1 означает значение темы
class AppConfigPage_Theme : public ConfigPage
{
    Q_OBJECT

public:
    AppConfigPage_Theme(QWidget *parent = nullptr);
    virtual ~AppConfigPage_Theme();

    int applyChanges(void);
    void retranslateUi(void);

    // Группы кастома активны только при выбранном Custom:
    // на базовых темах настройки не действуют
    void updateCustomEnabled(void);

private slots:
    void onClickedNotePaperColor(void);
    void onClickedNotePaperColorReset(void);
    void onClickedNoteFontColor(void);
    void onClickedNoteFontColorReset(void);
    void onClickedSecretColor(void);
    void onClickedSecretColorReset(void);
    void onClickedNotePaperImageLoad(void);
    void onClickedNotePaperImageClear(void);
    void refreshNotePaperImageLabel(void);
    void onClickedTreeBackground(void);
    void onClickedTreeBackgroundReset(void);
    void onClickedTreeTextReset(void);
    void onClickedTreeText(void);
    void onClickedTableBackground(void);
    void onClickedTableBackgroundReset(void);
    void onClickedTableText(void);
    void onClickedTableTextReset(void);
    void onClickedTableAlternate(void);
    void onClickedTableAlternateReset(void);
    void onClickedInterfaceFont(void);
    void onClickedInterfaceFontReset(void);
    void onClickedResetAll(void);
    void onSwitchToCustom(void);

protected:
    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    void setupIconSizeComboBox(void);

    void setColorForButton(QToolButton *button, QColor *color, const QColor &value);
    void refreshInterfaceFontButton(void);

    // Базовая тема и размер иконок (переехало из Appearance).
    // Переключатель тем живёт в меню Вид-Темы: здесь только редактор Custom
    QGroupBox *baseBox;
    QLabel *customHint;
    QPushButton *switchToCustomButton;
    QLabel *baseThemeLabel;
    MtComboBox *baseThemeComboBox;
    QLabel *iconSizeLabel;
    MtComboBox *iconSizeComboBox;

    // Заметка (переехало из Misc)
    QGroupBox *noteBox;
    QRadioButton *notePaperColorRadio;
    QToolButton *notePaperColorButton;
    QColor *notePaperColor;
    QPushButton *notePaperColorReset;
    bool notePaperColorIsTheme;
    QLabel *noteFontColorLabel;
    QToolButton *noteFontColorButton;
    QColor *noteFontColor;
    QPushButton *noteFontColorReset;
    bool noteFontColorIsTheme;
    QLabel *secretColorLabel;
    QToolButton *secretColorButton;
    QColor *secretColor;
    QPushButton *secretColorReset;
    QLabel *notePaperImageLabel;
    QRadioButton *notePaperTextureRadio;
    QPushButton *notePaperImageLoad;
    QPushButton *notePaperImageClear;
    QString notePaperImagePath;

    // Дерево веток
    QGroupBox *treeBox;
    QLabel *treeBackgroundLabel;
    QToolButton *treeBackgroundButton;
    QColor *treeBackground;
    QPushButton *treeBackgroundReset;
    bool treeBackgroundIsTheme;
    QLabel *treeTextLabel;
    QToolButton *treeTextButton;
    QColor *treeText;
    QPushButton *treeTextReset;
    bool treeTextIsTheme;
    QLabel *treeFontSizeLabel;
    QSpinBox *treeFontSize;
    QLabel *treeIndentationLabel;
    QSpinBox *treeIndentation;

    // Список заметок
    QGroupBox *tableBox;
    QLabel *tableBackgroundLabel;
    QToolButton *tableBackgroundButton;
    QColor *tableBackground;
    QPushButton *tableBackgroundReset;
    bool tableBackgroundIsTheme;
    QLabel *tableTextLabel;
    QToolButton *tableTextButton;
    QColor *tableText;
    QPushButton *tableTextReset;
    bool tableTextIsTheme;
    QLabel *tableAlternateLabel;
    QToolButton *tableAlternateButton;
    QColor *tableAlternate;
    QPushButton *tableAlternateReset;
    bool tableAlternateIsTheme;
    QLabel *tableFontSizeLabel;
    QSpinBox *tableFontSize;
    QLabel *tableRowHeightLabel;
    QSpinBox *tableRowHeight;

    // Шрифт интерфейса
    QGroupBox *fontBox;
    QLabel *interfaceFontLabel;
    QPushButton *interfaceFontButton;
    QString interfaceFontFamily;
    int interfaceFontSize;
    QPushButton *interfaceFontReset;

    QPushButton *resetAllButton;
};

#endif // APPCONFIGPAGE_THEME_H
