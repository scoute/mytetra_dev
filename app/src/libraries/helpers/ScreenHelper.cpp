#include <QtGlobal>
#include <QApplication>
#include <QScreen>

#include "ScreenHelper.h"


ScreenHelper::ScreenHelper()
{

}


int ScreenHelper::getSizeX(void)
{
#if QT_VERSION >= 0x040000 && QT_VERSION < 0x050000
  int size=(qApp->desktop()->availableGeometry()).width();
#endif

#if QT_VERSION >= 0x050000 && QT_VERSION < 0x060000
  // Гарда пустого списка экранов: offscreen/headless падал на at(0) (fix/547)
  if(QApplication::screens().isEmpty())
    return 0;
  int size=(QApplication::screens().at(0)->availableGeometry()).width();
#endif

  return size;
}


int ScreenHelper::getSizeY(void)
{
#if QT_VERSION >= 0x040000 && QT_VERSION < 0x050000
  int size=(qApp->desktop()->availableGeometry()).height();
#endif

#if QT_VERSION >= 0x050000 && QT_VERSION < 0x060000
  if(QApplication::screens().isEmpty())
    return 0;
  int size=(QApplication::screens().at(0)->availableGeometry()).height();
#endif

  return size;
}
