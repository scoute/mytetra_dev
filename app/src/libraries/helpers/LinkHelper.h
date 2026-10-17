#ifndef LINKHELPER_H
#define LINKHELPER_H

class QString;
class QUrl;

class LinkHelper
{
public:
    LinkHelper();

    static void gotoReference(QString href);

    // Проверка и разбор внутренней ссылки mytetra://note/<id>.
    // Публичны: нужны форматтерам и индексу обратных ссылок
    static bool isHrefInternal(QString href);
    static QString getIdFromInternalHref(QString href);

    // Относительная ссылка без схемы (картинка, аттач из импорта):
    // раскрывается относительно каталога записи и проверяется.
    // Пусто если схема есть, выход за каталог или файла нет
    static QString resolveRelativeLink(const QString &workDir,
                                       const QString &href);

private:

    static bool openLinkWithDesktopServices(const QString &link);

    static bool isExternal(const QUrl &url);

};

#endif // LINKHELPER_H
