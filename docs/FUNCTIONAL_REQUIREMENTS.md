# DOS Text Editor - Functional Requirements Document

> Functional specification for a character-mode text editor similar to MS-DOS Edit

---

[中文](功能需求文档.md) | English

---

## 1. Project Overview

Develop a plain-text editor that runs in a DOS/terminal environment, using a character interface (not graphical). It supports both keyboard and mouse for editing. Target users are those who need to quickly edit configuration files and scripts in a command-line environment.

---

## 2. Core Functional Requirements

### 2.1 File Operations

| Feature | Description | Priority |
|---------|-------------|----------|
| **New file** | Create a new blank document; default name is `Untitle` (no extension) | High |
| **Open file** | Show a TUI directory browser to navigate and select a file; direct path input supported | High |
| **Save file** | Save to existing path directly; if no path, show directory browser to pick location and filename | High |
| **Save as** | Show directory browser to pick directory, then input or select filename | Medium |
| **Exit editor** | Prompt for unsaved changes before exit | High |
| **Recently opened files** | Optional: record and quickly open recently edited files | Low |
| **File browser path jump** | In file browser, press `/`, `\`, `~`, `Ctrl+G` or click the path bar to type a target path directly | Medium |
| **Extension→syntax mapping** | Add `syntax_ext_<ext>=<ini>` in `config.ini` to map any extension to any syntax INI | Low |

**Constraints:**
- Support common encodings (e.g., ANSI/ASCII, UTF-8, GBK)
- **Implemented**: UTF-8 multibyte character editing (Phase 5), GBK encoding auto-detection and conversion (Phase 6), internal storage uses UTF-8 uniformly
- Recommended max file size: 64KB–512KB (configurable via `max_file_size`)
- Loading prompt when opening large files

---

### 2.2 Text Editing

| Feature | Description | Priority |
|---------|-------------|----------|
| **Insert character** | Type character at cursor, existing content shifts right | High |
| **Delete character** | Backspace deletes char before cursor, Delete deletes char after cursor | High |
| **Line break** | Enter inserts newline at cursor | High |
| **Insert/Overwrite mode** | Insert key toggles | Medium |
| **Undo** | Ctrl+Z undoes last operation (multi-level) | Medium |
| **Redo** | Ctrl+Y or Ctrl+Shift+Z | Low |

---

### 2.3 Cursor and Navigation

| Feature | Description | Priority |
|---------|-------------|----------|
| **Arrow keys** | ↑↓←→ move cursor in text; **← at line start jumps to prev line end, → at line end jumps to next line start** (DOS standard; cursor can wrap across lines) | High |
| **Line start/end** | Home to line start, End to line end | High |
| **Document start/end** | Ctrl+Home to doc start, Ctrl+End to doc end | High |
| **Page up/down** | Page Up / Page Down for vertical scrolling | High |
| **Horizontal scroll** | Ctrl+← / Ctrl+→ move horizontal viewport left/right in scroll mode (effective in WRAP_NONE; not in WRAP_CHAR) | High |

---

### 2.4 Selection and Clipboard

| Feature | Description | Priority |
|---------|-------------|----------|
| **Text selection** | Shift+arrow keys to select text (block or line selection) | High |
| **Select all** | Ctrl+A selects entire document | High |
| **Cut** | Ctrl+X cut selection to clipboard | High |
| **Copy** | Ctrl+C copy selection to clipboard | High |
| **Paste** | Ctrl+V paste clipboard at cursor | High |
| **Delete selection** | Delete/Backspace deletes selection | High |

---

### 2.5 Find and Replace

| Feature | Description | Priority |
|---------|-------------|----------|
| **Find** | Find a string in the document | High |
| **Find next** | F3 or Ctrl+G find next match | High |
| **Find previous** | Shift+F3 find previous match | Medium |
| **Replace** | Replace matched string with specified content | Medium |
| **Replace all** | Replace all matches at once | Medium |
| **Find options** | Case sensitive, whole word match (optional) | Low |

---

### 2.6 Display and Interface

| Feature | Description | Priority |
|---------|-------------|----------|
| **Menu bar** | Top shows File, Edit, Search, View, Help menus | High |
| **Edit area** | Shows text with scrolling | High |
| **Status bar** | Shows current row, column, total lines, Insert/Overwrite mode, file encoding (UTF-8/GBK), syntax highlighting language | High |
| **Scrollbar** | Optional: simple scroll indicator in char mode | Medium |
| **Line numbers** | Optional: show line numbers on left | Medium |
| **Word wrap** | Long lines wrap; can be toggled | High |
| **Horizontal scroll** | When wrap off, support horizontal scrolling for long lines | High |
| **Tab display** | Tab char `\t` displayed as spaces per config (does not change file content) | Medium |
| **Syntax highlighting** | Color keywords, strings, comments by file type; supports external INI config | Medium |
| **Manual syntax language** | View > Set Language... to pick or disable syntax highlighting | Low |
| **Color scheme** | Optional: different foreground/background themes | Low |

---

### 2.7 Help and Prompts

| Feature | Description | Priority |
|---------|-------------|----------|
| **Shortcut help** | F1 shows common shortcuts | High |
| **Menu navigation** | Alt+letter opens corresponding menu | High |
| **Save prompt** | If unsaved on exit, prompt user to confirm | High |
| **Error messages** | Friendly prompts for open/save failures etc. | High |

---

### 2.8 Mouse Support

| Feature | Description | Priority |
|---------|-------------|----------|
| **Click to position** | Left-click in edit area moves cursor to clicked row/col | High |
| **Drag selection** | Drag after left-press to select from start to current cursor | High |
| **Menu click** | Left-click menu items to run commands | High |
| **Scrollbar** | Click/drag scrollbar area to scroll (if present) | Medium |
| **Double-click word** | Double-click selects current word | Low |
| **Triple-click line** | Triple-click selects whole line | Low |
| **Right-click menu** | Right-click shows context menu (cut/copy/paste/select all/find selection) | Low |
| **Mouse wheel** | Scroll wheel scrolls document (cursor position unchanged) | Medium |

**Constraints:**
- In DOS/terminal, mouse input via BIOS interrupt or driver (e.g., INT 33h)
- In Windows CMD, mouse events via Windows Console API; on Linux terminal via ANSI X10 mouse escape sequences (`\033[?1000h`). Depends on terminal support (xterm, VTE, etc.); may not work in tmux/screen/SSH or unsupported terminals—then **fall back to keyboard-only** (per 2.8 “fully keyboard-operable without mouse”)
- Mouse cursor in char mode can be a block or custom character
- Editor must be fully operable by keyboard when no mouse

---

## 3. Non-Functional Requirements

### 3.1 Runtime Environment

- **Platforms**: DOS (e.g., FreeDOS), Windows Command Prompt, Linux/Unix terminal
- **Language**: **C or C++** only, no other languages
- **Dependencies**: **Zero third-party**; use only:
  - **C standard library**: stdio, stdlib, string, ctype, stdbool, etc.
  - **Platform APIs**: DOS INT 10h/16h/21h/33h, Windows Console API, POSIX termios/ioctl
  - No ncurses, Qt, SDL, Check, Unity, etc.; tests use self-implemented assertions
- **Ctrl+C handling**: On Windows, use `SetConsoleCtrlHandler` to block default SIGINT so Ctrl+C can be used for “copy”; on Linux/DOS, set `c_lflag &= ~ISIG` in termios init so Ctrl+C does not terminate the process

### 3.2 Performance

- Startup: &lt; 2 seconds
- Open 100KB file: &lt; 1 second
- Keystroke response: no noticeable delay

### 3.3 Compatibility

- Support 80×25, 80×50 and other common terminal sizes
- Correctly handle window resize (e.g., terminal resized)

### 3.4 Reliability

- Avoid corrupting the original file on abnormal exit
- Prefer writing to a temp file on save, then replacing original on success

---

## 4. Shortcut Suggestions (Reference: MS-DOS Edit)

| Action | Shortcut |
|--------|----------|
| New | Ctrl+N or menu |
| Open | Ctrl+O |
| Save | Ctrl+S or F2 |
| Save as | Ctrl+Shift+S |
| Exit | Alt+F4 or Alt+X |
| Cut | Ctrl+X or Shift+Del |
| Copy | Ctrl+C or Ctrl+Ins |
| Paste | Ctrl+V or Shift+Ins |
| Undo | Ctrl+Z or Alt+BkSp |
| Find | Ctrl+Q+F |
| Find next | F3 or Ctrl+L |
| Replace | Ctrl+Q+A |
| Select all | Ctrl+A |
| Help | F1 |
| Toggle wrap mode | Ctrl+W |
| Horizontal scroll (WRAP_NONE) | Ctrl+← / Ctrl+→ |

---

## 5. Display System Design (Core Architecture)

> The display system is the foundation for the character-mode editor; all visual output goes through it. Layered abstraction enables cross-platform porting.

### 5.1 Layered Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Application: Editor logic (document, cursor, selection, menu state)  │
├─────────────────────────────────────────────────────────┤
│  Render: Convert logical state to screen (layout, coordinate transform, redraw)  │
├─────────────────────────────────────────────────────────┤
│  Output abstraction: Unified API (putchar, set_cursor, set_attr, etc.)  │
├─────────────────────────────────────────────────────────┤
│  Platform: DOS INT 10h / Windows Console / POSIX termios+ANSI  │
└─────────────────────────────────────────────────────────┘
```

### 5.2 Screen Buffer and Double Buffering

| Item | Implementation |
|------|----------------|
| **Display/buffer** | Maintain full-screen char buffer `char screen[rows][cols*2]` (char+attr) or equivalent |
| **Double buffering** | Each redraw writes to back buffer first, then diff with current screen and output only changed cells to reduce flicker and I/O |
| **Dirty regions** | Track `dirty_rect` (e.g., top_row, left_col, bottom_row, right_col); redraw only dirty area |
| **Full redraw** | On startup, window resize, mode switch |

**Key data structure (C):**
```c
typedef struct {
    char    ch;      /* Single-byte char (phase 1: ASCII/ANSI only, no multibyte) */
    uint8_t attr;    /* Attribute: foreground(4bit)+background(4bit) */
} Cell;
/*
 * Note: For future UTF-8 or GBK, modify this (e.g., uint16_t ch or wchar_t)
 * and update all render logic. This is an architectural change, not a small extension.
 */

typedef struct {
    int rows, cols;
    Cell *buffer;    // Current frame
    Cell *back;      // Back buffer for diff
    bool *dirty;     // Optional: per-cell dirty flag
} ScreenBuffer;
```

### 5.3 Coordinates and Viewport

| Concept | Definition | Implementation |
|---------|------------|----------------|
| **Logical coords** | Document (row, col), 0-based | Row can be long; col is logical (includes line number width) |
| **Screen coords** | Physical (y, x) for terminal cells | Edit area origin (menu_h, line_num_w) if line numbers shown |
| **Viewport** | Currently visible document line range | `view_top_row`: first visible doc row; visible rows = `rows - menu_h - status_h` (e.g., 80×25 ⇒ 25-1-1 = **23** edit rows; subtract 1 more if horizontal scrollbar) |
| **Transform** | Logical→screen | Affected by wrap mode; see 5.9 |

**Vertical scroll:** When cursor leaves viewport, update `view_top_row` so cursor stays in middle or edge (configurable).

### 5.4 Edit Area Rendering Flow

```
1. Based on mode (wrap/scroll), determine viewport and draw logic (see 5.9)
2. From view_top_row (and view_left_col in scroll mode), determine rows/cols to draw
3. For each line:
   a. If line numbers: render line number column (right-aligned, fixed width)
   b. Render text: left to right, handle selection highlight (selected chars use inverted attr)
   c. Long lines: apply 5.9 rules (wrap or horizontal clip)
4. If lines don't fill edit area: fill remaining with spaces + default attr
5. Cursor: convert logical (row,col) to screen (y,x) and write
6. If scrollbars: vertical bar + horizontal bar in scroll mode
```

### 5.5 Interface Layout

| Region | Position | Size | Content | Redraw when |
|--------|----------|------|---------|-------------|
| **Menu bar** | Top (0) | 1 row | Horizontal menu items; dropdown covers rows below | Open/close menu, change active item |
| **Edit area** | Middle | Dynamic | Text + line numbers + optional scrollbar | Doc change, scroll, selection, cursor move |
| **Status bar** | Bottom | 1 row | Row:col, total lines, INSERT/OVR, filename | Cursor move, mode change, file change |
| **Dialogs** | Overlay | Popup | Find/replace/open/save etc. | Open/close, input change |

**Example layout (80×25):**
- Menu bar: row 0
- Edit area: rows 1–23, cols 0–79 (if line numbers use 6 cols, text is 1–74)
- Status bar: row 24

### 5.6 Character Attributes and Colors

| Use | Attribute (example) | Notes |
|-----|---------------------|-------|
| Normal text | 0x07 | Light gray FG, black BG (VGA default) |
| Selected text | 0x70 | Inverted (black FG, light gray BG) |
| Line numbers | 0x08 | Dim gray, distinct from text |
| Menu active | 0x70 | Inverted highlight |
| Menu inactive | 0x07 | Normal |
| Cursor | Invert current cell attr | Or use hardware cursor (BIOS/terminal) |
| Error | 0x0C | Red FG |

Platform must provide: `set_attr(uint8_t attr)`, `get_attr()`, `put_char_at(int y, int x, char c)`.

### 5.7 Platform Output Abstraction

Unified interface (shared by DOS / Windows / Linux):

```c
// display.h - Platform-independent interface
void display_init(void);           // Init, get rows/cols
void display_exit(void);
void display_get_size(int *rows, int *cols);
void display_put_cell(int y, int x, char c, uint8_t attr);
void display_set_cursor(int y, int x);
void display_show_cursor(bool show);
void display_flush(void);          // Commit back buffer to screen
void display_clear(uint8_t attr);  // Clear screen with attr
```

**Platform implementations:**
- **DOS**: INT 10h (mode set, write char, set cursor), direct B800:0 write
- **Windows CMD**: `SetConsoleCursorPosition`, `WriteConsoleOutput`, `SetConsoleTextAttribute`
- **Linux**: POSIX termios raw mode, `write()` ANSI escape sequences for cursor and attr; no third-party libs

### 5.8 Mouse Cursor and Click Mapping

| Item | Implementation |
|------|----------------|
| **Mouse display** | Software cursor: draw custom char (e.g. `▶`) or block at (y,x); or hide system cursor and use highlight block |
| **Click→logical coords** | Based on display mode (5.9), map screen (sx,sy) back to logical (row,col) |
| **Bounds** | If `doc_row` > doc lines, clamp to last line; `doc_col` must not exceed line length |

### 5.9 Word Wrap and Horizontal Scroll (Dual Modes)

> Editor must support **word wrap** and **no wrap (horizontal scroll)**. User can switch via menu or shortcut.

#### 5.9.1 Mode Definitions

| Mode | Behavior | Use case |
|------|----------|----------|
| **Char wrap** | Long lines wrap at edit width into multiple display lines (break at char, not word boundary); no horizontal scroll | Reading long paragraphs, narrow screens |
| **No wrap** | One logical line = one screen line; horizontal scroll for long lines | Tables, code, long lines |

**State:** `enum { WRAP_CHAR, WRAP_NONE }` (WRAP_CHAR = char wrap, WRAP_NONE = scroll), stored in config and restored on startup.

#### 5.9.2 Display Column Width

Columns available for text (minus line numbers and scrollbar):

```
edit_cols = screen_cols - line_num_width - (vertical_scrollbar ? 1 : 0)
```

In wrap mode, wrap width per line = `edit_cols`. In scroll mode, logical line can be much longer than `edit_cols`.

#### 5.9.3 Word Wrap Mode

| Item | Implementation |
|------|----------------|
| **Logical→display lines** | For line length `len`: `display_lines = (len == 0) ? 1 : (len + edit_cols - 1) / edit_cols`; empty line is 1 (ceiling formula gives 0 for len=0) |
| **Logical→screen** | `(row, col)` → compute display lines for rows 0..row-1 for `offs`, then `sub = col / edit_cols`, `screen_row = view_top_row_offset + offs + sub - view_top_display_row`; `screen_col = col % edit_cols` |
| **Screen→logical** | From `screen_row`, accumulate `display_lines` to get `row`; `col = screen_col + (sub_offset * edit_cols)` |
| **Cursor/selection** | Cursor at any logical (row,col), convert to screen (y,x); selection in logical coords, render selected chars with inverted attr |
| **Vertical scroll** | `view_top_row` = first visible logical row; in wrap mode, also `view_top_sub` (wrap offset within line) or `view_top_display_row` (display-row offset from 0) |
| **Line numbers** | When a logical line wraps into multiple display lines, show line number only on first; continuation lines blank or repeat (product decision) |

**Storage:** Precompute or cache `display_line_offset[row]` = display lines before row, for O(1) logic↔screen.

#### 5.9.4 Scroll Mode (No Wrap)

| Item | Implementation |
|------|----------------|
| **Logical↔display** | 1 logical line = 1 display line |
| **Horizontal viewport** | Add `view_left_col`: left edge of viewport in logical cols; visible cols = `edit_cols` |
| **Logical→screen** | `screen_row = doc_row - view_top_row`; `screen_col = doc_col - view_left_col` (visible if doc_col in [view_left_col, view_left_col+edit_cols)) |
| **Screen→logical** | `doc_row = view_top_row + screen_row`; `doc_col = view_left_col + screen_col` |
| **Horizontal scroll** | When cursor moves right past viewport, `view_left_col += 1` or step; same for left. Page Left/Right moves by `edit_cols` |
| **Scrollbar** | Horizontal bar length = visible width / total line length; if lines differ, use max line or current line (product decision) |
| **Long line render** | Draw only `line[view_left_col .. view_left_col+edit_cols-1]`; left portion clipped |

#### 5.9.5 Mode Switch Handling

| Item | Implementation |
|------|----------------|
| **On switch** | WRAP_CHAR→WRAP_NONE: clear `view_top_sub`, set `view_left_col = 0`; WRAP_NONE→WRAP_CHAR: set `view_left_col = 0`, recompute `view_top_display_row` |
| **Cursor** | Logical (row,col) unchanged; only recompute screen position; in scroll mode if `view_left_col` hides cursor, adjust so cursor is visible |
| **Redraw** | Full edit area redraw after switch |

#### 5.9.6 Data Structures and API

```c
// Display mode
typedef enum { WRAP_CHAR, WRAP_NONE } WrapMode; /* WRAP_CHAR=wrap, WRAP_NONE=scroll */

// Viewport state (scroll mode)
int view_left_col;        // Horizontal offset; only for WRAP_NONE

// Viewport state (wrap mode): use "display row" for simpler impl
int view_top_display_row; // Viewport top = which display row (0-based)

// Wrap cache: logical row → display rows before that row
int *display_line_offset; // Or compute on demand

// Coordinate APIs
void logic_to_screen(int row, int col, int *out_y, int *out_x);
void screen_to_logic(int y, int x, int *out_row, int *out_col);
```

#### 5.9.7 Shortcut Suggestions

| Action | Shortcut |
|--------|----------|
| Toggle wrap mode | Ctrl+W or View menu |
| Scroll left | Ctrl+← (WRAP_NONE only) |
| Scroll right | Ctrl+→ (WRAP_NONE only) |
| Screen line start | Home (in scroll mode, goes to view_left_col) |
| Logical line start | Ctrl+Home or Home twice |

---

## 6. Development Plan and Implementation

### 6.1 Module Structure

| Module | File(s) | Responsibility |
|--------|---------|-----------------|
| Document model | `document.c/h` | Text storage, line structure, insert/delete, undo stack |
| Display | `display.c/h` | Interfaces and double buffering from §5 |
| Platform | `platform_dos.c`, `platform_win.c`, `platform_nix.c` | Platform-specific display_* |
| Input | `input.c/h` | Keyboard scan codes, mouse events, normalized event struct |
| Editor logic | `editor.c/h` | Cursor, selection, clipboard, document interaction |
| Menu/UI | `menu.c/h` | Menu bar, dropdown, dialogs |
| Main | `main.c` | Init, main loop, exit |

### 6.2 Document Model Implementation

| Feature | Implementation |
|---------|----------------|
| **Line storage** | `char* lines[]` or `struct Line { char* text; int len; }` dynamic array, one element per line |
| **Insert char** | In `lines[row]` at `col`, `memmove` to make space and write; `realloc` if needed |
| **Delete char** | `memmove` to shift col+1..left, decrement `len` |
| **Line break** | Split current line at col: new line = rest; truncate original; insert at `lines[row+1]` |
| **Merge lines** | Backspace at line start: append current to prev line, delete current line |
| **Undo** | Operation stack: each step `{type, row, col, old_text, new_text}`; undo = reverse execution. **Merge**: consecutive char input (adjacent cols, no jumps) as one record; space, Enter, delete, paste each as own record. **Save point**: on successful save set `undo_save_index = undo_top`; when undo_top reaches that, clear `modified` to avoid "unsaved" after undo to saved state |

### 6.3 Phase 1 (MVP) — Implementation

| Feature | Implementation |
|---------|----------------|
| **New** | `document_new()`: clear `lines`, add one empty line |
| **Open** | `fopen`, read, split by `\n` into `lines[]`, handle `\r` |
| **Save** | Write each line with `\n`; write to `.tmp` first, then `rename` |
| **Exit** | Check `document_modified`, show dialog if true |
| **Insert/delete/line break** | Call document APIs, update cursor |
| **Navigation** | Update `cursor_row`, `cursor_col` from keys, clamp; update `view_top_row` to keep cursor visible |
| **Menu bar** | Static "File  Edit  Search  View  Help"; Alt+letter highlights and opens |
| **Status bar** | `sprintf` "Row %d  Col %d  Lines %d  INS/OVR", output via `display_put_cell` |
| **Mouse click** | Input layer maps (x,y) to (row,col), call `editor_set_cursor(row,col)` |

### 6.4 Phase 2 — Implementation

| Feature | Implementation |
|---------|----------------|
| **Selection** | `selection_start`, `selection_end` (row,col) pairs; Shift+arrows update end |
| **Cut** | Copy selection to clipboard (internal `char* clipboard`), delete range |
| **Copy** | Copy selection to `clipboard` |
| **Paste** | Insert `clipboard` at cursor; handle `\n` (split lines) |
| **Mouse drag selection** | Left-press records start; move updates end and redraws highlight |
| **Menu click** | Check if click is in menu item rect, execute command |
| **Find** | Dialog for search string; linear scan `lines[]` from cursor, `strstr` match |
| **Find next** | Continue from `cursor_col+1` or next line |
| **Undo/redo** | Call document undo/redo, refresh display |

### 6.5 Phase 3 — Implementation

| Feature | Implementation |
|---------|----------------|
| **Replace** | Dialog for find/replace strings; replace current or all |
| **Replace all** | Loop find-and-replace until no match |
| **Save as** | Dialog for new path, run save logic with new path |
| **Line numbers** | Reserve columns in layout; `sprintf("%6d", row+1)`; in wrap mode omit or repeat for continuation (product decision) |
| **Word wrap** | Implement 5.9.3: `display_line_offset[]` cache, `logic_to_screen`/`screen_to_logic`, render with wrap at `edit_cols` |
| **Scroll mode** | Implement 5.9.4: `view_left_col`, horizontal scroll, render only `[view_left_col, view_left_col+edit_cols)` |
| **Mode switch** | `word_wrap` state + menu/shortcut; reset viewport and redraw on switch |
| **Vertical scrollbar** | Right column; `thumb_pos = view_top_row * bar_height / total_lines` (use `view_top_display_row` in wrap) |
| **Horizontal scrollbar** | In scroll mode, above status bar or bottom; `thumb_pos = view_left_col * bar_width / max(1, line_len)` |

### 6.6 Phase 4 — Implementation

| Feature | Implementation |
|---------|----------------|
| **Encoding** | Detect BOM or probe on read; internal UTF-8 or wide char, convert for terminal as needed |
| **Syntax highlighting** | Set attr by token type (keyword, string, etc.); start with 1–2 languages |
| **Wrap optimizations** | Defer or cache `display_line_offset` for large files; invalidate on resize |

### 6.7 Development Order

```
Phase 1 (≈2–3 weeks):
  [1] display abstraction + one platform (e.g. Windows)
  [2] document basics + line storage
  [3] Main loop + keyboard input
  [4] Edit area render + cursor
  [5] Menu bar, status bar
  [6] File open/save/new
  [7] Mouse click positioning

Phase 2 (≈2 weeks):
  [8] Selection + clipboard
  [9] Find
  [10] Undo stack
  [11] Mouse drag, menu click

Phase 3 (≈2 weeks):
  [12] Word wrap + scroll dual mode
  [13] Replace, save as
  [14] Line numbers, vertical/horizontal scrollbars

Phase 4: Implement by priority
```

---

## 7. Completed Extensions and Future Improvements

### 7.1 Completed (Phase 5–8)

| Feature | Description |
|---------|-------------|
| **Encoding support** | UTF-8 multibyte editing, GBK auto-detection and conversion, internal UTF-8 storage |
| **Syntax highlighting** | INI-driven; C/C++, Python, Markdown, JavaScript, HTML, CSS, JSON, Batch; View > Set Language... for manual switch |
| **Tab expansion** | `\t` displayed as spaces per tab_size; file still stores `\t` |
| **Status bar encoding/language** | Shows file encoding (UTF-8/GBK) and syntax language name |
| **Syntax incremental refresh** | After edit, rebuild block-comment state from cursor row until stable |
| **TUI file browser** | Open/Save show a full directory browser; keyboard, mouse, double-click to enter dirs or select files |
| **Path quick-jump** | In file browser, press /, \, ~, Ctrl+G or click path bar to type a target path directly |
| **Extension-syntax config** | syntax_ext_<ext>=<ini> in config.ini overrides syntax mapping for any extension |
| **Default filename** | Status bar and save prompts show Untitle when no file is open |

### 7.2 Future Improvements

1. **Multiple documents/tabs**: Open and switch between several files
2. **File filter**: Filter entries by extension in the file browser (e.g. *.c only)
3. **Recent files shortcut**: Integrate recent file list into the Open dialog
4. **Macros/scripts**: Record and replay operation sequences
5. **Plugin system**: Extend functionality via plugins

---

*Document version: v1.6*  
*Created: 2026-02-28*  
*Phase 8 updates: TUI directory browser for Open/Save, path quick-jump, configurable extension-syntax mapping (syntax_ext_*), default filename Untitle*
