#ifndef SYNCSCOPE_H
#define SYNCSCOPE_H

#include "SyncManifest.h"

// Срез плана по поддереву ветки (без модели/файлов — чистая логика,
// покрывается тестами).
//
// scope = выбранная ветка + все потомки + заметки в них.
// Предки добавляются ТОЛЬКО для размещения отсутствующей у нас ветки
// (иначе скачиваемой ветке некуда встать). Их изменения полей
// (переименование, иконка, родитель) в действия среза НЕ входят:
// пользователь выбрал поддерево, а не предков.
namespace SyncScope
{

SyncPlan slice(const SyncPlan &plan,
               const SyncManifest &local,
               const SyncManifest &remote,
               const QString &scopeRootId);

} // namespace SyncScope

#endif // SYNCSCOPE_H
