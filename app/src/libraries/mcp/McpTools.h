#ifndef MCP_TOOLS_H
#define MCP_TOOLS_H

#include <QString>
#include <QList>
#include <QMap>
#include <QSet>
#include <QJsonObject>

// Инструменты MCP-сервера поверх живой модели дерева.
// Чтение: структура из модели, тексты - тихо с диска через
// Record::getTextForStats (нет файла или закрытый шифр = пусто).
// Запись: apply-методы меняют модель, сохранение решает вызывающий

class KnowTreeModel;
class TreeItem;
class QModelIndex;
class QJsonArray;

class McpTools
{
public:
  explicit McpTools(KnowTreeModel *model);

  // Лимиты ответов: экономия токенов, обрезка с флагом truncated
  static const int DEFAULT_LIST_LIMIT=50;
  static const int MAX_LIST_LIMIT=200;
  static const int DEFAULT_NOTE_CHARS=20000;
  static const int MAX_TEXT_LENGTH=200000;

  // Плоский список веток в порядке дерева: {branches: [{id, name, parent_id}]}
  QJsonObject listBranches(void);

  // Записи ветки: {notes, truncated}. error если ветка не найдена
  QJsonObject listNotes(const QString &branchId, bool recursive, int limit,
                        QString &error);

  // Одна запись с плейнтекстом (до maxChars): {note: {...}}.
  // error если запись не найдена
  QJsonObject getNote(const QString &noteId, int maxChars, QString &error);

  // Подстрока без учета регистра по имени, меткам и тексту:
  // {results: [{id, name, branch_path, snippet}]}.
  // error при пустом запросе
  QJsonObject searchNotes(const QString &query, int limit, QString &error);

  // HTML в плейнтекст без QTextDocument (под QCoreApplication рискован):
  // вырезание script/style и тегов, unescape сущностей,
  // схлопывание пробелов
  static QString htmlToPlain(const QString &html);

  // Плейнтекст в минимальный HTML для хранения (зеркало htmlToPlain)
  static QString plainToHtml(const QString &plain);

  // --- Запись. Статические apply-методы работают поверх любой живой
  // модели (MCP-процесс или GUI) и НЕ сохраняют: вызывающий решает сам.
  // Прямые create/update-методы сохраняют модель после применения ---

  // Новая подветка с готовым id (пустой parentId = верхний уровень).
  // Возвращает {id}. Ошибки: пустое имя/id, нет родителя, коллизия id,
  // шифрованная родительская ветка
  static QJsonObject applyCreateBranch(KnowTreeModel *model,
                                       const QString &parentId,
                                       const QString &branchId,
                                       const QString &name,
                                       QString &error);

  // Новая запись с готовым id. Возвращает {id} реально сохраненной
  // записи. Ошибки: пустое имя, нет ветки, шифрованная ветка
  static QJsonObject applyCreateNote(KnowTreeModel *model,
                                     const QString &branchId,
                                     const QString &noteId,
                                     const QString &name,
                                     const QString &html,
                                     const QString &tags,
                                     QString &error);

  // Обновление полей записи, меняются только помеченные флагами.
  // Возвращает {id, updated_fields}. Ошибки: нечего менять, нет записи,
  // пустое имя, шифрованная запись
  static bool applyUpdateNote(KnowTreeModel *model,
                              const QString &noteId,
                              bool hasName, const QString &name,
                              bool hasHtml, const QString &html,
                              bool hasTags, const QString &tags,
                              QString &error,
                              QStringList &updatedFields);

  // Прямые методы поверх собственной модели с сохранением
  QJsonObject createBranchDirect(const QString &parentId,
                                 const QString &branchId,
                                 const QString &name,
                                 QString &error);

  QJsonObject createNoteDirect(const QString &branchId,
                               const QString &noteId,
                               const QString &name,
                               const QString &html,
                               const QString &tags,
                               QString &error);

  QJsonObject updateNoteDirect(const QString &noteId,
                               bool hasName, const QString &name,
                               bool hasHtml, const QString &html,
                               bool hasTags, const QString &tags,
                               QString &error);

  // Удаление записи: файлы в корзину, запись из XML долой, сайдкар
  // бэклинков чистится. Возвращает {id, deleted}. Шифр не мешает:
  // для перемещения в корзину ключ не нужен
  static bool applyDeleteNote(KnowTreeModel *model,
                              const QString &noteId,
                              QString &error);

  // Удаление ветки со всеми подветками и записями. Возвращает
  // {id, deleted, deleted_notes}. Подтверждения нет: флаг режима
  // и есть подтверждение
  static QJsonObject applyDeleteBranch(KnowTreeModel *model,
                                       const QString &branchId,
                                       QString &error);

  QJsonObject deleteNoteDirect(const QString &noteId, QString &error);
  QJsonObject deleteBranchDirect(const QString &branchId, QString &error);

  // --- Переименование, перенос, поиск дублей, статистика ---

  // Новое имя ветки. Возвращает {id}. Пустое имя отклоняется,
  // шифр не мешает (только атрибут XML)
  static QJsonObject applyRenameBranch(KnowTreeModel *model,
                                       const QString &branchId,
                                       const QString &name,
                                       QString &error);

  // Перенос ветки: asChild=true - внутрь цели, false - соседней после.
  // Возвращает {id, new_parent_id} (пустой - верхний уровень).
  // Циклы, self-move и корень отклоняются моделью
  static QJsonObject applyMoveBranch(KnowTreeModel *model,
                                     const QString &sourceId,
                                     const QString &targetId,
                                     bool asChild,
                                     QString &error);

  // Перенос записи в другую ветку без трогания файлов: id и каталог
  // сохраняются, внутренние ссылки целы. Возвращает {id, branch_id}.
  // В ту же ветку, в шифр и из шифра - отказ
  static QJsonObject applyMoveNote(KnowTreeModel *model,
                                   const QString &noteId,
                                   const QString &targetBranchId,
                                   QString &error);

  QJsonObject renameBranchDirect(const QString &branchId,
                                 const QString &name,
                                 QString &error);
  QJsonObject moveBranchDirect(const QString &sourceId,
                               const QString &targetId,
                               bool asChild,
                               QString &error);
  QJsonObject moveNoteDirect(const QString &noteId,
                             const QString &targetBranchId,
                             QString &error);

  // Группы дублей: по имени (без учета регистра) и по содержимому
  // (sha1 текста). Пустые имена и тексты не сравниваются
  QJsonObject findDuplicates(int limit);

  // Итоги базы как в статусбаре + построчная детализация при detailed
  QJsonObject baseStats(bool detailed);

  // --- Метки ---

  // Все метки с количеством заметок, по убыванию популярности
  QJsonObject listTags(void);

  // Заметки с точным совпадением метки (целиком, без учета регистра).
  // error при пустой метке
  QJsonObject tagNotes(const QString &tag, int limit, QString &error);

  // Заметки без меток (до limit, с флагом truncated)
  QJsonObject untaggedNotes(int limit);

  // Переименование метки по всей базе (совпадение как в tagNotes).
  // Совпадение с собой отклоняется (слияние делается через rename
  // в существующую). Шифрованные без ключа пропускаются в skipped.
  // Возвращает false + error на пустых именах
  static bool applyRenameTag(KnowTreeModel *model,
                             const QString &oldTag,
                             const QString &newTag,
                             QString &error,
                             int &renamed,
                             int &skipped);

  // Удаление метки из всех заметок. Шифрованные пропускаются
  static bool applyDeleteTag(KnowTreeModel *model,
                             const QString &tag,
                             QString &error,
                             int &cleared,
                             int &skipped);

  // Сколько читаемых заметок несет метку (для проверки применения)
  static int countTagNotes(KnowTreeModel *model, const QString &tagLower);

  QJsonObject renameTagDirect(const QString &oldTag,
                              const QString &newTag,
                              QString &error);
  QJsonObject deleteTagDirect(const QString &tag, QString &error);

// --- Избранное ---

// Звездочки активного хранилища (sidecar или поля XML):
// {favorites: [note...]}. Шифрованные записи без ключа и мертвые
// id сайдкара пропускаются. Лимита нет: избранного обычно мало
QJsonObject listFavorites(void);

// Поставить/снять звездочку. Возвращает {id, favorite}.
// Ошибки: нет записи, шифрованная запись
static QJsonObject applyFavoriteNote(KnowTreeModel *model,
                                     const QString &noteId,
                                     bool starred,
                                     QString &error);

QJsonObject favoriteNoteDirect(const QString &noteId, QString &error);
QJsonObject unfavoriteNoteDirect(const QString &noteId, QString &error);

// id звездочек сайдкара напрямую файлом (для проверки применения
// живой записью: синглетон в серверном процессе может быть протухшим)
static QSet<QString> sidecarFavoriteIds(void);

private:
  KnowTreeModel *m_model;

  void appendBranches(const QModelIndex &parent, const QString &parentId, QJsonArray &out);
  void collectBranchNotes(TreeItem *item, bool recursive, QJsonArray &out);
  void collectFromModel(QMap<QString, QList<QJsonObject>> &byName,
                        QMap<QString, QList<QJsonObject>> &byText);
  void searchBranch(TreeItem *item, const QString &queryLower, int limit, QJsonArray &out);
  void searchModel(const QModelIndex &parent, const QString &queryLower, int limit, QJsonArray &out);

  // Тихое чтение текста с однократным повтором если файл
  // перезаписали прямо во время чтения (смена mtime)
  QString readTextWithRetry(class Record *record);

  static QString branchPathOf(TreeItem *item);
  static QJsonObject noteObject(class Record *record, const QString &branchId, const QString &branchPath);
};

#endif // MCP_TOOLS_H
