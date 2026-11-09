#ifndef SETTINGSTRANSFER_H
#define SETTINGSTRANSFER_H

#include <QString>
#include <QStringList>

class QSettings;

// Перенос настроек программы между сборками: выгрузка conf.ini,
// editorconf.ini и shortcut.ini в один INI-файл и загрузка обратно.
// Движок чистый (только QSettings и файлы), без синглтонов —
// вызывается и из диалога настроек, и из headless-стенда.
//
// Выгружается не всё: версия схем (миграции приемника должны
// отработать сами), абсолютные пути базы и корзины (на другой
// машине их может не быть) и секреты шифрования (хеш и флаг
// сохраненного пароля) никогда не покидают свою сборку.
// knownbases.ini и database.ini не переносятся вовсе: первое —
// список баз с абсолютными путями, второе — параметры конкретной
// базы (соль, хеши), а не программы.

class SettingsTransfer : public QObject
{
  Q_OBJECT

public:

  // Ключи conf.ini вне переноса
  static QStringList confSkipKeys(void);

  // Ключи editorconf.ini вне переноса
  static QStringList editorSkipKeys(void);

  // Выгрузка в один INI-файл: [Meta], [Conf], [Editor],
  // [CustomTheme], [Shortcuts/<раздел>]. conf и editorConf — живые
  // или временные QSettings, shortcutPath — путь к shortcut.ini
  static bool exportSettings(QSettings *conf,
                             QSettings *editorConf,
                             const QString &shortcutPath,
                             const QString &outPath,
                             QString *error);

  // Загрузка: проверка [Meta], резервные копии *.bak-<штамп> рядом
  // с файлами, применение в живые conf/editorConf (sync в конце),
  // shortcut.ini файлом (перечитать через initKeyTable — дело
  // вызывателя). Версии схем и пути приемника не трогаются
  static bool importSettings(QSettings *conf,
                             QSettings *editorConf,
                             const QString &shortcutPath,
                             const QString &inPath,
                             QString *error);

private:

  // Копирование группы (пустая — весь корень) с пропуском ключей
  static void copyGroup(QSettings *src,
                        const QString &srcGroup,
                        QSettings *dst,
                        const QString &dstGroup,
                        const QStringList &skip);

  // Резервная копия файла рядом с ним
  static bool backupFile(const QString &path, QString *error);

  // Текущий штамп для имен копий и файлов
  static QString currentStamp(void);

  // Выгрузка своего файла текстуры бумаги в [CustomTheme]:
  // ключ themeCustomNotePaperImage хранит абсолютный путь рабочего
  // каталога, файла на другой машине нет. Байты едут с собой
  static void exportCustomTheme(QSettings *conf, QSettings *out);

  // Загрузка: байты из [CustomTheme] кладутся в custom-theme
  // приемника, ключ в conf переписывается на новый абсолютный путь.
  // Без секции ничего не трогается (свой старый файл как был)
  static bool importCustomTheme(QSettings *conf,
                                QSettings *in,
                                QString *error);
};

#endif // SETTINGSTRANSFER_H
