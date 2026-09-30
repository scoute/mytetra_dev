TEMPLATE = app
CONFIG += console warn_on testcase c++14
QT += core testlib

INCLUDEPATH += ../../app/src

SOURCES += tst_rc5simple.cpp \
    ../../app/src/libraries/crypt/RC5Simple.cpp
