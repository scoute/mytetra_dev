#ifndef MCP_TOOLS_H
#define MCP_TOOLS_H

#include <QString>
#include <QJsonObject>

// Read-only инструменты MCP-сервера поверх живой модели дерева.
// Структура читается из модели, тексты - тихо с диска через
// Record::getTextForStats (нет файла или закрытый шифр = пусто).
// Модель не меняется

class KnowTreeModel;
class TreeItem;
class QModelIndex;
class QJsonArray;

class McpTools
{
public:
  explicit McpTools(KnowTreeModel *model);

  // Плоский список веток в порядке дерева: {branches: [{id, name, parent_id}]}
  QJsonObject listBranches(void);

  // Записи ветки: {notes: [{id, name, tags, branch_id, branch_path}]}.
  // error заполняется если ветка не найдена
  QJsonObject listNotes(const QString &branchId, bool recursive, QString &error);

  // Одна запись с плейнтекстом: {note: {id, name, tags, branch_id,
  // branch_path, format, text, truncated}}. error если запись не найдена
  QJsonObject getNote(const QString &noteId, QString &error);

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

private:
  KnowTreeModel *m_model;

  void appendBranches(const QModelIndex &parent, const QString &parentId, QJsonArray &out);
  void collectBranchNotes(TreeItem *item, bool recursive, QJsonArray &out);
  void searchBranch(TreeItem *item, const QString &queryLower, int limit, QJsonArray &out);
  void searchModel(const QModelIndex &parent, const QString &queryLower, int limit, QJsonArray &out);

  // Тихое чтение текста с однократным повтором если файл
  // перезаписали прямо во время чтения (смена mtime)
  QString readTextWithRetry(class Record *record);

  static QString branchPathOf(TreeItem *item);
  static QJsonObject noteObject(class Record *record, const QString &branchId, const QString &branchPath);
};

#endif // MCP_TOOLS_H
