#include <QUrl>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QDesktopServices>
#include <QDebug>

#include "LinkHelper.h"

#include "views/mainWindow/MainWindow.h"
#include "views/tree/KnowTreeView.h"
#include "models/tree/KnowTreeModel.h"
#include "libraries/FixedParameters.h"
#include "libraries/helpers/ObjectHelper.h"


LinkHelper::LinkHelper()
{

}


void LinkHelper::gotoReference(QString href)
{
    if(href.length()==0)
        return;

    // Если клик по обычной ссылке
    if(!isHrefInternal(href))
    {
        openLinkWithDesktopServices( href );
    }
    else
    {
        // Иначе клик по "внутренней" ссылке с протоколом "mytetra:"

        // Пролучение ID из ссылки
        QString recordId=getIdFromInternalHref(href);

        // todo: вынести следующий код в отдельный метод главного окна

        // Нахождение ветки, в которой лежит данная запись
        QStringList pathToRecord=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model())->getRecordPath(recordId);

        find_object<MainWindow>("mainwindow")->setTreeAndRecordtablePositions(pathToRecord, recordId);
    }
}


bool LinkHelper::openLinkWithDesktopServices(const QString &link)
{
    // Если передан существующий локальный путь, он открывается системным
    // обработчиком напрямую через file:// URL, без вызова shell. Раньше сырая
    // строка уходила в "cmd /C start" (Windows) без кавычек, и метасимволы
    // в пути выполнялись командным интерпретатором как отдельные команды
    QFileInfo directFileInfo(link);
    if(directFileInfo.exists())
        return QDesktopServices::openUrl(QUrl::fromLocalFile(directFileInfo.absoluteFilePath()));

    QUrl url = QUrl(link);

    if(!url.isValid())
        return false;

    QString scheme=url.scheme().toLower();

    // Опасные схемы никогда не открываются
    if(scheme=="javascript" || scheme=="data" || scheme=="vbscript")
        return false;

    // Использовать метод QUrl::isLocalFile() нельзя, так как он просто
    // возвращает true если схема "file" и все.
    // Вместо этого написана специальная функция определения, внешняя это
    // или внутренняя ссылка
    if ( isExternal( url ) || scheme=="mailto" )
    {
        // Для внешних ссылок используется QDesktopServices
        return QDesktopServices::openUrl(url);
    }

    // Локальный file:// URL открывается только если файл существует
    if(scheme=="file")
    {
        QFileInfo fileInfo(url.toLocalFile());
        if(fileInfo.exists())
            return QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));

        return false;
    }

    return false; // Неизвестная схема, ничего не открываем
}


bool LinkHelper::isExternal(const QUrl &url)
{
    // Проверка схемы URL
    QString scheme = url.scheme().toLower();

    QStringList external = QStringList() << "http"
                                         << "https"
                                         << "ftp"
                                         << "sftp";

    return external.contains( scheme );
}


bool LinkHelper::isHrefInternal(QString href)
{
    if(href.contains(QRegExp("^"+FixedParameters::appTextId+":\\/\\/note\\/\\w+$")))
        return true;
    else
        return false;
}


QString LinkHelper::getIdFromInternalHref(QString href)
{
    if(!isHrefInternal(href))
        return "";

    href.replace(QRegExp("^"+FixedParameters::appTextId+":\\/\\/note\\/"), "");

    return href;

}
