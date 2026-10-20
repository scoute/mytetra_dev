#ifndef UNIQUEIDHELPER_H
#define UNIQUEIDHELPER_H

class QString;


QString getUniqueId(void);
QString getUniqueImageName(void);
QString getUniqueImageName(const QString &extension);

#endif // UNIQUEIDHELPER_H
