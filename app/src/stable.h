// Прекомпилированный заголовок (qmake PRECOMPILED_HEADER).
// Ускоряет сборку: частые заголовки компилируются один раз,
// а не в каждой единице трансляции.
// Состав = заголовки Qt с частотой >= 10 по коду (подсчитано
// grep'ом, список можно расширять) + ходовой STL.
// Только стабильное. Своих заголовков здесь нет специально:
// любая правка своего файла иначе дёргала бы пересборку всего
// проекта. Платформенного (windows.h, unistd.h) нет: для него
// важен порядок относительно _WIN32_WINNT.

#ifdef __cplusplus

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QSet>
#include <QList>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QDateTime>
#include <QTimer>
#include <QRegularExpression>
#include <QMimeData>
#include <QDebug>
#include <QtDebug>
#include <QWidget>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QToolButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QMenu>
#include <QAction>
#include <QHeaderView>
#include <QListWidget>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QColor>
#include <QPainter>
#include <QScrollBar>

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <memory>

#endif
