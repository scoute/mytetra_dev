#ifndef _TRASHMONITORING_H_
#define	_TRASHMONITORING_H_

#include <QWidget>
#include <QDir>
#include <QSet>


class TrashMonitoring
{

public:
 TrashMonitoring(void);
 virtual ~TrashMonitoring(void);

 void init(const QString trashPath);
 
 // Функция, вызываемая после фактического добавления файла в корзину
 // принимает имя файла без пути к директории
 void addFile(const QString fileName);
 
 void update(void);

private:
 
 enum AppendFileMode {
     Head=0,
     Tail=1
 };

 void appendFileToList(const QString fileName,
                       const unsigned int fileTime,
                       const qint64 fileSize,
                       const AppendFileMode appendFileMode);

 void removeOldesFile(void);
 
 QString m_path;
 QDir m_dir;
 qint64 m_dirSize;
  
 struct FileData
 {
  QString fileName;
  unsigned int fileTime;
  qint64 fileSize;
 };

 //! Список обслуживаемых файлов со всеми их данными,
 //! файлы удаляются в последовательности поступления в этот список
 QList<FileData> m_filesList;

 //! Список имен файлов, для ускорения проверок
 QSet<QString> m_availableFilesName;

};

#endif	/* _TRASHMONITORING_H_ */

