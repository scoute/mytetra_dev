#ifndef _CRYPTMIGRATE_H_
#define _CRYPTMIGRATE_H_

#include <QString>

class KnowTreeModel;
class QProgressDialog;

// Перешифровка базы новым шифром (п.4 плана).
// Для каждой зашифрованной ветки прогоняется штатная пара
// switchToDecrypt + switchToEncrypt: вся механика полей, файлов
// и подветок уже оттестирована этими методами, нового кода
// шифрования тут нет. Метод берется из настройки cryptMethod
// (расшифровка отличает старый шифр по магии блоба сама).
// Отмена останавливает между ветками: готовая часть уже новая,
// остаток старый, смешанная база полностью читается
struct CryptMigrateResult
{
    int branches=0;
    int records=0;
    bool canceled=false;
};

class CryptMigrate
{

public:

    static CryptMigrateResult reencryptAll(KnowTreeModel *model,
                                           QProgressDialog *progress=nullptr);

private:

    static void collectCryptBranches(class TreeItem *item,
                                       QList<class TreeItem *> &branches);
};

#endif /* _CRYPTMIGRATE_H_ */
