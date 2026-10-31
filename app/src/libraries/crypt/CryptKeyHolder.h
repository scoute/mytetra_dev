#ifndef CRYPTKEYHOLDER_H
#define CRYPTKEYHOLDER_H

#include <QObject>
#include <QByteArray>

// Держатель ключа шифрования.
// Выделен из GlobalParameters чтобы ключ не жил в
// сервис-локаторе UI-указателей. Каноническое место
// хранения, GlobalParameters::set/getCryptKey делегируют сюда.
class CryptKeyHolder : public QObject
{
    Q_OBJECT

public:

    explicit CryptKeyHolder(QObject *parent=nullptr);
    ~CryptKeyHolder(void) override;

    void setKey(const QByteArray &key);
    QByteArray key(void) const;
    void clear(void);
    bool hasKey(void) const;

private:

    QByteArray m_key;
};

#endif // CRYPTKEYHOLDER_H
