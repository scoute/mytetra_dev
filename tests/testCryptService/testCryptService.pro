TEMPLATE = app
CONFIG += console warn_on testcase c++14
QT += core widgets testlib

INCLUDEPATH += ../../app/src

SOURCES += tst_cryptservice.cpp \
    ../../app/src/libraries/crypt/CryptService.cpp \
    ../../app/src/libraries/crypt/RC5Simple.cpp
