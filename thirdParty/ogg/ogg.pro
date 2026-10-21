# libogg 1.3.5, Ogg-контейнер для .opus-файлов. Источник: https://github.com/xiph/ogg,
# лицензия COPYING_OGG (BSD). Отдельная статическая либа, симметрично opus.

TEMPLATE = lib
CONFIG += staticlib
TARGET = ogg
QT -= core gui

OBJECTS_DIR = $${_PRO_FILE_PWD_}/build/obj
DESTDIR = $${_PRO_FILE_PWD_}/build/lib

INCLUDEPATH += $${_PRO_FILE_PWD_} \
               $${_PRO_FILE_PWD_}/include

SOURCES += \
    src/framing.c \
    src/bitwise.c

HEADERS += \
    include/ogg/ogg.h \
    include/ogg/os_types.h \
    include/ogg/config_types.h \
    src/crctable.h

