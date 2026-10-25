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
  resources/default_style.xml (plus 4 additive entries used by the new
  language files: Declaration, Flow, Package, Literal),
  resources/languages/{cpp,glsl,lua,python}.xml
  (JSON and XML highlighters are hardcoded, they need no language xml)

Extra language data (same engine, same style):
  resources/languages/{c,go,rust,java,csharp,js,bash,cmake,make,yaml,sql,
  php,qml,typescript,css}.xml and ini.xml
  Keyword lists are extracted mechanically from QSourceHighlite
  languagedata.cpp (https://github.com/Waqar144/QSourceHighlite, MIT, see
  LICENSE.QSourceHighlite-MIT; only word lists were taken, no code).
  Exceptions: bash builtins are a hand-curated standard list (the upstream
  shell table contains app-specific command names); c.xml keywords are from
  the ISO C17 standard; ini.xml is hand-written (no upstream data).
  Behavior notes: make "ifn?def/ifn?eq" entries work as regexes by luck
  (match ifdef/ifndef); cmake commands are matched in lower, UPPER and
  original case because CMake is case-insensitive.
  HTML and Angular reuse QXMLHighlighter (no xml needed);
  React reuses js.xml keywords.

MyTetra-side adapters (our files, upstream patterns reused):
  include/QCLikeHighlighter, include/internal/QCLikeHighlighter.hpp,
  src/internal/QCLikeHighlighter.cpp - QCXXHighlighter logic with the keyword
  file as a constructor parameter (used for C, Java, C#, Go, Rust, JS, QML,
  TypeScript).
  include/QBashHighlighter, include/internal/QBashHighlighter.hpp,
  src/internal/QBashHighlighter.cpp - QPythonHighlighter-pattern highlighter
  for Bash (# comments only after blank, "..."/'...' strings, $var/${var}/$?
  highlighting, function calls, VAR= assignments, [[ ]]/;;/||/&& operators,
  -x/--long options; rule ideas mirror nano sh.nanorc, code is original).
  Reused for CMake, Make and YAML (all share # comments and ${VAR} style).
  include/QSqlHighlighter, QPhpHighlighter, QCssHighlighter, QIniHighlighter
  (+ headers and src/internal/*.cpp) - small adapters in the same style:
  SQL (-- and /* */ comments), PHP (QCLike plus $variables and # comments),
  CSS (properties only when followed by a colon, @-rules, hex colors),
  INI ([sections], key=, ;/# comments).

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
