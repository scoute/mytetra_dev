// Перенос настроек на QTest (см. 494, линия v2).
// Порт settingstransferprobe: выгрузка conf/editor/shortcut в один INI
// и загрузка обратно. Только временные файлы и QSettings, без AppContext:
// движок SettingsTransfer от синглтонов не зависит.
#include <QtTest>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QStringList>
#include <QTemporaryDir>

#include "models/appConfig/SettingsTransfer.h"

class TestSettingsTransfer : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);

    void exportKeepsPortable(void);
    void exportSkipsDangerous(void);
    void importApplies(void);
    void importKeepsTargetOwn(void);
    void importBackups(void);
    void importRejectsBad(void);

private:

    QTemporaryDir m_tmp;
    QString m_out;

    void seedFixture(const QString &confPath,
                     const QString &editorPath,
                     const QString &shortcutPath);
};

void TestSettingsTransfer::seedFixture(const QString &confPath,
                                       const QString &editorPath,
                                       const QString &shortcutPath)
{
    QSettings conf(confPath, QSettings::IniFormat);
    conf.setValue(QStringLiteral("version"), 61);
    conf.setValue(QStringLiteral("programm"), QStringLiteral("mytetra"));
    conf.setValue(QStringLiteral("tetradir"), QStringLiteral("/old/base"));
    conf.setValue(QStringLiteral("trashdir"), QStringLiteral("/old/trash"));
    conf.setValue(QStringLiteral("passwordMiddleHash"), QStringLiteral("secret-hash"));
    conf.setValue(QStringLiteral("passwordSaveFlag"), 1);
    conf.setValue(QStringLiteral("interfaceTheme"), QStringLiteral("dark-style"));
    conf.setValue(QStringLiteral("interfaceIconSize"), 24);
    conf.setValue(QStringLiteral("recordTableShowFields"), QStringLiteral("name,author"));
    conf.setValue(QStringLiteral("mainwingeometry"), QByteArray("fake-geometry-blob"));
    conf.setValue(QStringLiteral("attachAppendDir"), QStringLiteral("/home/user/Документы"));
    conf.sync();

    QSettings editorConf(editorPath, QSettings::IniFormat);
    editorConf.setValue(QStringLiteral("version"), 47);
    editorConf.setValue(QStringLiteral("default_font"), QStringLiteral("DejaVu Sans"));
    editorConf.setValue(QStringLiteral("default_font_size"), 11);
    editorConf.setValue(QStringLiteral("linkColorExternal"), QStringLiteral("#0066CC"));
    editorConf.sync();

    QSettings shortcuts(shortcutPath, QSettings::IniFormat);
    shortcuts.beginGroup(QStringLiteral("editor"));
    shortcuts.setValue(QStringLiteral("bold"), QStringLiteral("Ctrl+B"));
    shortcuts.setValue(QStringLiteral("save"), QStringLiteral("Ctrl+S"));
    shortcuts.endGroup();
    shortcuts.beginGroup(QStringLiteral("note"));
    shortcuts.setValue(QStringLiteral("addNewToEnd"), QStringLiteral("Ctrl+Alt+N"));
    shortcuts.endGroup();
    shortcuts.sync();
}

void TestSettingsTransfer::initTestCase(void)
{
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    const QString confPath=m_tmp.filePath(QStringLiteral("conf.ini"));
    const QString editorPath=m_tmp.filePath(QStringLiteral("editorconf.ini"));
    const QString shortcutPath=m_tmp.filePath(QStringLiteral("shortcut.ini"));
    m_out=m_tmp.filePath(QStringLiteral("mytetra-settings-test.ini"));

    seedFixture(confPath, editorPath, shortcutPath);

    // Выгрузка
    QString error;
    QSettings conf(confPath, QSettings::IniFormat);
    QSettings editorConf(editorPath, QSettings::IniFormat);
    QVERIFY2(SettingsTransfer::exportSettings(&conf, &editorConf, shortcutPath,
                                              m_out, &error),
             "export must succeed");
}

void TestSettingsTransfer::exportKeepsPortable(void)
{
    QSettings out(m_out, QSettings::IniFormat);
    QCOMPARE(out.value(QStringLiteral("Meta/format")).toString(), QStringLiteral("1"));

    // Переносимое на месте: оформление, шрифты, список, иконки, хоткеи
    QCOMPARE(out.value(QStringLiteral("Conf/interfaceTheme")).toString(),
             QStringLiteral("dark-style"));
    QCOMPARE(out.value(QStringLiteral("Conf/interfaceIconSize")).toInt(), 24);
    QCOMPARE(out.value(QStringLiteral("Conf/recordTableShowFields")).toString(),
             QStringLiteral("name,author"));
    QCOMPARE(out.value(QStringLiteral("Conf/mainwingeometry")).toByteArray(),
             QByteArray("fake-geometry-blob"));
    QCOMPARE(out.value(QStringLiteral("Conf/attachAppendDir")).toString(),
             QStringLiteral("/home/user/Документы"));
    QCOMPARE(out.value(QStringLiteral("Editor/default_font")).toString(),
             QStringLiteral("DejaVu Sans"));
    QCOMPARE(out.value(QStringLiteral("Editor/default_font_size")).toInt(), 11);
    QCOMPARE(out.value(QStringLiteral("Editor/linkColorExternal")).toString(),
             QStringLiteral("#0066CC"));
    QCOMPARE(out.value(QStringLiteral("Shortcuts/editor/bold")).toString(),
             QStringLiteral("Ctrl+B"));
    QCOMPARE(out.value(QStringLiteral("Shortcuts/note/addNewToEnd")).toString(),
             QStringLiteral("Ctrl+Alt+N"));
}

void TestSettingsTransfer::exportSkipsDangerous(void)
{
    QSettings out(m_out, QSettings::IniFormat);

    // Опасное отсутствует: версии, пути, секреты
    QVERIFY2(!out.contains(QStringLiteral("Conf/version")), "conf version must be skipped");
    QVERIFY2(!out.contains(QStringLiteral("Conf/tetradir")), "tetradir must be skipped");
    QVERIFY2(!out.contains(QStringLiteral("Conf/trashdir")), "trashdir must be skipped");
    QVERIFY2(!out.contains(QStringLiteral("Conf/passwordMiddleHash")),
             "middle hash must be skipped");
    QVERIFY2(!out.contains(QStringLiteral("Conf/passwordSaveFlag")),
             "save flag must be skipped");
    QVERIFY2(!out.contains(QStringLiteral("Editor/version")),
             "editor version must be skipped");
}

void TestSettingsTransfer::importApplies(void)
{
    // Загрузка в чужие файлы с собственными версией, путями и секретом
    QSettings targetConf(m_tmp.filePath(QStringLiteral("target-conf.ini")),
                         QSettings::IniFormat);
    targetConf.setValue(QStringLiteral("version"), 61);
    targetConf.setValue(QStringLiteral("tetradir"), QStringLiteral("/new/base"));
    targetConf.setValue(QStringLiteral("passwordMiddleHash"),
                        QStringLiteral("target-secret"));
    targetConf.sync();

    QSettings targetEditor(m_tmp.filePath(QStringLiteral("target-editor.ini")),
                           QSettings::IniFormat);
    targetEditor.setValue(QStringLiteral("version"), 47);
    targetEditor.sync();

    const QString targetShortcut=m_tmp.filePath(QStringLiteral("target-shortcut.ini"));

    // У приемника свой хоткей: бэкап должен его сохранить
    {
        QSettings seedSc(targetShortcut, QSettings::IniFormat);
        seedSc.setValue(QStringLiteral("editor/bold"), QStringLiteral("Ctrl+X"));
        seedSc.sync();
    }

    QString error;
    QVERIFY2(SettingsTransfer::importSettings(&targetConf, &targetEditor, targetShortcut,
                                              m_out, &error),
             "import must succeed");

    // Применено
    QCOMPARE(targetConf.value(QStringLiteral("interfaceTheme")).toString(),
             QStringLiteral("dark-style"));
    QCOMPARE(targetEditor.value(QStringLiteral("default_font")).toString(),
             QStringLiteral("DejaVu Sans"));

    QSettings targetShortcuts(targetShortcut, QSettings::IniFormat);
    QCOMPARE(targetShortcuts.value(QStringLiteral("editor/bold")).toString(),
             QStringLiteral("Ctrl+B"));
}

void TestSettingsTransfer::importKeepsTargetOwn(void)
{
    QSettings targetConf(m_tmp.filePath(QStringLiteral("target-conf.ini")),
                         QSettings::IniFormat);

    // Своё не затёрто: версии, пути, секреты приемника целы
    QCOMPARE(targetConf.value(QStringLiteral("version")).toInt(), 61);
    QCOMPARE(targetConf.value(QStringLiteral("tetradir")).toString(),
             QStringLiteral("/new/base"));
    QCOMPARE(targetConf.value(QStringLiteral("passwordMiddleHash")).toString(),
             QStringLiteral("target-secret"));
}

void TestSettingsTransfer::importBackups(void)
{
    // Резервные копии созданы
    QVERIFY2(!QDir(m_tmp.path()).entryList(
                 QStringList() << QStringLiteral("target-conf.ini.bak-*"),
                 QDir::Files).isEmpty(),
             "conf backup must exist");
    QVERIFY2(!QDir(m_tmp.path()).entryList(
                 QStringList() << QStringLiteral("target-shortcut.ini.bak-*"),
                 QDir::Files).isEmpty(),
             "shortcut backup must exist");
}

void TestSettingsTransfer::importRejectsBad(void)
{
    // Битый файл: без Meta — отказ до любых изменений
    const QString badPath=m_tmp.filePath(QStringLiteral("bad.ini"));
    QFile bad(badPath);
    QVERIFY2(bad.open(QIODevice::WriteOnly), "bad fixture must be writable");
    bad.write("[Conf]\ninterfaceTheme=evil\n");
    bad.close();

    QSettings targetConf(m_tmp.filePath(QStringLiteral("target-conf.ini")),
                         QSettings::IniFormat);
    QSettings targetEditor(m_tmp.filePath(QStringLiteral("target-editor.ini")),
                           QSettings::IniFormat);
    const QString targetShortcut=m_tmp.filePath(QStringLiteral("target-shortcut.ini"));

    QString error;
    QVERIFY2(!SettingsTransfer::importSettings(&targetConf, &targetEditor, targetShortcut,
                                              badPath, &error),
             "import of bad file must fail");
    QVERIFY2(!error.isEmpty(), "error must be set");
    QCOMPARE(targetConf.value(QStringLiteral("interfaceTheme")).toString(),
             QStringLiteral("dark-style"));
}

QTEST_APPLESS_MAIN(TestSettingsTransfer)

#include "test_settingstransfer.moc"
