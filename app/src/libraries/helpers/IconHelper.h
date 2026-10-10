#ifndef _ICONHELPER_H_
#define _ICONHELPER_H_

#include <QIcon>
#include <QIconEngine>
#include <QPixmap>
#include <QPainter>
#include <QColor>


// Выдача иконок из набора текущей темы оформления.
// Старые темы (default, dark) используют классический набор
// :/resource/pic/, modern — :/resource/pic_modern/,
// midnight — :/resource/pic_dark/.
// Имя файла, отсутствующее в тематическом наборе, берется
// из классического (флаги стран, сплэш, точки-заглушки общие)
class IconHelper
{
public:

    // Каталог набора для текущей темы: "pic", "pic_modern" или "pic_dark"
    static QString themeIconSet(void);

    // Прямой путь к файлу иконки для текущей темы (для QPixmap и проверок)
    static QString resolvedPath(const QString &iconFileName);

    // Иконка из набора текущей темы. Путь резолвится движком
    // в момент отрисовки, поэтому переключение темы применяется
    // вживую без перезапуска и без обновления каждого виджета
    static QIcon get(const QString &iconFileName);

    // Иконка цвета текста/фона (кнопки A с полосой): глиф из набора
    // текущей темы плюс полоса цвета внизу (стиль LibreOffice).
    // В отличие от запекания в QPixmap, тема резолвится при каждой
    // отрисовке, поэтому переключение темы не оставляет старый глиф
    static QIcon colorStripIcon(const QString &iconFileName, const QColor &stripColor);

private:

    IconHelper(void) {}
};


// Движок иконки, хранящий только имя файла. Реальный файл
// подставляется из набора текущей темы при каждой отрисовке
class ThemeIconEngine : public QIconEngine
{
public:

    explicit ThemeIconEngine(const QString &iconFileName);
    virtual ~ThemeIconEngine(void) {}

    virtual void paint(QPainter *painter, const QRect &rect,
                       QIcon::Mode mode, QIcon::State state) override;
    virtual QPixmap pixmap(const QSize &size,
                           QIcon::Mode mode, QIcon::State state) override;
    virtual QSize actualSize(const QSize &size,
                             QIcon::Mode mode, QIcon::State state) override;
    virtual QIconEngine *clone(void) const override;
    virtual QString key(void) const override;

private:

    QIcon currentIcon(void) const;

    QString iconFileName;
};


 // Движок кнопок цвета текста/фона: тематический глиф плюс полоса
// текущего цвета внизу значка. Имя файла хранится, реальный файл
// подставляется из набора текущей темы при каждой отрисовке,
// поэтому переключение темы применяется без пересоздания иконки
class ColorStripIconEngine : public QIconEngine
{
public:

    explicit ColorStripIconEngine(const QString &iconFileName,
                                  const QColor &stripColor);
    virtual ~ColorStripIconEngine(void) {}

    virtual void paint(QPainter *painter, const QRect &rect,
                       QIcon::Mode mode, QIcon::State state) override;
    virtual QPixmap pixmap(const QSize &size,
                           QIcon::Mode mode, QIcon::State state) override;
    virtual QSize actualSize(const QSize &size,
                             QIcon::Mode mode, QIcon::State state) override;
    virtual QIconEngine *clone(void) const override;
    virtual QString key(void) const override;

private:

    QPixmap render(const QSize &size,
                   QIcon::Mode mode, QIcon::State state) const;

    QString iconFileName;
    QColor stripColor;
};

#endif // _ICONHELPER_H_
