# DOS Text Editor

[中文](README_zh-CN.md) | English

A character-mode text editor inspired by MS-DOS Edit, written in C17 with zero third-party dependencies. Runs on Windows CMD, Linux terminals, and FreeDOS.

## Features

- Pure C17, zero third-party dependencies
- Windows Console API / POSIX termios / DOS BIOS platform backends
- Double-buffered rendering (minimal flicker, dirty-region diff)
- Char-wrap and horizontal-scroll display modes
- Visual vertical scrollbar (rightmost column) with click and drag
- Visual horizontal scrollbar (bottom row, horizontal-scroll mode only) with click and drag
- Multi-level undo / redo
- Find and replace (case-sensitive or insensitive)
- Full mouse support: click, double-click (word), triple-click (line), drag, wheel, right-click menu
- Menu bar (Alt+letter or mouse click to open; dropdown items clickable by mouse)
- Right-click context menu: Cut / Copy / Paste / Select All / Find Selected
- Line number display (toggle via View menu)
- Internal clipboard (cross-platform, no OS clipboard dependency)
- **UTF-8 multi-byte editing** — CJK and other Unicode characters, codepoint-aware cursor movement, wide-character (2-column) display
- **GBK / UTF-8 auto-detection** — files are detected as UTF-8 or GBK on open, converted internally to UTF-8, and written back in the original encoding on save
- **Tab expansion** — `\t` is expanded to spaces aligned at configurable tab stops (display only; stored as literal `\t`)
- **Syntax highlighting** — rule-based colorization driven by external INI files; ships with C/C++, Python, Markdown, JavaScript, HTML, CSS, JSON, Batch; fully customizable without recompiling
- **Manual language switch** — View > Set Language... opens a scrollable list to choose syntax highlighting for the current file (or disable it)
- **TUI file/directory browser** — Open and Save show a full directory browser dialog; navigate with keyboard, mouse, scroll wheel, and double-click; press `/`, `\`, `~`, `Ctrl+G` or click the path bar to jump directly to any path
- **Configurable extension→syntax mapping** — add `syntax_ext_<ext>=<ini>` entries in `config.ini` to map any file extension to any syntax INI, no recompile needed
- **Default filename** — status bar shows `Untitle` when no file is open

---

## Code Stats

| Category | Files | Lines |
|----------|-------|-------|
| C source (`src/`, `tests/`) | 31 | ~7,500 |
| Headers (`include/`, `src/platform/`) | 17 | ~1,050 |
| Syntax rules (INI) | 8 | ~440 |
| **Total** | 56 | **~9,000** |

*Excludes `cmake-build/`. ~303 automated unit tests.*

---

## Build (CMake)

### Requirements

- CMake 3.16+
- Visual Studio 2026 Enterprise (Windows) or GCC/Clang (Linux/macOS)

### Windows — quick build

```bat
cmake_build.bat          # configure + build (Release)
cmake_build.bat test     # build + run all ~303 unit tests
cmake_build.bat clean    # remove cmake-build/
cmake_build.bat rebuild  # clean + rebuild
```

Output: `cmake-build\bin\Release\edit.exe`

### Manual CMake — Windows (Visual Studio 2026 not auto-detected)

```bat
cmake -S . -B cmake-build -G "Visual Studio 18 2026" -A x64 ^
      -DCMAKE_GENERATOR_INSTANCE="C:\Program Files\Microsoft Visual Studio\18\Enterprise"
cmake --build cmake-build --config Release
cmake --build cmake-build --config Release --target test_runner
cmake-build\bin\Release\test_runner.exe
```

### Manual CMake — cross-platform

```bash
cmake -S . -B cmake-build
cmake --build cmake-build --config Release
cmake --build cmake-build --config Release --target test_runner
cmake-build/bin/Release/test_runner
```

---

## Usage

```bat
cmake-build\bin\Release\edit.exe [filename]
```

If `filename` does not exist, a new file is created with that name on first save.

When opening a file with a recognized extension (`.c`, `.cpp`, `.h`, `.py`, `.md`, etc.) the editor automatically applies syntax highlighting. Highlighting rules are loaded from the `syntax/` folder next to the executable — copy or edit the INI files to customize colors.

---

## Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| New | Ctrl+N |
| Open | Ctrl+O |
| Save | Ctrl+S or F2 |
| Save As | Ctrl+Shift+S |
| Exit | Alt+X |
| Undo | Ctrl+Z |
| Redo | Ctrl+Y |
| Cut | Ctrl+X |
| Copy | Ctrl+C |
| Paste | Ctrl+V |
| Select All | Ctrl+A |
| Find | Ctrl+F |
| Find Next | F3 |
| Find Prev | Shift+F3 |
| Replace | Ctrl+H |
| Toggle Wrap / HScroll | Ctrl+W |
| Horizontal scroll (HScroll mode) | Ctrl+← / Ctrl+→ |
| Toggle Insert / Overwrite | Insert |
| Help | F1 |

---

## Mouse Operations

| Gesture | Effect |
|---------|--------|
| Left-click | Move cursor to position |
| Double-click | Select word under cursor |
| Triple-click | Select entire line |
| Left-drag | Extend selection; auto-scrolls when dragged to top/bottom edge |
| Mouse wheel | Scroll viewport (cursor stays in place), 3 lines per tick |
| Right-click | Open context menu: Cut / Copy / Paste / Select All / Find Selected |
| Click menu bar item | Open dropdown |
| Click dropdown item | Execute command |
| Click vertical scrollbar track | Page up / page down |
| Drag vertical scrollbar thumb | Scroll to any position |
| Click horizontal scrollbar track | Pan left / right by half-page (HScroll mode only) |
| Drag horizontal scrollbar thumb | Pan to any horizontal position (HScroll mode only) |

### Screen layout

```
┌─ Menu bar ──────────────────────────────────────────┐  row 0
│ File(F)  Edit(E)  Search(S)  View(V)  Help(H)        │
├─ Edit area ──────────────────────────────────── │v│  │  rows 1…N-2
│ (file content, line numbers optional)           │e│  │
│                                                 │r│  │
│                                                 │t│  │
├─ H-scrollbar (WRAP_NONE mode only) ─────────────┤ ├──│  row N-2
│ [======#####=================================]  │+│  │
├─ Status bar ────────────────────────────────────┴─┤  │  row N-1
│  filename  Ln:1 Col:1 / 42 lines  INS  NWRP        │
└──────────────────────────────────────────────────┘
```

- **Vertical scrollbar** (rightmost column): `|` = track, `#` = thumb. Always visible.
- **Horizontal scrollbar** (second-to-last row, WRAP_NONE only): `=` = track, `#` = thumb. The `+` corner cell connects both scrollbars.

---

## Project Structure

```
TextEditor/
├── src/
│   ├── main.c            # event loop, keyboard & mouse dispatch, entry point
│   ├── editor.c          # cursor, selection, text editing, scroll, syntax ctx
│   ├── document.c        # line array, undo/redo, encoding fields
│   ├── display.c         # double-buffered Unicode cell screen layer
│   ├── viewport.c        # layout, tab-aware coord mapping, scrollbar, syntax render
│   ├── input.c           # normalized input events (keyboard + mouse + wheel)
│   ├── menu.c            # menu bar rendering, dropdown navigation, commands
│   ├── dialog.c          # modal dialogs: input, confirm, find, replace, help,
│   │                     #   right-click context menu
│   ├── search.c          # incremental find & replace (forward / backward)
│   ├── clipboard.c       # internal clipboard (platform-independent)
│   ├── config.c          # INI config file parser (config.ini)
│   ├── util.c            # memory, string, and path utilities
│   ├── utf8.c            # UTF-8 decode/encode, width calc, codepoint cursor move
│   ├── encoding.c        # GBK/UTF-8 auto-detection and conversion (Windows API)
│   ├── regex_simple.c    # lightweight NFA regex: . * + ? [] \d\w\s ^ $ \b
│   ├── syntax.c          # INI-driven syntax highlighting engine
│   └── platform/
│       ├── platform.h        # platform abstraction interface
│       ├── platform_win.c    # Windows Console API (Unicode, mouse wheel)
│       ├── platform_nix.c    # POSIX termios + ANSI escape sequences
│       └── platform_dos.c    # DOS INT 10h/16h (skeleton)
├── include/              # public header files (types.h, editor.h, …)
│   ├── types.h           # Cell, WrapMode, color/attr macros
│   ├── editor.h          # Editor struct (incl. SyntaxContext), full API
│   ├── document.h        # Document struct (incl. encoding), full API
│   ├── viewport.h        # Viewport, Selection, viewport_render (+ SyntaxContext)
│   ├── syntax.h          # SyntaxDef, SyntaxRule, SyntaxContext, highlight API
│   ├── encoding.h        # FileEncoding, encoding_detect, gbk↔utf8 convert
│   ├── regex_simple.h    # regex_match / regex_search
│   └── utf8.h            # utf8_decode/encode/cp_width/byte_to_col …
├── syntax/               # external syntax highlighting rules (INI format)
│   ├── c.ini             # C / C++ keywords, strings, comments, preprocessor
│   ├── python.ini        # Python keywords, strings, comments, decorators
│   ├── markdown.ini      # Markdown headings, bold/italic, code blocks, links
│   ├── javascript.ini    # JavaScript / TypeScript keywords, templates, decorators
│   ├── html.ini          # HTML / XML / SVG tags, attributes, entities, comments
│   ├── css.ini           # CSS / SCSS / Less selectors, properties, hex colors
│   ├── json.ini          # JSON keywords, strings; jsonc block/line comments
│   └── batch.ini         # Batch script keywords, echo, set, if, for
├── tests/                # ~303 unit tests, zero third-party
│   ├── test_runner.c     # test harness entry point + assertions
│   ├── test_document.c
│   ├── test_clipboard.c
│   ├── test_search.c
│   ├── test_viewport.c   # coord mapping + vertical & horizontal scrollbar hit-tests
│   ├── test_util.c
│   ├── test_config.c
│   ├── test_editor.c     # scroll_view, sel_word, sel_line, find_selected flow
│   ├── test_utf8.c       # UTF-8 decode/encode/width/cursor-move (18 cases)
│   ├── test_encoding.c   # encoding_detect + GBK↔UTF-8 roundtrip (10 cases)
│   ├── test_syntax.c     # highlight_line: keyword/comment/string/number/priority
│   └── mocks/
│       └── mock_display.c    # stub display layer for headless test builds
├── docs/                  # project documentation (specs, requirements, tests)
├── CMakeLists.txt
├── cmake_build.bat       # Windows quick-build / test / clean / rebuild
└── config.ini            # runtime configuration (wrap mode, line numbers, …)
```

---

## Configuration (`config.ini`)

| Key | Default | Description |
|-----|---------|-------------|
| `wrap_mode` | `0` | `0` = Char-wrap, `1` = Horizontal-scroll |
| `show_lineno` | `false` | Show line numbers in edit area |
| `tab_size` | `4` | Tab stop width (display only; stored as `\t` in the file) |
| `max_file_size` | `524288` | Maximum file size to open (bytes) |
| `syntax_ext_<ext>` | — | Map a file extension to a syntax INI, e.g. `syntax_ext_xyz=c.ini` |

---

## Syntax Highlighting

Rules are loaded from `syntax/<lang>.ini` files next to the executable. The editor matches the file extension automatically; if no match is found, highlighting is disabled.

### Rule types

| Type | Description |
|------|-------------|
| `keyword` | Word-boundary keyword list |
| `string` | Quoted string literal (configurable delimiter and escape) |
| `line_comment` | From prefix to end of line (e.g. `//`, `#`) |
| `block_comment` | Multi-line delimited span (e.g. `/* … */`) |
| `number` | Integer, hex (`0x…`), float, with optional suffixes |
| `line_start` | Entire line if it starts with a given prefix (e.g. `#include`) |
| `regex` | Custom pattern using the built-in lightweight regex engine |

### Supported regex features

`. * + ? [abc] [a-z] [^…] \d \w \s ^ $ \b`

### Example INI snippet

```ini
[meta]
name       = C
extensions = .c .cpp .h .hpp

[rule.keywords]
type     = keyword
color    = cyan
priority = 10
words    = int char void if else for while return struct typedef

[rule.line_comment]
type     = line_comment
color    = green
priority = 20
prefix   = //

[rule.block_comment]
type  = block_comment
color = green
start = /*
end   = */
```

Add or edit INI files in `syntax/` to support new languages — **no recompile needed**.

---

## Roadmap

- [x] Phase 1: editing, file I/O, menus, basic mouse (click + drag)
- [x] Phase 2: selection, clipboard, find/replace, undo/redo
- [x] Phase 3: wrap/scroll modes, line numbers, status bar
- [x] Phase 4: full mouse support (wheel, double/triple-click, right-click menu, scrollbars), horizontal scrollbar
- [x] Phase 5: UTF-8 / GBK multi-byte editing, wide-character display, codepoint-aware cursor
- [x] Phase 6: tab expansion, GBK auto-detect & convert, lightweight regex, INI-driven syntax highlighting (C / Python / Markdown)
- [x] Phase 7: status bar encoding / language indicator, syntax incremental refresh, manual language switch dialog, more syntax INI files (JavaScript, HTML, CSS, JSON, Batch)
- [x] Phase 8: TUI directory browser for Open/Save, path quick-jump (`/` `\` `~` `Ctrl+G`), configurable extension→syntax mapping, default filename `Untitle`

---

## Documentation

| Document | 中文 | English |
|----------|------|---------|
| Code structure | [代码文件结构](docs/代码文件结构.md) | [CODE_STRUCTURE](docs/CODE_STRUCTURE.md) |
| Functional requirements | [功能需求文档](docs/功能需求文档.md) | [FUNCTIONAL_REQUIREMENTS](docs/FUNCTIONAL_REQUIREMENTS.md) |
| Functional test spec | [功能测试规范](docs/功能测试规范.md) | [FUNCTIONAL_TEST_SPEC](docs/FUNCTIONAL_TEST_SPEC.md) |
