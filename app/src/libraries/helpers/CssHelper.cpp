#include <QtGlobal>
#include <QApplication>
#include <QScreen>
#include <QFile>
#include <QDebug>
#include <QStyle>
#include <QLabel>
#include <QTimer>
#include <QRegularExpression>
#include <QHeaderView>

#include "CssHelper.h"

#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/helpers/ObjectHelper.h"
#include "views/tree/KnowTreeView.h"
#include "views/recordTable/RecordTableView.h"
#include "libraries/OrderedMap.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppFiles.h"
#include "libraries/helpers/DiskHelper.h"
#include "libraries/helpers/PaletteHelper.h"

extern GlobalParameters globalParameters;
extern FixedParameters fixedParameters;
extern AppConfig mytetraConfig;
extern AppFiles mytetraFiles;


CssHelper::CssHelper()
{

}


qreal CssHelper::getCalculateIconSizePx(void)
{
#if QT_VERSION >= 0x040000 && QT_VERSION < 0x050000
    qreal dpiX=qApp->desktop()->physicalDpiX();
    qreal dpiY=qApp->desktop()->physicalDpiY();
    qreal dpi=(dpiX+dpiY)/2;
#endif

#if QT_VERSION >= 0x050000 && QT_VERSION < 0x060000
    qreal dpi=QApplication::screens().at(0)->physicalDotsPerInch();
#endif

    qreal iconSizeMm=6; // Размер иконки в миллиметрах (рекомендованный)
    qreal iconSizeInch=iconSizeMm/25.4; // Размер иконки в дюймах
    qreal iconSizePx=iconSizeInch*dpi;

    return iconSizePx;
}


// Замена в CSS-стиле все вхождения подстроки META_ICON_SIZE на вычисленный размер иконки в пикселях
QString CssHelper::replaceCssMetaIconSize(const QString &styleText)
{
    QMap<QString, qreal> table;

    // Перебираются имена названий размеров иконок
    for ( auto sizeName : fixedParameters.interfaceIconSizeAvailableMap.keys() )
    {
        // Во втором аргументе значения QMap-таблицы хранится коэффициент масштабирования
        table[sizeName] = getCalculateIconSizePx() *
                          fixedParameters.interfaceIconSizeAvailableMap.at( sizeName ).second;
    }

    QString resultText = styleText;

    for (auto name : table.keys())
    {
      qreal value=table.value(name);
      resultText.replace( name, QString::number( (int) value ) );
    }

    return resultText;
}


QString CssHelper::removeCssComments(const QString& input)
{
    QString result;
    result.reserve(input.length());

    int pos = 0;
    int length = input.length();
    bool inComment = false;

    while (pos < length) {
        if (!inComment) {
            // Начало комментария
            int commentStart = input.indexOf("/*", pos);

            if (commentStart == -1) {
                // Комментариев больше нет - в результат добавляется оставшийся текст
                result.append(input.mid(pos));
                break;
            } else {
                // Добавляется текст до начала комментария
                result.append(input.mid(pos, commentStart - pos));
                pos = commentStart + 2; // Пропускаем "/*"
                inComment = true;
            }
        } else {
            // Поиск конца комментария
            int commentEnd = input.indexOf("*/", pos);

            if (commentEnd == -1) {
                // Комментарий не закрыт - просто выход
                break;
            } else {
                pos = commentEnd + 2; // Пропускается "*/"
                inComment = false;
            }
        }
    }

    return result;
}


void CssHelper::extractCssStyles()
{
    // Удаление устаревших стилей, которые размещались по устаревшим соглашениям
    removeVeryOldCssStyles();

    // Удаление устаревших стилей, которые имеют более старую версию чем те что хранятся в ресурсах
    removePreviousThemes();


    QString dirName=globalParameters.getWorkDirectory();

    // Если в рабочей директории нет каталога с темами
    QDir themesDir(dirName+"/themes");
    if ( !themesDir.exists() )
    {
        // Каталог с темами распаковывается из ресурсов
        mytetraFiles.createThemesFiles( dirName+"/themes" );
    }
    else // Иначе есть каталог с темами
    {
        // Надо проверить все ли каталоги тем существуют.
        // custom пропускается: своих файлов у неё нет, она на основе Default
        for ( const auto &themeName : fixedParameters.themesAvailableList )
        {
            if(themeName=="custom")
                continue;

            QDir themeDir(dirName+"/themes/"+themeName);
            if ( !themeDir.exists() )
            {
                // Каталог с темой распаковывается из ресурсов
                mytetraFiles.createThemesFiles( dirName+"/themes", themeName );
            }
        }
    }
}


// Удаление устаревших стилей, размещаемых по устаревшим соглашениям
void CssHelper::removeVeryOldCssStyles()
{
    QString dirName=globalParameters.getWorkDirectory();

    // Устаревший файл стиля, который ранее находился рядом с конфиг-файлами
    // а не в отдельной директории, удаляется
    QFile fromFile(dirName+"/stylesheet.css");
    if ( fromFile.exists() )
    {
        fromFile.remove();
    }

    // Устаревший каталог стиля, который делал пользователь gee12, удаляется
    QDir veryOldstyleDir(dirName+"/style");
    if ( veryOldstyleDir.exists() )
    {
        DiskHelper::removeDirectory( veryOldstyleDir.absolutePath() );
    }
}


// Удаление стилей с более старой версией чем хранимой в ресурсах
void CssHelper::removePreviousThemes()
{
    for ( const auto &themeName : fixedParameters.themesAvailableList )
    {
        // Своя тема не имеет эталона в ресурсах: сравнивать не с чем
        if(themeName=="custom")
            continue;

        // На диске
        QString hddStyleText = getCssFileContentForTheme(themeName);
        QString hddHeaderText = hddStyleText.section('\n', 0, 0); // Первая строка
        int versionAtHdd = getMyTetraStyleVersion(hddHeaderText);

        // В QRC ресурсах
        QString qrcStyleText = getCssFileContentForTheme(themeName, true);
        QString qrcHeaderText = qrcStyleText.section('\n', 0, 0); // Первая строка
        int versionAtQrc = getMyTetraStyleVersion(qrcHeaderText);

        bool doRemove = false;

        // Если версия на диске не определена, а в ресурсах найдена нормальная версия
        if( versionAtHdd==0 and versionAtQrc>0 )
        {
            doRemove = true;
        }

        // Если версия в ресурсах более новая чем на диске
        if( versionAtHdd < versionAtQrc )
        {
            doRemove = true;
        }

        if ( doRemove )
        {
            QString dirName = globalParameters.getWorkDirectory()+"/themes/"+themeName;

            DiskHelper::removeDirectory(dirName);
        }
    }
}


// Получение номера версии из строки вида
// /* <MyTetraStyle name="dark" version="1" about="QDarkStyleSheet"/> */
int CssHelper::getMyTetraStyleVersion(const QString& text)
{
    QString trimmed = text.trimmed();

    // Проверяется формат комментария /* ... */
    if (!trimmed.startsWith("/*") || !trimmed.endsWith("*/")) {
        return 0;
    }

    // Извлекается содержимое комментария (убирается /* и */ и пробелы вокруг XML-тега)
    QString content = trimmed.mid(2, trimmed.length() - 4).trimmed();

    // Проверяется наличие тега MyTetraStyle с атрибутом version
    QRegularExpression regex(R"(<MyTetraStyle\b[^>]*\bversion\s*=\s*['"](\d+)['"][^>]*/?>)");
    QRegularExpressionMatch match = regex.match(content);

    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }

    return 0;
}


void CssHelper::loadCurrentTheme()
{
    QString themeName=mytetraConfig.getInterfaceTheme();

    // Тема из конфига может отсутствовать в сборке (удалена
    // или конфиг пришел из другой ветки): откат на default
    // вместо неоформленного Fusion
    if( !fixedParameters.themesAvailableList.contains(themeName) )
    {
        qWarning() << "Unknown interface theme in config:" << themeName
                   << ", fallback to default";
        themeName="default";
        mytetraConfig.setInterfaceTheme(themeName);
    }

    applyTheme( themeName );

    // Свои цвета и размеры поверх свежей темы (тема сносит
    // per-widget stylesheets, поэтому после, а не до)
    applyCustomDesign();
}


QString CssHelper::getCssFileContentForTheme(const QString &themeName, bool fromResources)
{
    if ( !fixedParameters.themesAvailableList.contains(themeName) )
    {
        qWarning() << "Incorrect interface theme name: " << themeName;
        return QString();
    }

    // Своя тема собирается на основе Default: своих CSS-файлов нет
    const QString cssName=(themeName=="custom") ? "default" : themeName;

    QString fileName;

    if (fromResources)
    {
        QString targetOs = globalParameters.getTargetOs();
        QString qrcPath = QString(":/resource/standartconfig/")+targetOs+QString("/themes");

        fileName = qrcPath+"/"+cssName+"/stylesheet.css";
    }
    else
    {
        QString dirName = globalParameters.getWorkDirectory();
        fileName = dirName+"/themes/"+cssName+"/stylesheet.css";
    }


    QFile cssFile( fileName );

    bool openResult=cssFile.open(QIODevice::ReadOnly | QIODevice::Text);

    // Если файла не существует
    if(!openResult)
    {
        qWarning() << "Can't find CSS theme file: " << fileName;
        return QString();
    }

    // Берется содержимое CSS-файла
    QString styleText = QTextStream(&cssFile).readAll();

    cssFile.close();

    return styleText;
}


bool CssHelper::applyTheme(const QString &themeName)
{
    QString styleText = getCssFileContentForTheme(themeName);

    if (styleText.isEmpty())
    {
        return false;
    }

    // Удаляются CSS комментарии
    styleText=CssHelper::removeCssComments(styleText);

    // Относительные адреса картинок темы переписываются в абсолютные QRC-адреса.
    // У светлой темы картинок вообще нет (только CSS-файл), а у темной их сотня
    // в подкаталоге pic/. Без подмены темная тема молча теряет все рисованные
    // элементы, если распакованные на диск PNG удалены или не распаковались.
    // После подмены обе темы универсально описываются одним CSS-файлом,
    // а картинки всегда берутся из ресурсов программы
    const QString qrcThemesPrefix=QStringLiteral(":/resource/standartconfig/")
                                 +globalParameters.getTargetOs()
                                 +QStringLiteral("/themes/");
    styleText.replace(QStringLiteral("url(\"themes/"), QStringLiteral("url(\"")+qrcThemesPrefix);
    styleText.replace(QStringLiteral("url('themes/"), QStringLiteral("url('")+qrcThemesPrefix);
    styleText.replace(QStringLiteral("url(themes/"), QStringLiteral("url(")+qrcThemesPrefix);

    // Добавляется CSS-правило, влияющее на размер иконок
    if ( mytetraConfig.getInterfaceIconSize()!="" )
    {
        QString cssIconSizeRules = QString("\nQToolBar, QToolButton, QPushButton { icon-size: %1px; }").
                                   arg( mytetraConfig.getInterfaceIconSize() );
        styleText += cssIconSizeRules;
    }

    // Преобразовываются названия размеров иконок в настоящие пиксели
    styleText=CssHelper::replaceCssMetaIconSize(styleText);

    // Загружается и применяется палитра, хранящаяся в CSS файле в правиле CustomPalette
    CssHelper::loadPalette(styleText);

    // Загруженный стиль оформления применяется
    qApp->setStyleSheet(styleText);

    CssHelper::updateInterface();

    return true;
}


void CssHelper::loadPalette(const QString &cssText)
{
    // Применяются цвета из CSS-правила CustomPalette
    PaletteHelper::applyCustomPalette( PaletteHelper::parseCustomPalette(cssText) );
}


void CssHelper::updateInterface()
{
    // Принудительное обновление всех виджетов чтобы применились новые цвета
    foreach (QWidget *widget, QApplication::allWidgets()) {

        // Для QLabel со ссылками - особый подход
        if (QLabel *label = qobject_cast<QLabel*>(widget)) {
            if (label->text().contains("<a ")) {
                // Принудительное обновление через очистку/установку текста
                QString text = label->text();
                label->setText("");
                // QApplication::processEvents();
                label->setText(text);
            }
        }

        QEvent updateEvent(QEvent::UpdateRequest);
        QApplication::sendEvent(widget, &updateEvent);

        widget->style()->unpolish(widget);
        widget->style()->polish(widget);

        widget->update();
        widget->updateGeometry();

        widget->repaint();

        // qDebug() << "Сброс виджета: " << widget->objectName();
    }

    QApplication::processEvents();


    // Уровень 2: Отложенная перерисовка через 10мс
    QTimer::singleShot(10, []() {
        foreach (QWidget *widget, QApplication::allWidgets()) {
            widget->repaint();
        }
        QApplication::processEvents();
    });


    // Уровень 3: Финальное обновление через 50мс
    QTimer::singleShot(50, []() {
        foreach (QWidget *widget, QApplication::allWidgets()) {
            widget->update();
        }
        QApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
    });


    // После сброса интерфейса слетают курсоры на таблицах и в деревьях,
    // их надо восстановить. Однако установка CSS возможна до того,
    // как появится главное окно, поэтому восстановление можно запускать
    // только если есть главное осно, например при переключении тем
    // Однако курсоры даже после restoreAllWindowState() не восстанавливается
    // Надо разобраться почему даже этот вызов не восстанавливает курсоры
    /*
    MainWindow *mainWindow = find_object<MainWindow>("mainwindow");
    if (mainWindow)
    {
        find_object<MainWindow>("mainwindow")->restoreAllWindowState();
    }
    */
}



// Кастомный дизайн поверх темы. Оригиналы запоминаются один раз
// чтобы сброс возвращал именно тему, а не дефолты Qt
void CssHelper::applyCustomDesign(void)
{
    static bool originalsTaken=false;
    static QFont appFontOrig;
    static QFont treeFontOrig;
    static QFont tableFontOrig;
    static int treeIndentOrig=-1;
    static int tableRowHeightOrig=-1;
    static bool tableAlternatingOrig=false;

    // Шрифт интерфейса: пусто означает шрифт темы
    const QString family=mytetraConfig.get_themeCustomInterfaceFontFamily();
    const int size=mytetraConfig.get_themeCustomInterfaceFontSize();

    if(qApp!=nullptr)
    {
        if(!originalsTaken)
            appFontOrig=qApp->font();

        if(family.isEmpty() && size<=0)
            qApp->setFont(appFontOrig);
        else
        {
            QFont font=qApp->font();

            if(!family.isEmpty())
                font.setFamily(family);

            if(size>0)
                font.setPointSize(size);

            qApp->setFont(font);
        }
    }

    KnowTreeView *tree=find_object<KnowTreeView>("knowTreeView");
    RecordTableView *table=find_object<RecordTableView>("recordTableView");

    if(tree==nullptr && table==nullptr)
        return;

    if(!originalsTaken)
    {
        if(tree!=nullptr)
        {
            treeFontOrig=tree->font();
            treeIndentOrig=tree->indentation();
        }

        if(table!=nullptr)
        {
            tableFontOrig=table->font();
            tableRowHeightOrig=table->verticalHeader()->defaultSectionSize();
            tableAlternatingOrig=table->alternatingRowColors();
        }

        originalsTaken=true;
    }

    // Дерево веток: пустые цвета означают цвета темы
    if(tree!=nullptr)
    {
        QString treeStyle;

        if(!mytetraConfig.get_themeCustomTreeBackground().isEmpty())
            treeStyle+="background: "+mytetraConfig.get_themeCustomTreeBackground()+"; ";

        if(!mytetraConfig.get_themeCustomTreeText().isEmpty())
            treeStyle+="color: "+mytetraConfig.get_themeCustomTreeText()+"; ";

        tree->setStyleSheet(treeStyle.isEmpty() ? QString() :
                            QString("QTreeView { %1 }").arg(treeStyle));

        const int treeSize=mytetraConfig.get_themeCustomTreeFontSize();

        if(treeSize>0)
        {
            QFont font=treeFontOrig;
            font.setPointSize(treeSize);
            tree->setFont(font);
        }
        else
            tree->setFont(treeFontOrig);

        const int indent=mytetraConfig.get_themeCustomTreeIndentation();
        tree->setIndentation(indent>=0 ? indent : treeIndentOrig);
    }

    // Список заметок: пустые цвета означают цвета темы
    if(table!=nullptr)
    {
        QString tableStyle;

        if(!mytetraConfig.get_themeCustomTableBackground().isEmpty())
            tableStyle+="background: "+mytetraConfig.get_themeCustomTableBackground()+"; ";

        if(!mytetraConfig.get_themeCustomTableText().isEmpty())
            tableStyle+="color: "+mytetraConfig.get_themeCustomTableText()+"; ";

        const QString alternate=mytetraConfig.get_themeCustomTableAlternate();

        if(!alternate.isEmpty())
        {
            table->setAlternatingRowColors(true);
            tableStyle+="alternate-background-color: "+alternate+"; ";
        }
        else
            table->setAlternatingRowColors(tableAlternatingOrig);

        table->setStyleSheet(tableStyle.isEmpty() ? QString() :
                             QString("QTableView { %1 }").arg(tableStyle));

        const int tableSize=mytetraConfig.get_themeCustomTableFontSize();

        if(tableSize>0)
        {
            QFont font=tableFontOrig;
            font.setPointSize(tableSize);
            table->setFont(font);
        }
        else
            table->setFont(tableFontOrig);

        const int rowHeight=mytetraConfig.get_themeCustomTableRowHeight();
        table->verticalHeader()->setDefaultSectionSize(
            rowHeight>=0 ? rowHeight : tableRowHeightOrig);
    }
}
