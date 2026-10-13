#include <QApplication>
#include <QTranslator>
#include <QDebug>

#include "LanguageHelper.h"


QString LanguageHelper::m_currentLanguage="";


bool LanguageHelper::switchLanguage(const QString &langCode)
{
    static QTranslator *translator=nullptr;

    // Старый переводчик снимается всегда: иначе строки смешиваются
    if(translator!=nullptr)
    {
        qApp->removeTranslator(translator);
        delete translator;
        translator=nullptr;
    }

    m_currentLanguage="en";

    // Английский исходный: переводчик не нужен
    if(langCode.isEmpty() || langCode=="en")
        return true;

    translator=new QTranslator(qApp);

    QString langFileName=":/resource/translations/mytetra_"+langCode+".qm";

    if(!translator->load(langFileName))
    {
        qWarning() << "LanguageHelper: cant load" << langFileName;
        delete translator;
        translator=nullptr;
        return false;
    }

    qApp->installTranslator(translator);
    m_currentLanguage=langCode;

    return true;
}


QString LanguageHelper::currentLanguage(void)
{
    return m_currentLanguage;
}
