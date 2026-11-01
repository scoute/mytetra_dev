#include "CryptKeyHolder.h"


CryptKeyHolder::CryptKeyHolder(QObject *parent) : QObject(parent)
{

}


CryptKeyHolder::~CryptKeyHolder(void)
{
    // Ключ в памяти затирается при уничтожении
    m_key.fill(0);
}


void CryptKeyHolder::setKey(const QByteArray &key)
{
    m_key=key;
}


QByteArray CryptKeyHolder::key(void) const
{
    return m_key;
}


void CryptKeyHolder::clear(void)
{
    m_key.fill(0);
    m_key.clear();
}


bool CryptKeyHolder::hasKey(void) const
{
    return !m_key.isEmpty();
}
