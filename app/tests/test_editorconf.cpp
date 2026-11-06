// EditorConfig на QTest, линия v2 (см. 502, 503).
// Отсутствующий или пустой известный параметр подменяется дефолтом
// из актуальной таблицы вместо падения. Неизвестные имена по-прежнему
// роняют программу и здесь не гоняются.
// Фикстура живет в репо (app/tests/data/editorconf.ini), тест мутирует
// ее копию в QTemporaryDir.
#include <QtTest>
#include <QApplication>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include "libraries/wyedit/EditorConfig.h"

class TestEditorConf : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);

    void emptyFallsBackToDefault(void);
    void missingFallsBackToDefault(void);

private:

    QTemporaryDir m_tmp;
    QString m_ini;

    void copyFixture(void);
};

// Дефолт ключа из актуальной таблицы (тройки имя/тип/значение)
static QString tableDefault(const QString &key)
{
    QStringList table=EditorConfig::get_parameter_table_46(false);

    for(int i=0; i<table.size()/3; ++i)
        if(table.at(i*3)==key)
            return table.at(i*3+2);

    return QStringLiteral("<no-such-key>");
}

void TestEditorConf::initTestCase(void)
{
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");
    m_ini=m_tmp.filePath(QStringLiteral("editorconf.ini"));
}

void TestEditorConf::copyFixture(void)
{
    // Репозиторная фикстура -> свежая writable-копия под каждый слот
    QFile::remove(m_ini);
    QVERIFY2(QFile::copy(QStringLiteral(TEST_DATA_DIR "/editorconf.ini"), m_ini),
             "repo fixture must be copied to temp dir");
    QVERIFY2(QFile::exists(m_ini), "temp ini must exist");
}

void TestEditorConf::emptyFallsBackToDefault(void)
{
    copyFixture();

    // Известный ключ с пустым значением: раньше criticalError, теперь дефолт
    {
        QSettings fix(m_ini, QSettings::IniFormat);
        fix.setValue(QStringLiteral("default_font"), QString());
        fix.sync();
    }

    EditorConfig ed(m_ini);

    const QString expectedDefault=tableDefault(QStringLiteral("default_font"));
    QVERIFY2(!expectedDefault.isEmpty(), "table default must be known");
    QCOMPARE(ed.get_default_font(), expectedDefault);
    QCOMPARE(ed.get_configFallbackCount(), 1);
}

void TestEditorConf::missingFallsBackToDefault(void)
{
    copyFixture();

    EditorConfig ed(m_ini);

    // Удаленный после загрузки ключ: то же поведение
    ed.settingsHandle()->remove(QStringLiteral("default_font"));
    ed.settingsHandle()->sync();

    const QString expectedDefault=tableDefault(QStringLiteral("default_font"));
    QCOMPARE(ed.get_default_font(), expectedDefault);
    QCOMPARE(ed.get_configFallbackCount(), 1);
    QCOMPARE(ed.get_configFallbackHits().value(QStringLiteral("default_font")), 1);
}

QTEST_MAIN(TestEditorConf)

#include "test_editorconf.moc"
