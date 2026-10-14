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

class Favorites : public QObject
{
  Q_OBJECT

 public:

  static Favorites &instance(void);

  static const QString StorageSidecar; // "sidecar"
  static const QString StorageXml;     // "xml"

  // Активное хранилище из настроек, "sidecar" по умолчанию
  QString storage(void) const;

  // Сменить хранилище: звездочки обоих бэкендов объединяются
  // в новое, второе чистится. Пустышка при том же режиме
  void setStorage(const QString &mode);

  QStringList ids(void) const;
  bool isFavorite(const QString &id) const;
  void setFavorite(const QString &id, bool on);

  // Выкинуть id удаленных заметок (нужно только сайдкару:
  // в XML мертвых id не бывает). Возвращает число выкинутых
  int purgeDead(void);

  // Тихое удаление id из сайдкара без сигнала: для чистки
  // внутри пересборки панели, которая сама уже в курсе
  void removeIdsSilent(const QStringList &dead);

 signals:

  void favoritesChanged(void);

 private:

  explicit Favorites(QObject *parent=nullptr);

  QString sidecarFileName(void) const;

  void sidecarEnsureLoaded(void) const;
  bool sidecarLoad(void);
  bool sidecarSave(void) const;
  void sidecarReplace(const QSet<QString> &fresh);

  QSet<QString> xmlIdSet(void) const;
  void xmlWriteAll(const QStringList &fresh);

  mutable bool m_sidecarLoaded=false;
  mutable QSet<QString> m_sidecarIds;
};

#endif // FAVORITES_H
