TEMPLATE = app
CONFIG += console warn_on testcase c++14
QT += core testlib

INCLUDEPATH += ../../app/src

HEADERS += ../../app/src/libraries/FixedParameters.h \
    ../../app/src/libraries/OrderedMap.h

SOURCES += tst_fixedparameters.cpp \
    ../../app/src/libraries/FixedParameters.cpp \
    ../../app/src/libraries/OrderedMap.cpp
