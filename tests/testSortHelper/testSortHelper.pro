TEMPLATE = app
CONFIG += console warn_on testcase c++14
QT += core testlib

INCLUDEPATH += ../../app/src

SOURCES += tst_sorthelper.cpp \
    ../../app/src/libraries/helpers/SortHelper.cpp
