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
