#include <QFile>
#include <QHash>

#include "IconHelper.h"
#include "models/appConfig/AppConfig.h"

extern AppConfig mytetraConfig;


QString IconHelper::themeIconSet(void)
{
    const QString theme=mytetraConfig.getInterfaceTheme();

    if(theme=="modern")
        return "pic_modern";

    if(theme=="midnight")
        return "pic_dark";

    if(theme=="midnight-thin")
        return "pic_dark_thin";

    return "pic";
}


QString IconHelper::resolvedPath(const QString &iconFileName)
{
    if(iconFileName.isEmpty())
        return QString();

    const QString iconSet=themeIconSet();

    // Тематический файл берется только если он реально есть в ресурсах,
    // иначе общий классический (флаги, сплэш, заглушки не дублируются)
    if(iconSet!="pic")
    {
        const QString themePath=":/resource/"+iconSet+"/"+iconFileName;

        if(QFile::exists(themePath))
            return themePath;
    }

    return ":/resource/pic/"+iconFileName;
}


QIcon IconHelper::get(const QString &iconFileName)
{
    return QIcon(new ThemeIconEngine(iconFileName));
}


ThemeIconEngine::ThemeIconEngine(const QString &iconFileName)
    : iconFileName(iconFileName)
{

}


QIcon ThemeIconEngine::currentIcon(void) const
{
    // Подлинные иконки кешируются по полному пути: один файл
    // парсится один раз, а не при каждой отрисовке
    static QHash<QString, QIcon> iconCache;

    const QString path=IconHelper::resolvedPath(iconFileName);

    QHash<QString, QIcon>::iterator it=iconCache.find(path);

    if(it==iconCache.end())
        it=iconCache.insert(path, QIcon(path));

    return it.value();
}


void ThemeIconEngine::paint(QPainter *painter, const QRect &rect,
                            QIcon::Mode mode, QIcon::State state)
{
    currentIcon().paint(painter, rect, Qt::AlignCenter, mode, state);
}


QPixmap ThemeIconEngine::pixmap(const QSize &size,
                                QIcon::Mode mode, QIcon::State state)
{
    return currentIcon().pixmap(size, mode, state);
}


QSize ThemeIconEngine::actualSize(const QSize &size,
                                  QIcon::Mode mode, QIcon::State state)
{
    return currentIcon().actualSize(size, mode, state);
}


QIconEngine *ThemeIconEngine::clone(void) const
{
    return new ThemeIconEngine(iconFileName);
}


// Ключ кеша зависит от темы: после переключения темы pixmap-кеш
// Qt промахивается и иконки перерисовываются из нового набора
QString ThemeIconEngine::key(void) const
{
    return "ThemeIcon:"+iconFileName+":"+IconHelper::themeIconSet();
}
