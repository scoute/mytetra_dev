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

private:

    static bool openLinkWithDesktopServices(const QString &link);

    static bool isExternal(const QUrl &url);

};

#endif // LINKHELPER_H
