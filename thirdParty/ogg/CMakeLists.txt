# libogg 1.3.5, статика для Qt6-линии (см. ogg.pri — те же файлы).
add_library(ogg STATIC
  src/framing.c
  src/bitwise.c
)

target_include_directories(ogg PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
