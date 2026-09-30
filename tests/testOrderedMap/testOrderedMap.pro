TEMPLATE = app
CONFIG += console warn_on testcase c++14
QT += core testlib

INCLUDEPATH += ../../app/src

SOURCES += tst_orderedmap.cpp \
    ../../app/src/libraries/OrderedMap.cpp
