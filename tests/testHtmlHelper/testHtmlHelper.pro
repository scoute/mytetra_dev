TEMPLATE = app
CONFIG += console warn_on testcase c++14
QT += core testlib

INCLUDEPATH += ../../app/src

SOURCES += tst_htmlhelper.cpp \
    ../../app/src/libraries/helpers/HtmlHelper.cpp
