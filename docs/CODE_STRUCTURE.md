# DOS Text Editor - Code File Structure

[中文](代码文件结构.md) | English

> Project file organization and module responsibilities designed per the functional requirements document. **Zero third-party dependencies** across the entire project; only C standard library and platform-native APIs.

---

## 1. Directory Structure Overview

```
TextEditor/
├── src/                    # Source code
│   ├── main.c
│   ├── editor.c
│   ├── document.c
│   ├── display.c
│   ├── viewport.c
│   ├── input.c
│   ├── menu.c
│   ├── dialog.c            # Dialogs (including list selection dialog)
│   ├── search.c
│   ├── clipboard.c
│   ├── config.c
│   ├── util.c
│   ├── utf8.c             # UTF-8 encode/decode, codepoint width, coordinate conversion
│   ├── encoding.c         # GBK/UTF-8 encoding detection and conversion
│   ├── regex_simple.c     # Lightweight regex engine
│   ├── syntax.c           # Syntax highlighting engine
│   └── platform/
│       ├── platform.h
│       ├── platform_win.c
│       ├── platform_dos.c
│       └── platform_nix.c
├── include/                # Public headers
│   ├── editor.h
│   ├── document.h
│   ├── display.h
│   ├── viewport.h
│   ├── input.h
│   ├── menu.h
│   ├── dialog.h
│   ├── search.h
│   ├── clipboard.h
│   ├── config.h
│   ├── util.h
│   ├── types.h
│   ├── utf8.h
│   ├── encoding.h
│   ├── regex_simple.h
│   └── syntax.h
├── syntax/                 # Syntax highlighting config files (INI format)
│   ├── c.ini
│   ├── python.ini
│   ├── markdown.ini
│   ├── javascript.ini
│   ├── html.ini
│   ├── css.ini
│   ├── json.ini
│   └── batch.ini
├── tests/                  # Automated tests
│   ├── test_runner.c        # Test entry, assertion macros, result summary
│   ├── test_document.c
│   ├── test_clipboard.c
│   ├── test_search.c
│   ├── test_viewport.c
│   ├── test_util.c
│   ├── test_config.c
│   ├── test_editor.c
│   ├── test_utf8.c
│   ├── test_encoding.c
│   ├── test_syntax.c
│   ├── mocks/               # Mock implementations (for isolating platform dependencies)
│   │   └── mock_display.c
│   └── fixtures/           # Test data files
│       ├── simple.txt      # Multi-line text
│       ├── long_line.txt   # Extra-long single line
│       ├── tmp_config.ini  # Temporary config for tests
│       └── tmp_test.txt    # Temporary file for tests
├── docs/                   # Project documentation (specs, requirements, tests)
│   ├── 代码文件结构.md
│   ├── CODE_STRUCTURE.md
│   ├── 功能需求文档.md
│   ├── FUNCTIONAL_REQUIREMENTS.md
│   ├── 功能测试规范.md
│   └── FUNCTIONAL_TEST_SPEC.md
├── CMakeLists.txt          # CMake build configuration
├── cmake_build.bat         # Windows quick-build script
├── config.ini              # Default config file
└── README.md
```

---

## 2. Module Design Goals

### 2.1 `main.c`

**Responsibility:** Program entry, initialization flow, main event loop, cleanup on exit

| Goal | Corresponding requirement |
|------|---------------------------|
| Startup initialization | Call each module's `*_init()`, load config, detect terminal size |
| Main loop | Wait for input events → dispatch to editor/menu → call display_flush to refresh |
| Exit flow | Prompt for unsaved changes → save config → call each module's `*_exit()` |
| Command-line args | Support `edit filename` to open specified file at startup |

**Dependencies:** editor, display, input, config, platform

---

### 2.2 `document.c` / `document.h`

**Responsibility:** Document data model, text storage, line editing, undo/redo

| Goal | Corresponding requirement |
|------|---------------------------|
| Line storage structure | `struct Line { char *text; int len; int cap; }` or `char **lines` dynamic array |
| Insert character | Insert at (row, col), memmove + realloc |
| Delete character | Backspace/Delete to remove preceding/following character |
| Line break | On Enter, split current line and insert new line |
| Merge lines | Backspace at line start merges with previous line |
| Get line/col | `document_get_line()`, `document_get_char()`, `document_line_count()` |
| Undo stack | Operation record `{type, row, col, old_str, new_str}`, undo/redo |
| Modified flag | `document_modified` for save prompt |
| Max line/col limit | Configurable document size limit |

**Extended (Phase 5/6):**
- Encoding info: `encoding` (UTF-8/GBK), `has_utf8_bom`, detect on load, restore on save
- UTF-8 codepoint delete: delete/backspace by codepoint instead of byte

**Interface example:**
```c
Document* document_new(void);
void document_free(Document *doc);
int document_load(Document *doc, const char *path);
int document_save(Document *doc, const char *path);
void document_insert_char(Document *doc, int row, int col, char c);
void document_delete_char(Document *doc, int row, int col, int backward);
void document_break_line(Document *doc, int row, int col);
void document_merge_line(Document *doc, int row);
int document_undo(Document *doc);
int document_redo(Document *doc);
```

---

### 2.3 `display.c` / `display.h`

**Responsibility:** Display abstraction layer, screen buffer, double buffering, platform interface encapsulation

| Goal | Corresponding requirement |
|------|---------------------------|
| Platform-independent interface | display_init, display_exit, display_get_size, display_put_cell, etc. (see requirement 5.7) |
| Screen buffer | Cell 2D array (char + attribute), current frame + back buffer |
| Double buffering | Write to back buffer, diff and output changed cells in display_flush |
| Dirty-region optimization | dirty_rect tracking, redraw only changed regions |
| Character attributes | ATTR_NORMAL, ATTR_SELECT, ATTR_LINENO, etc. |
| Cursor | display_set_cursor, display_show_cursor |

**Excluded:** Layout calculation, coordinate conversion, edit-area drawing logic (handled by viewport)

---

### 2.4 `viewport.c` / `viewport.h`

**Responsibility:** Viewport management, word-wrap/scroll mode, logic↔screen coordinate conversion, edit-area rendering

| Goal | Corresponding requirement |
|------|---------------------------|
| Viewport state | view_top_row, view_top_display_row, view_left_col |
| Display mode | WrapMode (WRAP_WORD / WRAP_NONE), word_wrap configurable |
| Logic→screen | logic_to_screen(row, col, &y, &x) |
| Screen→logic | screen_to_logic(y, x, &row, &col) |
| Line-wrap cache | display_line_offset[], logical line to cumulative display line offset |
| Edit area column width | edit_cols = screen_width - line_num_width - scrollbar_width |
| Render edit area | Get lines from document per mode, display_put_cell per char, handle selection highlight |
| Line number column | Show line number for first display line, blank or repeated for wrapped continuation |
| Scrollbar | Vertical bar, horizontal bar in scroll mode |
| Ensure cursor visible | Adjust view_* after cursor move so cursor stays in viewport |
| Mode toggle | viewport_toggle_wrap(), reset viewport, invalidate cache |
| UTF-8-aware rendering | Render by codepoint width (ASCII=1, CJK=2) |
| Tab expansion | `\t` expanded to spaces by tab_size for display |
| Syntax highlight integration | Accept SyntaxContext, combine selection/syntax/default attributes |

**Dependencies:** document, display, config, syntax

---

### 2.5 `editor.c` / `editor.h`

**Responsibility:** Edit logic, cursor, selection, clipboard operations, command dispatch

| Goal | Corresponding requirement |
|------|---------------------------|
| Cursor position | cursor_row, cursor_col, boundary clamp |
| Selection range | selection_start, selection_end (row,col), normalized start<=end |
| Keyboard navigation | Arrow keys (including DOS-style jump to line start/end), Home/End, Ctrl+Home/End, Page Up/Down, Ctrl+←/→ (horizontal scroll viewport in WRAP_NONE mode) |
| Extend selection | Shift+arrow to extend selection_end |
| Insert/overwrite mode | insert_mode, Insert key toggles |
| Delete selection | Delete/Backspace with selection deletes selection |
| Cut/copy/paste | Call clipboard, then document delete/insert |
| Select all | selection set to (0,0) to (last_row, last_col) |
| Document interaction | All edits via document interface |
| Viewport interaction | Call viewport_ensure_cursor_visible after cursor move |
| Syntax highlight context | SyntaxContext, manages current file syntax definition and block comment state (Phase 6) |
| Auto syntax match | Load corresponding INI syntax config by extension on open |
| Syntax incremental rebuild | Incremental rebuild of block comment state from cursor line after each edit (Phase 7) |

**Dependencies:** document, viewport, clipboard, syntax

---

### 2.6 `input.c` / `input.h`

**Responsibility:** Keyboard and mouse input, event normalization

| Goal | Corresponding requirement |
|------|---------------------------|
| Keyboard events | Read scan code/virtual key, support arrows, function keys, Ctrl/Alt/Shift combos |
| Mouse events | Left/right press/release, move, get (x, y) |
| Event structure | `typedef struct { int type; int key; int mod; int mx, my; } InputEvent` |
| Non-blocking read | input_poll() or input_wait_event(), platform-dependent |
| DOS mouse | INT 33h init, state read |
| Windows console | ReadConsoleInput |
| Linux | POSIX termios raw mode, blocking `read()`; mouse via ioctl/ANSI protocol (no ncurses) |

**Interface example:**
```c
void input_init(void);
void input_exit(void);
int input_poll(InputEvent *ev);
void input_translate_screen_to_logic(int sx, int sy, int *row, int *col);  // calls viewport
```

---

### 2.7 `menu.c` / `menu.h`

**Responsibility:** Menu bar, dropdown menus, dialogs, menu item and command binding

| Goal | Corresponding requirement |
|------|---------------------------|
| Menu bar | File / Edit / Search / View / Help horizontal items, Alt+letter to activate |
| Dropdown | Expand to show sub-items, arrow keys to select, Enter to execute |
| Menu drawing | Highlight current item (reverse video), display_put_cell per character |
| Menu click | Check if (mx, my) in menu item rect, execute corresponding command |
| Dialogs | Find, replace, open, save as, confirm exit popups |
| Input field | Text input in dialog, support Backspace, Enter to confirm, Esc to cancel |
| Error prompt | Popup for "Cannot open file" etc. |
| Save confirm | "Save changes? Yes/No/Cancel" |
| Status bar | File name, line/col, mode, wrap, encoding, language |
| View > Set Language... | Popup list selection dialog to switch syntax highlighting (Phase 7) |

**Command mapping:** Menu/shortcut → call editor, document, search interfaces

---

### 2.8 `search.c` / `search.h`

**Responsibility:** Find, replace, find options

| Goal | Corresponding requirement |
|------|---------------------------|
| Find | Search for str in document starting from (row, col), return match position |
| Find next | Continue search after current match, wrap to document start |
| Find previous | Search backward |
| Replace | Replace current match or all matches |
| Find options | case_sensitive, whole_word (optional) |
| Highlight match | Optional: temporarily highlight current match in edit area |

**Interface example:**
```c
void search_init(const char *find_str, const char *replace_str, int options);
/* from_row/from_col: search start (input); out_row/out_col: match position (output); returns match length, -1=not found */
int search_next(Document *doc, int from_row, int from_col, int *out_row, int *out_col);
int search_prev(Document *doc, int from_row, int from_col, int *out_row, int *out_col);
/* match_len: current match length in chars, used to determine how many chars to delete before inserting replacement */
int search_replace_current(Document *doc, int row, int col, int match_len);
int search_replace_all(Document *doc);
```

---

### 2.9 `clipboard.c` / `clipboard.h`

**Responsibility:** Internal clipboard (no system clipboard, DOS-compatible)

| Goal | Corresponding requirement |
|------|---------------------------|
| Storage | `char *clipboard` dynamic array, may contain `\n` |
| Copy | clipboard_set(text, len) |
| Paste | clipboard_get() returns content, caller inserts into document |
| Clear | On exit or when copying new content |
| Multiline | Split by `\n` when pasting |

**Interface example:**
```c
void clipboard_set(const char *text, int len);
const char* clipboard_get(int *out_len);
int clipboard_is_empty(void);
```

---

### 2.10 `config.c` / `config.h`

**Responsibility:** Config storage, defaults, load on start, save on exit

| Goal | Corresponding requirement |
|------|---------------------------|
| word_wrap | Word-wrap toggle, configurable default |
| Recent files | recent_files[8], path list (optional) |
| Line numbers | show_line_numbers |
| Tab width | tab_size, default 4 |
| Encoding | Default encoding (future) |
| Config file | Load by priority: ① user config (`~/.editrc` or `%APPDATA%\edit.ini`) → ② program dir `config.ini` (defaults); save only to user config on exit; use built-in defaults if both missing |
| Max file size | max_file_size, etc. |

**Interface example:**
```c
void config_load(const char *path);
void config_save(const char *path);
bool config_get_word_wrap(void);
void config_set_word_wrap(bool v);
void config_add_recent(const char *path);
const char** config_get_recent(int *count);
```

---

### 2.11 `util.c` / `util.h`

**Responsibility:** General utilities, strings, memory

| Goal | Purpose |
|------|---------|
| Strings | safe_strdup, trim, extended sprintf, etc. |
| Paths | Get current dir, path join, file existence check |
| Encoding | Simple ANSI/UTF-8 detection (future) |
| Memory | Wrap malloc/realloc, unified handling on failure |
| Executable dir | get_exe_dir(), to locate config.ini, syntax/ directory |

---

### 2.12 `utf8.c` / `utf8.h` (Phase 5)

**Responsibility:** UTF-8 encode/decode, codepoint width, byte↔display column conversion, cursor advance

| Goal | Corresponding requirement |
|------|---------------------------|
| utf8_seq_len | Determine UTF-8 sequence length from first byte |
| utf8_decode | Decode UTF-8 sequence to Unicode codepoint |
| utf8_encode | Encode codepoint to UTF-8 bytes |
| utf8_cp_width | ASCII=1, CJK/fullwidth=2 |
| utf8_byte_to_col / utf8_col_to_byte | In-line byte offset↔display column conversion |
| utf8_next_char / utf8_prev_char | Advance by codepoint, for cursor movement |

---

### 2.13 `encoding.c` / `encoding.h` (Phase 6B)

**Responsibility:** File encoding detection and GBK↔UTF-8 conversion

| Goal | Corresponding requirement |
|------|---------------------------|
| encoding_detect | BOM → UTF-8; valid UTF-8 → UTF-8; otherwise → GBK |
| gbk_to_utf8_buf | Windows CP936→UTF-8 |
| utf8_to_gbk_buf | Windows UTF-8→CP936 |

---

### 2.14 `regex_simple.c` / `regex_simple.h` (Phase 6C1)

**Responsibility:** Lightweight regex engine for syntax highlighting rules

| Supported | Description |
|-----------|--------------|
| `. * + ?` | Wildcard, quantifiers |
| `\d \w \s` | Digit, word char, whitespace |
| `[abc] [^x]` | Character class, negated class |
| `^ $ \b` | Line start, line end, word boundary |

---

### 2.15 `syntax.c` / `syntax.h` (Phase 6C2)

**Responsibility:** Syntax highlighting engine, INI config parsing, block comment state

| Goal | Corresponding requirement |
|------|---------------------------|
| syntax_load | Load syntax definition from INI |
| syntax_match_ext | Match language by file extension |
| syntax_list_languages | Scan syntax/ directory to list languages (Phase 7) |
| syntax_rebuild_state | Incremental rebuild of block comment state across lines |
| syntax_highlight_line | Compute highlight attributes for single line |
| syntax_ctx_init / syntax_ctx_free | Syntax context lifecycle |

---

### 2.16 `dialog.c` / `dialog.h`

**Responsibility:** Modal dialogs, input fields, confirmation, list selection

| Goal | Corresponding requirement |
|------|---------------------------|
| dialog_input | Single-line text input |
| dialog_confirm | Yes/No confirmation |
| dialog_save_prompt | Save / Don't save / Cancel |
| dialog_error | Error message |
| dialog_find / dialog_replace | Find, replace input |
| dialog_help | F1 shortcut help |
| dialog_context_menu | Right-click menu |
| dialog_list_select | Scrollable list selection (View > Set Language...) |

---

### 2.17 `platform/` Platform Implementation

**Responsibility:** Map display, input abstractions to platform APIs

#### `platform.h`

Defines platform abstraction (display_*, input_* declarations) and `#ifdef` to select platform.

#### `platform_win.c` (Windows console)

| Goal | Corresponding requirement |
|------|---------------------------|
| display | SetConsoleCursorPosition, WriteConsoleOutput, SetConsoleTextAttribute |
| Keyboard | ReadConsoleInput for KEY_EVENT |
| Mouse | ReadConsoleInput for MOUSE_EVENT, ENABLE_MOUSE_INPUT |
| Size | GetConsoleScreenBufferInfo |

#### `platform_dos.c` (FreeDOS / MS-DOS)

| Goal | Corresponding requirement |
|------|---------------------------|
| display | INT 10h set mode, write char, set cursor; direct B800:0 video memory write |
| Keyboard | INT 16h read scan code |
| Mouse | INT 33h init, get position and button state |
| Files | DOS INT 21h or standard C library (if available) |

#### `platform_nix.c` (Linux / Unix terminal)

| Goal | Corresponding requirement |
|------|---------------------------|
| display | termios raw mode, `write()` for ANSI escapes (cursor, color), no ncurses |
| Keyboard | `read()` from stdin for key codes |
| Mouse | Send `\033[?1000h` for ANSI X10 mouse protocol (terminal-dependent); fallback to keyboard-only if unsupported |
| Size | ioctl TIOCGWINSZ or environment variable |

---

### 2.18 `include/types.h`

**Responsibility:** Global type definitions, constants

| Content | Description |
|---------|-------------|
| Cell, ScreenBuffer | Display-related structures |
| WrapMode | WRAP_CHAR (char wrap), WRAP_NONE (scroll) |
| InputEventType | KEY_DOWN, MOUSE_CLICK, etc. |
| Attribute constants | ATTR_NORMAL, ATTR_SELECT, etc. |

---

## 3. Module Dependencies

```
                         main.c
                            |
    +----------------------+----------------------+
    |                      |                      |
 editor.c              menu.c                config.c
    |                      |                      |
    +-- document.c         |                      |
    +-- viewport.c ---------+                      |
    |        |              |                      |
    |   display.c      dialog.c                    |
    |        |              |                      |
    +-- clipboard.c   +-- search.c                |
    +-- syntax.c      |        |                  |
    |        |         |   document.c              |
    +-- utf8.c         +--------+                  |
    +-- encoding.c            |                    |
    +-- regex_simple.c  util.c                    |
         |                   |                    |
    platform/              (win|dos|nix)
```

---

## 4. Compilation Units and Linking

| Target | Build command example |
|--------|------------------------|
| Windows | `cl /Fe:edit.exe main.c ... platform/platform_win.c` (no third-party libs) |
| DOS | `wcc -fe=edit.exe ...` or `tcc -o edit.exe ...` (no third-party libs) |
| Linux | `gcc -o edit main.c ... platform/platform_nix.c` (no -lncurses etc., only stdlib+POSIX) |

Use `#define PLATFORM_WIN` etc. in platform.h or conditional compilation to include platform_*.c.

### 4.1 CMake Build (Recommended)

Project root has `CMakeLists.txt` for main program and unit tests:

```bash
mkdir build && cd build
cmake ..
cmake --build .              # Build edit and test_runner
./bin/test_runner            # or ctest / cmake --build . --target test
```

| Target | Description |
|--------|-------------|
| **edit** | Main program: main.c, display, input, menu, dialog, platform, core libs |
| **test_runner** | Unit tests: 10 test_* modules, mock_display, core libs (no display/input/menu) |

---

## 5. Automated Tests

### 5.1 Test Directory and Files

| File | Responsibility |
|------|----------------|
| **test_runner.c** | Defines `ASSERT_EQ`, `ASSERT_STR_EQ` macros; pass/fail counts; `main()` calls each `test_*_run()` |
| **test_document.c** | Tests document new/load/save, insert/delete, break/merge, undo/redo |
| **test_clipboard.c** | Tests clipboard set/get, multiline, empty |
| **test_search.c** | Tests search find, replace, replace all, edge cases |
| **test_viewport.c** | Tests logic_to_screen, screen_to_logic, wrap/scroll conversion, scrollbar hit test |
| **test_util.c** | Tests util string, path, memory wrapper |
| **test_config.c** | Tests config load/save, defaults, word_wrap, etc. |
| **test_editor.c** | Tests editor_scroll_view, sel_word, sel_line, find_selected |
| **test_utf8.c** | Tests UTF-8 encode/decode, codepoint width, coordinate conversion, cursor advance |
| **test_encoding.c** | Tests encoding_detect, GBK↔UTF-8 conversion (Windows) |
| **test_syntax.c** | Tests syntax_highlight_line, syntax_list_languages edge cases |
| **mocks/mock_display.c** | Implements display_* as no-ops so viewport/editor tests avoid real terminal |
| **fixtures/** | simple.txt, long_line.txt, tmp_config.ini, tmp_test.txt |

### 5.2 Testable Modules and Dependencies

| Module | Direct unit test | Mock needed | Notes |
|--------|------------------|-------------|-------|
| **document** | ✓ | None | Pure logic, stdlib/filesystem only |
| **clipboard** | ✓ | None | No external deps |
| **search** | ✓ | document | Construct Document, or use with tested document |
| **util** | ✓ | None | Pure functions |
| **config** | ✓ | None | Temp config file or in-memory config |
| **viewport** | ✓ | document, display | mock_display avoids terminal init |
| **editor** | ✓ | document, viewport, clipboard | Test scroll_view, sel_word, sel_line, find_selected |
| **utf8** | ✓ | None | Pure functions, UTF-8 encode/decode, width, coordinates |
| **encoding** | ✓ | None | Encoding detect, GBK↔UTF-8 (Windows needs API) |
| **syntax** | ✓ | document | Test highlight_line, list_languages edge cases |
| **display** | ✗ | - | Platform-dependent, manual/integration test |
| **input** | ✗ | - | Platform-dependent |
| **menu** | ✗ | - | Depends on display, event loop |
| **dialog** | ✗ | - | Depends on display, input; manual test |

### 5.3 Test Framework and Assertions

**Minimal self-implementation**, **zero third-party dependencies** (no Unity, Check, etc.), only C stdlib (stdio, stdlib, string), works on all targets including DOS:

```c
// test_runner.c core
/* On assertion failure, record error and return immediately (later assertions in same function are skipped) */
#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "FAIL %s:%d: %d != %d\n", __FILE__, __LINE__, (int)(a), (int)(b)); \
        fail_count++; return; \
    } else { pass_count++; } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        fprintf(stderr, "FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
        fail_count++; return; \
    } else { pass_count++; } \
} while(0)

static int pass_count, fail_count;

void test_document_run(void);
void test_clipboard_run(void);
void test_search_run(void);
void test_viewport_run(void);
void test_util_run(void);
void test_config_run(void);
void test_editor_run(void);
void test_utf8_run(void);
void test_encoding_run(void);
void test_syntax_run(void);

int main(void) {
    test_document_run();
    test_clipboard_run();
    test_search_run();
    test_viewport_run();
    test_util_run();
    test_config_run();
    test_editor_run();
    test_utf8_run();
    test_encoding_run();
    test_syntax_run();
    printf("%d passed, %d failed\n", pass_count, fail_count);
    return fail_count > 0 ? 1 : 0;
}
```

### 5.4 Example Test Cases per Module

#### document

| Case | Action | Expected |
|------|--------|----------|
| new | document_new() | 1 empty line, line_count=1 |
| insert | Insert "ab" at (0,0) | Line content "ab", len=2 |
| delete_back | Backspace at (0,1) | Line becomes "b" |
| delete_fwd | Delete at (0,0) | Line becomes "" |
| break_line | Line break at "a|bc" | Two lines "a", "bc" |
| merge_line | Backspace at line 2 start | Two lines merge to "abc" |
| load_save | Write "hello\n" to file, load and read | Content matches |
| undo | Insert then undo | Back to pre-insert |
| redo | Redo after undo | Back to post-insert |

#### clipboard

| Case | Action | Expected |
|------|--------|----------|
| set_get | set("x",1), get() | Returns "x" |
| multiline | set("a\nb",3), get() | Returns "a\nb" |
| empty | get() without prior set | Returns empty or NULL, is_empty=1 |

#### search

| Case | Action | Expected |
|------|--------|----------|
| find | Doc "foo bar foo", find "foo" | First at (0,0) |
| find_next | Continue find_next | (0,8) |
| replace | Replace current "foo"→"X" | Doc becomes "X bar foo" |
| replace_all | Replace all | Doc becomes "X bar X" |
| not_found | Find "zzz" | Returns -1 or failure |

#### viewport

| Case | Action | Expected |
|------|--------|----------|
| wrap_logic_to_screen | edit_cols=10, line "12345678901", (0,5)→(y,x) | Correct per wrap rules |
| wrap_screen_to_logic | Reverse | (row,col) matches input |
| nowrap_logic_to_screen | view_left_col=5, (0,7)→(y,x) | screen_col=2 |
| nowrap_screen_to_logic | Reverse | doc_col=7 |

### 5.5 Mock Implementation Notes

**mock_display.c:**

```c
// No platform API calls, only maintain memory buffer or no-op
void display_init(void) { /* noop */ }
void display_put_cell(int y, int x, char c, uint8_t attr) { /* optional: write to buffer for assertions */ }
void display_set_cursor(int y, int x) { /* noop */ }
void display_flush(void) { /* noop */ }
// ... other interfaces
```

Use `-DTEST` or `-I. -DMOCK_DISPLAY` when building tests; link mock_display.c instead of platform_win.c.

### 5.6 Build and Run

**CMake (recommended):**

```bash
mkdir build && cd build && cmake .. && cmake --build .
ctest   # or ./bin/test_runner
```

**Makefile example:**

```makefile
TEST_SRCS = tests/test_runner.c tests/test_document.c tests/test_clipboard.c \
            tests/test_search.c tests/test_viewport.c tests/test_util.c tests/test_config.c \
            tests/test_editor.c tests/test_utf8.c tests/test_encoding.c tests/test_syntax.c
MOCK_SRCS = tests/mocks/mock_display.c
CORE_SRCS = src/document.c src/clipboard.c src/search.c src/viewport.c src/util.c src/config.c \
            src/editor.c src/utf8.c src/encoding.c src/regex_simple.c src/syntax.c

test: $(TEST_SRCS) $(MOCK_SRCS) $(CORE_SRCS)
	$(CC) -DTEST -I include -I src -I tests -o test_runner $^
	./test_runner
```

- Test build has **zero third-party dependencies**, links only project sources and C stdlib, no -lncurses, -lcheck, etc.

| Platform | Command |
|----------|---------|
| Any (CMake) | `cmake --build build --target test` or `ctest -C build` |
| Linux/macOS | `make test` |
| Windows | `nmake test` or `make -f Makefile.win test` |
| DOS | Build test_runner with OpenWatcom/Turbo C etc., no third-party libs |

### 5.7 Mapping to Functional Test Spec

| Auto test coverage | Functional test spec section |
|--------------------|-----------------------------|
| test_document | 2.2 Open, 2.3 Save, 3.x Edit, 3.5/3.6 Undo/Redo |
| test_clipboard | 5.3–5.5 Cut/Copy/Paste |
| test_search | 6.1–6.5 Find/Replace |
| test_viewport | 7.4 Word wrap, 7.5 Horizontal scroll, scrollbar hit test |
| test_editor | scroll_view, sel_word, sel_line, find_selected |
| test_config | Config load/save, word_wrap, tab_size, etc. |
| test_util | Utility correctness |
| test_utf8 | UTF-8 encode/decode, width, coordinates, cursor advance |
| test_encoding | Encoding detect, GBK↔UTF-8 conversion |
| test_syntax | Syntax highlight rules, syntax_list_languages edge cases |

**Currently ~303 automated cases.** Integration, UI, mouse, performance tests are manual per the Functional Test Specification.

---

*Document version: v1.5*  
*Matches functional requirements v1.5*  
*Updates: Added utf8.c, encoding.c, regex_simple.c, syntax.c, dialog.c and headers, syntax/ directory (8 language INI files), test_utf8.c, test_encoding.c, test_syntax.c, CMakeLists.txt build config*
