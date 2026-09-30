#include <QtTest>

#include "libraries/FixedParameters.h"

// Тестовый класс для проверки неизменяемых параметров приложения.
// Фиксирует списки полей записей и веток, константы формул и тем.

class TestFixedParameters : public QObject
{
    Q_OBJECT

private slots:

    // Проверка создания и уничтожения объекта
    void constructorDestructor()
    {
        FixedParameters params;
        Q_UNUSED(params);
    }

    // Проверка списков полей записей
    void recordFields()
    {
        QVERIFY(FixedParameters::recordFieldAvailableList.contains("id"));
        QVERIFY(FixedParameters::recordFieldAvailableList.contains("name"));
        QVERIFY(FixedParameters::recordFieldAvailableList.contains("attachCount"));

        QVERIFY(FixedParameters::isRecordFieldAvailable("name"));
        QVERIFY(!FixedParameters::isRecordFieldAvailable("nonexistent"));

        QVERIFY(FixedParameters::isRecordFieldNatural("name"));
        QVERIFY(!FixedParameters::isRecordFieldNatural("attachCount"));

        QVERIFY(FixedParameters::isRecordFieldCalculable("hasAttach"));
        QVERIFY(FixedParameters::isRecordFieldCalculable("attachCount"));
        QVERIFY(!FixedParameters::isRecordFieldCalculable("name"));
    }

    // Проверка соответствия списков: природные + вычисляемые покрывают доступные текстовые поля
    void recordFieldListsConsistent()
    {
        // Каждое природное поле должно быть в общем списке доступных
        foreach (const QString &field, FixedParameters::recordNaturalFieldAvailableList)
        {
            QVERIFY2(FixedParameters::recordFieldAvailableList.contains(field),
                     qPrintable(QString("Поле %1 нет в recordFieldAvailableList").arg(field)));
        }

        // Каждое вычисляемое поле должно быть в общем списке доступных
        foreach (const QString &field, FixedParameters::recordCalculableFieldAvailableList)
        {
            QVERIFY2(FixedParameters::recordFieldAvailableList.contains(field),
                     qPrintable(QString("Поле %1 нет в recordFieldAvailableList").arg(field)));
        }

        // Шифруемые поля записей должны быть доступными
        foreach (const QString &field, FixedParameters::recordFieldCryptedList)
        {
            QVERIFY2(FixedParameters::isRecordFieldAvailable(field),
                     qPrintable(QString("Шифруемое поле %1 недоступно").arg(field)));
        }
    }

    // Проверка полей веток
    void itemFields()
    {
        QVERIFY(FixedParameters::itemFieldAvailableList.contains("id"));
        QVERIFY(FixedParameters::itemFieldAvailableList.contains("name"));
        QVERIFY(FixedParameters::itemFieldAvailableList.contains("icon"));

        foreach (const QString &field, FixedParameters::itemFieldCryptedList)
        {
            QVERIFY2(FixedParameters::itemFieldAvailableList.contains(field),
                     qPrintable(QString("Шифруемое поле ветки %1 недоступно").arg(field)));
        }
    }

    // Проверка текстовых и числовых констант
    void constants()
    {
        QCOMPARE(FixedParameters::iconsRelatedDirectory, QString("icons"));
        QCOMPARE(FixedParameters::appTextId, QString("mytetra"));

        QCOMPARE(FixedParameters::mathExpDescriptionType, QString("mathExpression"));
        QCOMPARE(FixedParameters::mathExpVersion, 1);
        QCOMPARE(FixedParameters::mathExpVersionNumberLen, 4);
        QCOMPARE(FixedParameters::mathExpHeaderLen, 29);

        QVERIFY(FixedParameters::themesAvailableList.contains("default"));
        QVERIFY(FixedParameters::themesAvailableList.contains("dark"));
    }

    // Проверка таблицы размеров иконок
    void iconSizeMap()
    {
        QCOMPARE(FixedParameters::interfaceIconSizeAvailableMap.size(), 9);
        QVERIFY(FixedParameters::interfaceIconSizeAvailableMap.contains("META_ICON_SINGLE_SIZE"));
        QCOMPARE(FixedParameters::interfaceIconSizeAvailableMap.at("META_ICON_SINGLE_SIZE").second, 1.0f);
        QCOMPARE(FixedParameters::interfaceIconSizeAvailableMap.at("META_ICON_DOUBLE_SIZE").second, 2.0f);
    }

    // Проверка описаний полей: фильтрация по переданному списку
    void fieldDescriptions()
    {
        QMap<QString, QString> all = FixedParameters::recordFieldDescription(
            FixedParameters::recordFieldAvailableList);

        QVERIFY(all.contains("name"));
        QVERIFY(all.contains("author"));

        QMap<QString, QString> filtered = FixedParameters::recordFieldDescription(QStringList({"name"}));
        QCOMPARE(filtered.size(), 1);
        QVERIFY(filtered.contains("name"));

        QMap<QString, QString> empty = FixedParameters::recordFieldDescription(QStringList());
        QVERIFY(empty.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestFixedParameters)

#include "tst_fixedparameters.moc"
