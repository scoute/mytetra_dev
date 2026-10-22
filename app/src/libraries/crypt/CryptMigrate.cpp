#include "CryptMigrate.h"

#include <QProgressDialog>
#include <QApplication>

#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/appConfig/AppConfig.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/crypt/CryptService.h"
#include "libraries/crypt/CipherBackend.h"

extern AppConfig mytetraConfig;
extern DataBaseConfig dataBaseConfig;
extern GlobalParameters globalParameters;


void CryptMigrate::collectCryptBranches(TreeItem *item,
                                        QList<TreeItem *> &branches)
{
    if(item==nullptr)
        return;

    if(item->getField("crypt")=="1")
        branches.append(item);

    for(int i=0; i<item->childCount(); ++i)
        collectCryptBranches(item->child(i), branches);
}


CryptMigrateResult CryptMigrate::reencryptAll(KnowTreeModel *model,
                                              QProgressDialog *progress)
{
    CryptMigrateResult result;

    if(model==nullptr)
        return result;

    // Крипта закрыта: ключа нет, расшифровка даст мусор.
    // Вызыватель проверяет заранее, здесь страховка
    if(globalParameters.getCryptKey().isEmpty())
        return result;

    QList<TreeItem *> branches;
    collectCryptBranches(model->getItem(QModelIndex()), branches);

    if(progress!=nullptr)
    {
        progress->setMaximum(branches.size());
        progress->setValue(0);
    }

    const QByteArray key=globalParameters.getCryptKey();

    for(int i=0; i<branches.size(); ++i)
    {
        branches[i]->switchToDecrypt();
        branches[i]->switchToEncrypt();

        result.branches++;

        if(progress!=nullptr)
        {
            progress->setValue(i+1);
            qApp->processEvents();

            if(progress->wasCanceled())
            {
                result.canceled=true;
                break;
            }
        }
    }

    // Проверочный blob пароля тоже переводится на новый шифр,
    // иначе база вечно таскает хвост старого формата
    QByteArray check=QByteArray::fromBase64(
        dataBaseConfig.get_middle_hash_check_data().toLatin1());

    if(!check.isEmpty())
    {
        QByteArray plain=CryptService::decryptByteArray(key, check);

        if(!plain.isEmpty())
            dataBaseConfig.set_middle_hash_check_data(
                QString(CryptService::encryptByteArray(key, plain).toBase64()));
    }

    model->save();

    return result;
}
