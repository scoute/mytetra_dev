#ifndef FAVORITES_H
#define FAVORITES_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

// Избранные заметки с переключаемым хранилищем: сайдкар
// data/favorites.xml (по умолчанию) или поля favorite прямо
// в записях mytetra.xml. Переключение хранилища в настройках
// переносит существующие звездочки, старое чистится.
// Порядок пользовательский (как favor у gee12, только канон -
// сайдкар, mytetra.xml не трогаем): ids() отдает в порядке списка,
// панель умеет перетаскивание мышью и пункты "выше/ниже".
// Базы gee12 (формат 1.3, числовой favor) открываются и при
// первом старте отдают свой порядок в сайдкар (seedFromFavorOrder).

class KnowTreeModel;

class Favorites : public QObject
{
  Q_OBJECT

 public:

  static const QString StorageSidecar; // "sidecar"
  static const QString StorageXml;     // "xml"

  // Активное хранилище из настроек, "sidecar" по умолчанию
  QString storage(void) const;

  // Сменить хранилище: звездочки обоих бэкендов объединяются
  // в новое, второе чистится. Пустышка при том же режиме
  void setStorage(const QString &mode);

  // Id в порядке списка (сайдкар) или по номерам favor (xml)
  QStringList ids(void) const;
  bool isFavorite(const QString &id) const;
  void setFavorite(const QString &id, bool on);

  // Переставить список целиком в заданном порядке. Потерять
  // звездочку молча нельзя: неупомянутые дописываются в хвост,
  // дубли режутся. В xml-режиме пустышка: там порядок - номера
  // favor в mytetra.xml, их не правим
  void setOrder(const QStringList &orderedIds);

  // Однократный посев из gee12-базы: сайдкара еще нет, а в записях
  // стоят числовые favor - порядок забирается оттуда (сначала favor
  // по возрастанию, затем наши favorite="1" по имени). mytetra.xml
  // не правится, файл пишется только если есть что сеять
  void seedFromFavorOrder(KnowTreeModel *model);

  // Выкинуть id удаленных заметок (нужно только сайдкару:
  // в XML мертвых id не бывает). Возвращает число выкинутых
  int purgeDead(void);

  // Тихое удаление id из сайдкара без сигнала: для чистки
  // внутри пересборки панели, которая сама уже в курсе
  void removeIdsSilent(const QStringList &dead);

 signals:

  void favoritesChanged(void);

 public:

  explicit Favorites(QObject *parent=nullptr);

 private:

  QString sidecarFileName(void) const;

  void sidecarEnsureLoaded(void) const;
  bool sidecarLoad(void);
  bool sidecarSave(void) const;
  void sidecarReplace(const QStringList &fresh);

  // Все id с флагом favorite/favor по дереву, упорядоченные: сначала
  // числовой favor gee12 по возрастанию, затем favorite="1" по имени.
  // Зашифрованные ветки без ключа пропускаются: читать их записи нельзя
  QStringList xmlIdListOrdered(void) const;
  void xmlWriteAll(const QStringList &fresh);

  // Порядок сайдкара как есть (включая мертвые id: при возврате
  // базы звездочки оживают сами)
  mutable QStringList m_sidecarOrder;
  // База, для которой загружен сайдкар (fix/544): при смене tetradir
  // кеш сбрасывается, иначе чужие звезды затирают новый favorites.xml
  mutable QString m_sidecarTetradir;
  mutable bool m_sidecarLoaded=false;
};

#endif // FAVORITES_H
