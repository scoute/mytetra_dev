QCodeEditor syntax-highlight core, vendored for MyTetra (code-block languages).

Upstream: https://github.com/Megaxela/QCodeEditor (MIT, see LICENSE.MIT).
Upstream commit used: master as of 2026-10 (files only, no git history).

Taken (unchanged, except include search path via app.pro):
  include/QCXXHighlighter, QGLSLHighlighter, QJSONHighlighter, QLanguage,
  QLuaHighlighter, QPythonHighlighter, QStyleSyntaxHighlighter, QSyntaxStyle,
  QXMLHighlighter, QHighlightRule, QHighlightBlockRule (forwarding headers)
  include/internal/*.hpp (same set, real declarations)
  src/internal/QCXXHighlighter.cpp, QGLSLHighlighter.cpp, QJSONHighlighter.cpp,
  QLanguage.cpp, QLuaHighlighter.cpp, QPythonHighlighter.cpp,
  QStyleSyntaxHighlighter.cpp, QSyntaxStyle.cpp, QXMLHighlighter.cpp
  resources/default_style.xml, resources/languages/{cpp,glsl,lua,python}.xml
  (JSON and XML highlighters are hardcoded, they need no language xml)

Extra language data (same engine, same style):
  resources/languages/{c,go,rust,java,csharp,js,bash}.xml
  Keyword lists for go/rust/java/csharp/js and shell keywords for bash.xml
  are extracted mechanically from QSourceHighlite languagedata.cpp
  (https://github.com/Waqar144/QSourceHighlite, MIT, see
  LICENSE.QSourceHighlite-MIT; only word lists were taken, no code).
  Bash builtins in bash.xml are a hand-curated standard list, because the
  upstream shell table contains app-specific command names.
  c.xml keywords are from the ISO C17 standard (hand-written list).
  HTML and Angular reuse QXMLHighlighter (no xml needed);
  React reuses js.xml keywords.

MyTetra-side adapters (our files, upstream patterns reused):
  include/QCLikeHighlighter, include/internal/QCLikeHighlighter.hpp,
  src/internal/QCLikeHighlighter.cpp - QCXXHighlighter logic with the keyword
  file as a constructor parameter (used for C, Java, C#, Go, Rust, JS).
  include/QBashHighlighter, include/internal/QBashHighlighter.hpp,
  src/internal/QBashHighlighter.cpp - QPythonHighlighter-pattern highlighter
  for Bash (# comments, "..."/'...' strings, $var highlighting).

Deliberately NOT taken:
  QCodeEditor widget, QLineNumberArea, QFramedTextAttribute,
  completers (QGLSL/QPython/QLua), example/, CMake files.
  MyTetra notes are rich-text HTML documents, not plain-text editors:
  highlighting is applied one-shot onto the selected code block and baked
  into the note HTML, so the editor widget and live-document machinery
  are not needed.

Adaptations for qmake (this repo):
  qcodeeditor_resources.qrc replaces upstream resources/qcodeeditor_resources.qrc,
  keeping the same resource paths (:/default_style.xml, :/languages/*.xml)
  so Q_INIT_RESOURCE(qcodeeditor_resources) in upstream sources works unchanged.
  app.pro adds ../thirdParty/qcodeeditor/include to INCLUDEPATH.
