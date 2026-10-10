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


QIcon IconHelper::colorStripIcon(const QString &iconFileName, const QColor &stripColor)
{
    return QIcon(new ColorStripIconEngine(iconFileName, stripColor));
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


ColorStripIconEngine::ColorStripIconEngine(const QString &iconFileName,
                                           const QColor &stripColor)
    : iconFileName(iconFileName),
      stripColor(stripColor)
{

}


QPixmap ColorStripIconEngine::render(const QSize &size,
                                     QIcon::Mode mode, QIcon::State state) const
{
    if(size.isEmpty())
        return QPixmap();

    QPixmap result(size);
    result.fill(Qt::transparent);

    QPainter painter(&result);

    // Глиф из набора текущей темы (резолвится здесь, а не в конструкторе)
    QIcon glyph(IconHelper::resolvedPath(iconFileName));
    QPixmap glyphPixmap=glyph.pixmap(size, mode, state);
    painter.drawPixmap((size.width()-glyphPixmap.width())/2,
                       (size.height()-glyphPixmap.height())/2,
                       glyphPixmap);

    // Текущий цвет полосой внизу значка (стиль LibreOffice),
    // а не заливкой всего фона: тонкие контурные иконки иначе тонут
    const int stripHeight=qMax(3, size.height()/5);
    painter.fillRect(0, size.height()-stripHeight,
                     size.width(), stripHeight, stripColor);
    painter.end();

    return result;
}


void ColorStripIconEngine::paint(QPainter *painter, const QRect &rect,
                                 QIcon::Mode mode, QIcon::State state)
{
    QPixmap rendered=render(rect.size(), mode, state);
    painter->drawPixmap(rect.topLeft(), rendered);
}


QPixmap ColorStripIconEngine::pixmap(const QSize &size,
                                     QIcon::Mode mode, QIcon::State state)
{
    return render(size, mode, state);
}


QSize ColorStripIconEngine::actualSize(const QSize &size,
                                       QIcon::Mode mode, QIcon::State state)
{
    return QIcon(IconHelper::resolvedPath(iconFileName)).actualSize(size, mode, state);
}


QIconEngine *ColorStripIconEngine::clone(void) const
{
    return new ColorStripIconEngine(iconFileName, stripColor);
}


// Ключ кеша зависит от темы и цвета полосы: после переключения темы
// или смены цвета pixmap-кеш Qt промахивается и иконка перерисовывается
QString ColorStripIconEngine::key(void) const
{
    return "ColorStrip:"+iconFileName+":"+IconHelper::themeIconSet()
           +":"+QString::number(stripColor.rgba(), 16);
}
