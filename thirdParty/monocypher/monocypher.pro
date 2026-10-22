# Monocypher 4.x, современная криптография для MyTetra:
# XChaCha20-Poly1305 (шифрование), Argon2id (KDF из пароля),
# Blake2b (хеширование). Источник: https://github.com/LoupVaillant/Monocypher,
# лицензия COPYING_MONOCYPHER (BSD-2 или CC0 на выбор).
# Отдельная статическая либа, симметрично opus/ogg.

TEMPLATE = lib
CONFIG += staticlib
TARGET = monocypher
QT -= core gui

OBJECTS_DIR = $${_PRO_FILE_PWD_}/build/obj
DESTDIR = $${_PRO_FILE_PWD_}/build/lib

INCLUDEPATH += $${_PRO_FILE_PWD_}

SOURCES += \
    monocypher.c

HEADERS += \
    monocypher.h
