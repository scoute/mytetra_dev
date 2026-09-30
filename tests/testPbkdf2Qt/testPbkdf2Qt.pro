TEMPLATE = app
CONFIG += console warn_on testcase c++14
QT += core testlib

INCLUDEPATH += ../../app/src

SOURCES += tst_pbkdf2qt.cpp \
    ../../app/src/libraries/crypt/Pbkdf2Qt.cpp
