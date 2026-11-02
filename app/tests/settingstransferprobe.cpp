// Перенос настроек: выгрузка conf/editor/shortcut в один INI и
// загрузка обратно. Только временные файлы и QSettings, без AppContext:
// движок SettingsTransfer от синглтонов не зависит.
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include "models/appConfig/SettingsTransfer.h"

static int fails=0;
static void check(bool ok, const char *name)
{
    qDebug() << (ok ? "PASS" : "FAIL") << name;
    if(!ok)
        ++fails;
}

static void seedFixture(const QString &confPath,
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

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tmp;
    check(tmp.isValid(), "TRANSFER-TMPDIR");

    const QString confPath=tmp.filePath(QStringLiteral("conf.ini"));
    const QString editorPath=tmp.filePath(QStringLiteral("editorconf.ini"));
    const QString shortcutPath=tmp.filePath(QStringLiteral("shortcut.ini"));
    const QString outPath=tmp.filePath(QStringLiteral("mytetra-settings-test.ini"));

    seedFixture(confPath, editorPath, shortcutPath);

    // Выгрузка
    QString error;
    QSettings conf(confPath, QSettings::IniFormat);
    QSettings editorConf(editorPath, QSettings::IniFormat);
    check(SettingsTransfer::exportSettings(&conf, &editorConf, shortcutPath,
                                           outPath, &error),
          "TRANSFER-EXPORT-OK");

    QSettings out(outPath, QSettings::IniFormat);
    check(out.value(QStringLiteral("Meta/format")).toString()==QStringLiteral("1"),
          "TRANSFER-META-FORMAT");

    // Переносимое на месте: оформление, шрифты, список, иконки, хоткеи
    check(out.value(QStringLiteral("Conf/interfaceTheme")).toString()==QStringLiteral("dark-style"),
          "TRANSFER-KEEPS-THEME");
    check(out.value(QStringLiteral("Conf/interfaceIconSize")).toInt()==24,
          "TRANSFER-KEEPS-ICONSIZE");
    check(out.value(QStringLiteral("Conf/recordTableShowFields")).toString()==QStringLiteral("name,author"),
          "TRANSFER-KEEPS-RECORDLIST");
    check(out.value(QStringLiteral("Conf/mainwingeometry")).toByteArray()==QByteArray("fake-geometry-blob"),
          "TRANSFER-KEEPS-GEOMETRY-BYTES");
    check(out.value(QStringLiteral("Conf/attachAppendDir")).toString()==QStringLiteral("/home/user/Документы"),
          "TRANSFER-KEEPS-CYRILLIC");
    check(out.value(QStringLiteral("Editor/default_font")).toString()==QStringLiteral("DejaVu Sans"),
          "TRANSFER-KEEPS-FONT");
    check(out.value(QStringLiteral("Editor/default_font_size")).toInt()==11,
          "TRANSFER-KEEPS-FONTSIZE");
    check(out.value(QStringLiteral("Editor/linkColorExternal")).toString()==QStringLiteral("#0066CC"),
          "TRANSFER-KEEPS-LINKCOLOR");
    check(out.value(QStringLiteral("Shortcuts/editor/bold")).toString()==QStringLiteral("Ctrl+B"),
          "TRANSFER-KEEPS-SHORTCUT");
    check(out.value(QStringLiteral("Shortcuts/note/addNewToEnd")).toString()==QStringLiteral("Ctrl+Alt+N"),
          "TRANSFER-KEEPS-SHORTCUT-GROUP");

    // Опасное отсутствует: версии, пути, секреты
    check(!out.contains(QStringLiteral("Conf/version")), "TRANSFER-SKIPS-CONF-VERSION");
    check(!out.contains(QStringLiteral("Conf/tetradir")), "TRANSFER-SKIPS-TETRADIR");
    check(!out.contains(QStringLiteral("Conf/trashdir")), "TRANSFER-SKIPS-TRASHDIR");
    check(!out.contains(QStringLiteral("Conf/passwordMiddleHash")), "TRANSFER-SKIPS-MIDDLEHASH");
    check(!out.contains(QStringLiteral("Conf/passwordSaveFlag")), "TRANSFER-SKIPS-SAVEFLAG");
    check(!out.contains(QStringLiteral("Editor/version")), "TRANSFER-SKIPS-EDITOR-VERSION");

    // Загрузка в чужие файлы с собственными версией, путями и секретом
    QSettings targetConf(tmp.filePath(QStringLiteral("target-conf.ini")), QSettings::IniFormat);
    targetConf.setValue(QStringLiteral("version"), 61);
    targetConf.setValue(QStringLiteral("tetradir"), QStringLiteral("/new/base"));
    targetConf.setValue(QStringLiteral("passwordMiddleHash"), QStringLiteral("target-secret"));
    targetConf.sync();

    QSettings targetEditor(tmp.filePath(QStringLiteral("target-editor.ini")), QSettings::IniFormat);
    targetEditor.setValue(QStringLiteral("version"), 47);
    targetEditor.sync();

    const QString targetShortcut=tmp.filePath(QStringLiteral("target-shortcut.ini"));

    // У приемника свой хоткей: бэкап должен его сохранить
    {
        QSettings seedSc(targetShortcut, QSettings::IniFormat);
        seedSc.setValue(QStringLiteral("editor/bold"), QStringLiteral("Ctrl+X"));
        seedSc.sync();
    }

    error.clear();
    check(SettingsTransfer::importSettings(&targetConf, &targetEditor, targetShortcut,
                                           outPath, &error),
          "TRANSFER-IMPORT-OK");

    // Применено
    check(targetConf.value(QStringLiteral("interfaceTheme")).toString()==QStringLiteral("dark-style"),
          "TRANSFER-APPLIES-THEME");
    check(targetEditor.value(QStringLiteral("default_font")).toString()==QStringLiteral("DejaVu Sans"),
          "TRANSFER-APPLIES-FONT");

    QSettings targetShortcuts(targetShortcut, QSettings::IniFormat);
    check(targetShortcuts.value(QStringLiteral("editor/bold")).toString()==QStringLiteral("Ctrl+B"),
          "TRANSFER-APPLIES-SHORTCUT");

    // Своё не затёрто: версии, пути, секреты приемника целы
    check(targetConf.value(QStringLiteral("version")).toInt()==61,
          "TRANSFER-KEEPS-TARGET-VERSION");
    check(targetConf.value(QStringLiteral("tetradir")).toString()==QStringLiteral("/new/base"),
          "TRANSFER-KEEPS-TARGET-TETRADIR");
    check(targetConf.value(QStringLiteral("passwordMiddleHash")).toString()==QStringLiteral("target-secret"),
          "TRANSFER-KEEPS-TARGET-SECRET");

    // Резервные копии созданы
    check(!QDir(tmp.path()).entryList(QStringList() << QStringLiteral("target-conf.ini.bak-*"),
                                      QDir::Files).isEmpty(),
          "TRANSFER-BACKUP-CONF");
    check(!QDir(tmp.path()).entryList(QStringList() << QStringLiteral("target-shortcut.ini.bak-*"),
                                      QDir::Files).isEmpty(),
          "TRANSFER-BACKUP-SHORTCUT");

    // Битый файл: без Meta — отказ до любых изменений
    const QString badPath=tmp.filePath(QStringLiteral("bad.ini"));
    QFile bad(badPath);
    bad.open(QIODevice::WriteOnly);
    bad.write("[Conf]\ninterfaceTheme=evil\n");
    bad.close();

    error.clear();
    check(!SettingsTransfer::importSettings(&targetConf, &targetEditor, targetShortcut,
                                            badPath, &error),
          "TRANSFER-REJECTS-NOFORMAT");
    check(!error.isEmpty(), "TRANSFER-REJECTS-WITH-ERROR");
    check(targetConf.value(QStringLiteral("interfaceTheme")).toString()==QStringLiteral("dark-style"),
          "TRANSFER-REJECT-KEEPS-TARGET");

    qDebug() << (fails==0 ? "SETTINGSPROBE-OK" : "SETTINGSPROBE-BAD");
    return fails;
}
