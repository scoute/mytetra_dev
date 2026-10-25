# QCodeEditor syntax-highlight core, vendored for MyTetra.
# Подключается из app/app.pro одной строкой:
#   include(../thirdParty/qcodeeditor/qcodeeditor.pri)
# Все пути через $$PWD, поэтому в Qt Creator файлы лежат
# аккуратным деревом, а не как ../internal.

INCLUDEPATH += $$PWD/include

HEADERS += $$PWD/include/internal/QBashHighlighter.hpp \
           $$PWD/include/internal/QCLikeHighlighter.hpp \
           $$PWD/include/internal/QCssHighlighter.hpp \
           $$PWD/include/internal/QCXXHighlighter.hpp \
           $$PWD/include/internal/QGLSLHighlighter.hpp \
           $$PWD/include/internal/QHighlightBlockRule.hpp \
           $$PWD/include/internal/QHighlightRule.hpp \
           $$PWD/include/internal/QIniHighlighter.hpp \
           $$PWD/include/internal/QJSONHighlighter.hpp \
           $$PWD/include/internal/QLanguage.hpp \
           $$PWD/include/internal/QLuaHighlighter.hpp \
           $$PWD/include/internal/QPhpHighlighter.hpp \
           $$PWD/include/internal/QPythonHighlighter.hpp \
           $$PWD/include/internal/QSqlHighlighter.hpp \
           $$PWD/include/internal/QStyleSyntaxHighlighter.hpp \
           $$PWD/include/internal/QSyntaxStyle.hpp \
           $$PWD/include/internal/QXMLHighlighter.hpp

SOURCES += $$PWD/src/internal/QBashHighlighter.cpp \
           $$PWD/src/internal/QCLikeHighlighter.cpp \
           $$PWD/src/internal/QCssHighlighter.cpp \
           $$PWD/src/internal/QCXXHighlighter.cpp \
           $$PWD/src/internal/QGLSLHighlighter.cpp \
           $$PWD/src/internal/QIniHighlighter.cpp \
           $$PWD/src/internal/QJSONHighlighter.cpp \
           $$PWD/src/internal/QLanguage.cpp \
           $$PWD/src/internal/QLuaHighlighter.cpp \
           $$PWD/src/internal/QPhpHighlighter.cpp \
           $$PWD/src/internal/QPythonHighlighter.cpp \
           $$PWD/src/internal/QSqlHighlighter.cpp \
           $$PWD/src/internal/QStyleSyntaxHighlighter.cpp \
           $$PWD/src/internal/QSyntaxStyle.cpp \
           $$PWD/src/internal/QXMLHighlighter.cpp

RESOURCES += $$PWD/qcodeeditor_resources.qrc
