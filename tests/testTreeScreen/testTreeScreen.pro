TEMPLATE = app
CONFIG += console warn_on testcase c++14
QT += gui core xml svg network widgets printsupport testlib

INCLUDEPATH += ../../app/src

include(../mytetra_harness.pri)
include(../knowtree_fixture.pri)

SOURCES += tst_treescreen.cpp
