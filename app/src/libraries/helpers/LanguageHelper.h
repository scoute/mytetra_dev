#ifndef LANGUAGEHELPER_H
#define LANGUAGEHELPER_H

#include <QString>


// Переключение языка интерфейса на лету без перезапуска.
// Переводчик хранится статически: иначе снятие старого и установка
// нового невозможны (в main translator жил на стеке)
class LanguageHelper
{
public:
    // Загрузить .qm для кода языка и поставить переводчиком приложения.
    // Пустой код или "en" снимает перевод (исходные английские строки).
    // Возвращает true если переводчик установлен (для en всегда true)
    static bool switchLanguage(const QString &langCode);

    // Текущий установленный код (что реально загружено)
    static QString currentLanguage(void);

private:
    static QString m_currentLanguage;
};

#endif // LANGUAGEHELPER_H
