TEMPLATE=app
QT-=gui

message(Touch mimeTeX qmake)

QMAKE_CFLAGS += -DAA
QMAKE_CXXFLAGS += -DAA

# Вендорный mimeTeX 2004 года: сотни варнингов старого C-кода (-Wall).
# Чинить его не будем, просто глушим выводы для этой цели:
# warn_off убирает -Wall только здесь, остальной проект не трогает
CONFIG += warn_off

SOURCES=mimetex.c \
        gifsave.c \

HEADERS=mimetex.h \
        texfonts.h

        
OBJECTS_DIR = $${_PRO_FILE_PWD_}/build/obj
DESTDIR = $${_PRO_FILE_PWD_}/build/bin

win32-msvc {
    DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS
}
