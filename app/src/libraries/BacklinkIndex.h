#ifndef BACKLINKINDEX_H
#define BACKLINKINDEX_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

class KnowTreeModel;


// Обратный индекс ссылок mytetra://note/ между заметками.
// Прямые пары sourceId -> {targetIds} живут в сайдкаре
// data/backlinks.xml рядом с mytetra.xml, обратный индекс
// строится в памяти при загрузке. Полный скан базы бывает
// только один раз (нет сайдкара) или по кнопке "Перепроверить".
// Штатно индекс правится точечно: сохранение записи, удаление,
// подчистка отсутствующих. Самоссылки не индексируются.

class BacklinkIndex : public QObject
{
  Q_OBJECT

public:

  static BacklinkIndex &instance(void);

  // Модель дерева для проверки существования записей и чтения файлов.
  // Лениво подхватывается из knowTreeView, если не задана явно
  void setTreeModel(KnowTreeModel *model);
  KnowTreeModel *treeModel(void);

  // Путь к сайдкар-файлу: рядом с mytetra.xml.
  // tetradir уже указывает на каталог с mytetra.xml (обычно data/),
  // итог data/backlinks.xml относительно корня базы
  static QString sidecarFileName(void);

  bool isLoaded(void) const;

  // Загрузка сайдкара. false если файла нет или он бит
  bool load(void);

  // Атомарная запись сайдкара через QSaveFile
  bool save(void) const;

  // Загрузка если есть, иначе полный скан с записью сайдкара
  void loadOrBuild(void);

  // Полный скан всех записей базы (первый запуск, кнопка)
  void buildFull(void);

  // Доидексировать записи дерева, которых нет в индексе
  // (импорт, синхро, внешняя правка). Возвращает число добавленных
  int ensureIndexed(void);

  // Перепарсить одну запись после сохранения. Самоссылки режутся
  void updateSource(const QString &recordId);

  // Убрать источники (удаление записей/веток)
  void removeSources(const QStringList &ids);

  // Убрать пары с несуществующими источником или целью
  // Возвращает число убранных пар
  int purgeMissing(void);

  // Входящие ссылки: кто ссылается на recordId.
  // Несуществующие источники сюда тоже попадают,
  // панель красит их красным через isSourceStale()
  QSet<QString> backlinksOf(const QString &recordId) const;

  // Источник есть в индексе, но записи с таким id нет в дереве
  bool isSourceStale(const QString &sourceId) const;

  QStringList staleSources(void) const;

  // Все исходящие note-ссылки из текста (без самоссылки selfId)
  static QSet<QString> parseOutgoing(const QString &selfId,
                                     const QString &text);

signals:

  // Индекс поменялся, видимой панели пора перечитаться
  void backlinksChanged(void);

private:

  explicit BacklinkIndex(QObject *parent=nullptr);

  void rebuildReverse(void);
  bool recordExists(const QString &recordId) const;
  QString recordTextFileName(const QString &recordId) const;
  void ensureLoaded(void);
  void collectAllRecordIds(QSet<QString> &ids) const;

  KnowTreeModel *m_treeModel=nullptr;
  bool m_loaded=false;

  // Прямые пары: источник -> цели
  QMap<QString, QSet<QString>> m_forward;

  // Обратные: цель -> источники
  QMap<QString, QSet<QString>> m_reverse;
};

#endif // BACKLINKINDEX_H
