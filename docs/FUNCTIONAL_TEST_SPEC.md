# DOS Text Editor - Functional Test Specification

[中文](功能测试规范.md) | English

> Test methods and success/failure boundary definitions for each feature

---

## 1. Test Instructions

| Term | Meaning |
|------|------|
| **Pass (✓)** | Meets all conditions of "success boundary", no "failure boundary" cases |
| **Fail (✗)** | Any "failure boundary" case occurs, or "success boundary" not met |
| **Manual test** | Human operation, visual inspection |
| **Automated test** | Unit tests or script-driven, **zero third-party dependencies**, uses only C standard library and project self-implemented assertions |
| **Boundary case** | Special inputs: empty document, single character, extra-long line, maximum file size, etc. |

---

## 2. File Operations

### 2.1 New File

| Test Step | Action |
|----------|------|
| 1 | Launch editor (no args or with `--new`) |
| 2 | Select File → New, or press Ctrl+N |

| Success Boundary | Failure Boundary |
|----------|----------|
| Edit area shows blank, only 1 line (empty line) | Crash, black screen, freeze |
| Cursor at (0, 0) | Cursor invisible or wrong position |
| Status bar shows "Row 1 Col 1" or equivalent | Status bar blank or wrong values |
| Modified flag false, no save/exit prompt | Incorrect "unsaved" prompt |
| Title/status bar may show "Untitled" or empty | - |

---

### 2.2 Open File

| Test Step | Action |
|----------|------|
| 1 | Prepare test file `test.txt` with known content (e.g., 3 lines, 10 chars each) |
| 2 | File → Open or Ctrl+O, enter path `test.txt` |
| 3 | Confirm open |

| Success Boundary | Failure Boundary |
|----------|----------|
| Edit area displays content consistent with file | Content lost, garbled, wrong line breaks |
| Cursor at (0, 0) | Cursor at wrong position |
| Row count, column count correct | Wrong row/column count |
| Status bar shows correct row/column | - |
| Opening empty file shows 1 empty line | Empty file causes crash |
| Opening non-existent path shows error prompt | Crash, silent failure |
| Opening file without read permission shows error | Crash, silent failure |
| Opening oversized file shows prompt or rejects | Silent truncation, memory overflow |

**Boundary cases:** Empty file, single character, only newline, extra-long single line, mixed \r\n and \n

---

### 2.3 Save File

| Test Step | Action |
|----------|------|
| 1 | New or open file, modify content |
| 2 | File → Save or Ctrl+S (existing path) or enter new path (Save As) |
| 3 | Read disk file with external tool (e.g., `type`, `cat`) to verify |

| Success Boundary | Failure Boundary |
|----------|----------|
| Disk file content matches edit area | Content mismatch, truncation |
| Line endings are `\n` (or platform-defined) | `\r\n` lost or extra |
| Original file correctly overwritten (when not Save As) | Original file corrupted, leftover temp file |
| "Unsaved" flag cleared after save | Still prompts unsaved |
| Saving to read-only/invalid path shows error | Crash, silent failure |
| Interrupt during save (e.g., disk full) does not corrupt original | Original file partially overwritten or corrupted |

---

### 2.4 Save As

| Test Step | Action |
|----------|------|
| 1 | Open or edit document |
| 2 | File → Save As, enter new path `other.txt` |
| 3 | Confirm |

| Success Boundary | Failure Boundary |
|----------|----------|
| New file `other.txt` has correct content | Not created or wrong content |
| Original file unchanged (if path existed before) | Original file incorrectly modified |
| Current edit path updated to new path | Subsequent save still writes to old path |
| When new path exists, overwrite with prompt (if implemented) | Silent overwrite, no prompt |

---

### 2.5 Exit and Pre-Save Prompt

| Test Step | Action |
|----------|------|
| 1a | Exit after modifying (Alt+X / Alt+F4) |
| 1b | Exit directly without modifying |
| 1c | Exit choosing "Don't Save" after modifying |
| 1d | Choose "Cancel" to not exit after modifying |

| Success Boundary | Failure Boundary |
|----------|----------|
| When modified, confirmation dialog (Save/Don't Save/Cancel) | Exits directly and loses changes |
| Choosing "Save" performs save then exit | Choose save but no write |
| Choosing "Don't Save" exits directly, no disk write | Still writes to disk |
| Choosing "Cancel" stays in editor | Still exits |
| When unmodified, exits directly, no dialog | Incorrectly shows confirmation |

---

## 3. Text Editing

### 3.1 Insert Character

| Test Step | Action |
|----------|------|
| 1 | Type `a`, `b`, `c` in empty line or middle of existing text |
| 2 | Observe cursor position and following characters |

| Success Boundary | Failure Boundary |
|----------|----------|
| New character appears at cursor, following characters shift right | Overwrites following chars, inserts at wrong position |
| Cursor moves to right of new character | Cursor unmoved or wrong position |
| Multi-line: inserts only in current line | Affects other lines |
| Insert at line end correctly extends that line | Wrong line break, truncation |

**Boundary:** Behavior at line start, line end, first character in empty document, when reaching max column limit (reject or wrap)

---

### 3.2 Delete Character (Backspace / Delete)

| Test Step | Action |
|----------|------|
| 1 | Cursor in line, press Backspace, observe previous character deleted |
| 2 | Cursor in line, press Delete, observe next character deleted |
| 3 | Cursor at line start, press Backspace (should merge with previous line) |
| 4 | Cursor in empty line, press Backspace (should delete empty line and merge) |

| Success Boundary | Failure Boundary |
|----------|----------|
| Backspace deletes character before cursor | Deletes wrong char, no effect |
| Delete deletes character after cursor | Deletes wrong char, no effect |
| Backspace at line start merges with previous line | Crash, delete only no merge |
| Backspace at document first line first column has no side effect | Crash, out of bounds |
| Delete at line end has no side effect (does not delete next line's first char) | Incorrectly deletes next line |

---

### 3.3 Newline (Enter)

| Test Step | Action |
|----------|------|
| 1 | Press Enter in middle of line |
| 2 | Press Enter at line end |
| 3 | Press Enter in empty line |

| Success Boundary | Failure Boundary |
|----------|----------|
| Current line breaks at cursor, second half moves to new line | Content lost, wrong order |
| Cursor moves to start of new line | Cursor stays on old line or wrong position |
| Enter at line end creates new empty line | No new line or extra empty lines |
| Enter in empty line creates another empty line | Crash, anomaly |

---

### 3.4 Insert/Overwrite Mode

| Test Step | Action |
|----------|------|
| 1 | Position before 'b' in "abc", press Insert to switch to overwrite mode |
| 2 | Type "x", observe becomes "axc" |
| 3 | Press Insert again to return to insert mode, cursor at col=2 (after 'x'), type "y", observe "axyc" |

| Success Boundary | Failure Boundary |
|----------|----------|
| In overwrite mode, input replaces current character | Still inserts |
| Status bar shows OVR / INS or equivalent | Not shown or wrong display |
| Behavior correctly changes after switch | Switch has no effect |

---

### 3.5 Undo (Ctrl+Z)

| Test Step | Action |
|----------|------|
| 1 | Type "hello", press Ctrl+Z 5 times |
| 2 | Press Ctrl+Z 5 more times (should stay empty after no ops) |
| 3 | Type "x", then Ctrl+Z, should return to empty |

| Success Boundary | Failure Boundary |
|----------|----------|
| Each undo reverts one operation | One undo reverts multiple, or undo ineffective |
| After undoing to empty document, no further change | Crash, out of bounds |
| Undo insert → delete, undo delete → restore | State inconsistent with actual |
| Undo newline → two lines merge, cursor correct | Wrong merge, cursor wrong |

---

### 3.6 Redo (Ctrl+Y)

| Test Step | Action |
|----------|------|
| 1 | Type "ab", Ctrl+Z undo, Ctrl+Y redo |
| 2 | Ctrl+Y again should have no change |

| Success Boundary | Failure Boundary |
|----------|----------|
| Redo restores undone operation | Redo ineffective, over-restore |
| After new operation, redo stack cleared (optional impl) | Redo produces unexpected content |
| When nothing to redo, Ctrl+Y has no side effect | Crash |

---

## 4. Cursor and Navigation

### 4.1 Arrow Keys

| Test Step | Action |
|----------|------|
| 1 | Press ↑↓←→ in multi-line text |
| 2 | Press ↑ on first line, ↓ on last column of last line |
| 3 | Cursor at line start, press ← (expected: jump to end of previous line) |
| 4 | Cursor at line end, press → (expected: jump to start of next line) |
| 5 | Press ← at document first row first col, → at document last row last col |

| Success Boundary | Failure Boundary |
|----------|----------|
| ↑ moves to same column on previous line (or line end if shorter) | Out of bounds, crash |
| ↓ moves to same column on next line (or line end if shorter) | Out of bounds, crash |
| ← at line start jumps to end of previous line (DOS standard) | ← at line start has no effect, or cursor out of bounds |
| → at line end jumps to start of next line (DOS standard) | → at line end has no effect, or cursor out of bounds |
| ← at document first row first col has no side effect (no previous line) | Crash, negative index |
| → at document last row last col has no side effect (no next line) | Crash, out of bounds |

---

### 4.2 Home / End

| Test Step | Action |
|----------|------|
| 1 | Press Home, End in middle of line |

| Success Boundary | Failure Boundary |
|----------|----------|
| Home moves to start of current line | Moves to wrong position |
| End moves to end of current line | Moves to wrong position |
| On empty line, Home/End both at (row, 0) | Column not 0 |

---

### 4.3 Ctrl+Home / Ctrl+End

| Test Step | Action |
|----------|------|
| 1 | Press Ctrl+Home, Ctrl+End anywhere in document |

| Success Boundary | Failure Boundary |
|----------|----------|
| Ctrl+Home moves to (0, 0) | Doesn't reach document start |
| Ctrl+End moves to last row, last column | Doesn't reach document end |
| In empty document, both at (0, 0) | Crash |

---

### 4.4 Page Up / Page Down

| Test Step | Action |
|----------|------|
| 1 | Press Page Up, Page Down in long document |

| Success Boundary | Failure Boundary |
|----------|----------|
| Cursor moves approximately one screen of rows (consistent with edit_rows) | Wrong number of rows moved |
| On first screen, Page Up to (0, current col) | Out of bounds |
| On last screen, Page Down to last line | Out of bounds, beyond document |

---

### 4.5 Ctrl+← / Ctrl+→ (Horizontal viewport scroll, only effective in WRAP_NONE mode)

| Test Step | Action |
|----------|------|
| 1 | Switch to horizontal scroll mode (WRAP_NONE), open file with extra-long line |
| 2 | Press Ctrl+← / Ctrl+→, observe viewport moving horizontally |
| 3 | Switch to wrap mode (WRAP_CHAR), confirm Ctrl+←/→ has no effect |

| Success Boundary | Failure Boundary |
|----------|----------|
| In WRAP_NONE, viewport scrolls left/right, visible range updates correctly | No effect, crash |
| After scroll, cursor stays within visible area | Cursor moves off screen |
| In WRAP_CHAR, Ctrl+←/→ has no effect | In wrap mode still triggers scroll or cursor jumps |
| At line start/end boundary, no further scroll | Out of bounds, view_left_col becomes negative |

---

## 5. Selection and Clipboard

### 5.1 Shift+Arrow Key Selection

| Test Step | Action |
|----------|------|
| 1 | Cursor at 'h' in "hello", press Shift+→ multiple times |
| 2 | Shift+↓ to select across lines |
| 3 | After selecting, move cursor (without Shift) should cancel selection |

| Success Boundary | Failure Boundary |
|----------|----------|
| Selected area highlighted (inverse, etc.) | No highlight, wrong highlight |
| Selected content matches logical range | Over-select, under-select |
| Cross-line selection correct | Wrong line boundaries |
| After canceling selection, highlight disappears | Leftover highlight |

---

### 5.2 Select All (Ctrl+A)

| Test Step | Action |
|----------|------|
| 1 | Press Ctrl+A anywhere |
| 2 | Check selection range |

| Success Boundary | Failure Boundary |
|----------|----------|
| Entire document selected | Partially selected, not selected |
| Selection start (0,0), end (last_row, last_col) | Wrong range |

---

### 5.3 Cut (Ctrl+X)

| Test Step | Action |
|----------|------|
| 1 | Select "abc", Ctrl+X |
| 2 | Observe clipboard and document |

| Success Boundary | Failure Boundary |
|----------|----------|
| Selected content removed from document | Not removed or wrong removal |
| Clipboard contains removed content | Clipboard empty or wrong |
| When nothing selected, cut has no effect or cuts current line (per product) | Crash |

---

### 5.4 Copy (Ctrl+C)

| Test Step | Action |
|----------|------|
| 1 | Select "abc", Ctrl+C |
| 2 | Document unchanged, clipboard has "abc" |
| 3 | Move cursor then paste |

| Success Boundary | Failure Boundary |
|----------|----------|
| Document unchanged | Document modified |
| Clipboard content correct (including multiline) | Lost newlines, truncation |
| After paste, content correct | Wrong paste |

---

### 5.5 Paste (Ctrl+V)

| Test Step | Action |
|----------|------|
| 1 | Copy "a\nb\nc", cursor before 'y' in "xyz", paste |
| 2 | Expected "xa\nb\ncyz" or equivalent |

| Success Boundary | Failure Boundary |
|----------|----------|
| Paste inserts clipboard content at cursor | Wrong insert position |
| Multi-line correctly split into multiple lines | Lost newlines, becomes single line |
| Empty clipboard, paste has no effect | Crash, insert garbage |
| When selection exists, paste may delete selection first (per product) | Behavior not as expected |

---

### 5.6 Delete Selection (Delete/Backspace)

| Test Step | Action |
|----------|------|
| 1 | Select a block of text, press Delete or Backspace |

| Success Boundary | Failure Boundary |
|----------|----------|
| Selected content deleted | Only deletes one char, wrong range |
| Cursor moves to start of deleted range | Wrong cursor position |

---

## 6. Find and Replace

### 6.1 Find

| Test Step | Action |
|----------|------|
| 1 | Document contains "foo bar foo", open find, enter "foo" |
| 2 | Execute find |

| Success Boundary | Failure Boundary |
|----------|----------|
| Locates first "foo" and highlights (if implemented) | Not found, wrong location |
| Empty search string: prompt or ignore | Crash |
| Searching for non-existent string shows "Not found" | Silent, false location |

---

### 6.2 Find Next (F3)

| Test Step | Action |
|----------|------|
| 1 | After finding first match, press F3 |
| 2 | Continue F3 until past end of document |

| Success Boundary | Failure Boundary |
|----------|----------|
| Each time locates next match | Repeats same match, skips matches |
| After document end, prompt or wrap to start | Crash, out of bounds |

---

### 6.3 Find Previous (Shift+F3)

| Test Step | Action |
|----------|------|
| 1 | Press Shift+F3 when after a match |

| Success Boundary | Failure Boundary |
|----------|----------|
| Locates previous match | Wrong direction, repeat |
| At document start, prompt or wrap to end | Crash |

---

### 6.4 Replace

| Test Step | Action |
|----------|------|
| 1 | Find "foo", replace with "bar" |
| 2 | Execute replace current |

| Success Boundary | Failure Boundary |
|----------|----------|
| Current match replaced with "bar" | Not replaced, wrong position |
| Other "foo" in document unchanged | Incorrectly replaced elsewhere |

---

### 6.5 Replace All

| Test Step | Action |
|----------|------|
| 1 | Document "foo foo foo", replace all "foo"→"bar" |
| 2 | Check document and replace count |

| Success Boundary | Failure Boundary |
|----------|----------|
| All "foo" become "bar" | Missed, over-replaced |
| Show replace count (if implemented) | Wrong count |
| When no match, show prompt | Crash, false replace |

---

### 6.6 Case Sensitivity (Optional)

| Test Step | Action |
|----------|------|
| 1 | Document "Foo foo FOO", case-insensitive find "foo" should find 3 |
| 2 | Case-sensitive find "foo" should find 1 |

| Success Boundary | Failure Boundary |
|----------|----------|
| After option toggle, find results correct | Option ineffective, wrong results |

---

## 7. Display and Interface

### 7.1 Menu Bar

| Test Step | Action |
|----------|------|
| 1 | Observe if top row has File, Edit, Search, View, Help |
| 2 | Alt+F, Alt+E etc. to open dropdown |

| Success Boundary | Failure Boundary |
|----------|----------|
| Menu items visible, recognizable | Garbled, overlapping, unreadable |
| Alt+letter opens corresponding dropdown | No response, wrong menu |
| Dropdown items selectable with arrow keys, Enter executes | No response, wrong command |

---

### 7.2 Edit Area and Scroll

| Test Step | Action |
|----------|------|
| 1 | Open file with more lines than visible |
| 2 | Arrow keys, Page Down to move down |
| 3 | Observe if viewport scrolls |

| Success Boundary | Failure Boundary |
|----------|----------|
| Visible content scrolls with cursor movement | No scroll, wrong scroll direction |
| Cursor always within visible area | Cursor off screen |
| At document bottom/top, no incorrect scroll | Out-of-bounds scroll, blank line anomaly |

---

### 7.3 Status Bar

| Test Step | Action |
|----------|------|
| 1 | Move cursor, type characters, toggle insert/overwrite |

| Success Boundary | Failure Boundary |
|----------|----------|
| Row/column match cursor (1-based or 0-based both OK) | Wrong values, not updated |
| Total line count correct | Wrong total lines |
| INSERT/OVR matches mode | Not updated, wrong display |

---

### 7.4 Wrap Mode Toggle (Ctrl+W)

| Test Step | Action |
|----------|------|
| 1 | Type line longer than edit area width |
| 2 | Switch to char-wrap mode (WRAP_CHAR), observe if wraps at character position |
| 3 | Switch to horizontal scroll mode (WRAP_NONE), observe single line + horizontal scroll |

| Success Boundary | Failure Boundary |
|----------|----------|
| In wrap mode, long line wraps into multiple display lines at char positions | Beyond screen, wrong truncation |
| In horizontal scroll mode, single line display, can scroll horizontally | Still wrapped, cannot scroll |
| After switch, display updates immediately | Leftover old display |
| Cursor, selection correct in both modes | Wrong coordinates, selection mess |

---

### 7.5 Horizontal Scroll (Horizontal Scroll Mode)

| Test Step | Action |
|----------|------|
| 1 | Disable auto-wrap, move cursor right in long line |
| 2 | Use Ctrl+→ or arrow keys |

| Success Boundary | Failure Boundary |
|----------|----------|
| Viewport moves right, can see right side of line | Cannot see, no scroll |
| view_left_col correct, render range correct | Wrong display, overlap |
| Home at line start goes to column 0 | Wrong position |

---

### 7.6 Line Number Display (Optional)

| Test Step | Action |
|----------|------|
| 1 | Enable line numbers, observe left side |

| Success Boundary | Failure Boundary |
|----------|----------|
| Each logical line start shows corresponding line number | Wrong number, missing |
| Wrapped continuation lines: line number per product (blank/repeat) | Inconsistent with document |
| Line number column does not shift edit area columns | Edit area column misalignment |

---

## 8. Help and Tips

### 8.1 F1 Help

| Test Step | Action |
|----------|------|
| 1 | Press F1 |

| Success Boundary | Failure Boundary |
|----------|----------|
| Displays shortcut list or help content | No response, blank |
| Can close and return to editing | Cannot close, freeze |

---

### 8.2 Error Prompts

| Test Step | Action |
|----------|------|
| 1 | Open non-existent file, no-permission file, save to read-only path |
| 2 | Observe prompt content |

| Success Boundary | Failure Boundary |
|----------|----------|
| Clear error message (popup or status bar) | No prompt, crash |
| After prompt, can continue using editor | Cannot recover |
| Disk full, invalid path etc. have prompt | Silent failure |

---

## 9. Mouse Support

### 9.1 Click to Position Cursor

| Test Step | Action |
|----------|------|
| 1 | Left-click at a position in edit area |
| 2 | Observe if cursor moves to that position |

| Success Boundary | Failure Boundary |
|----------|----------|
| Cursor moves to clicked (row, col) | Wrong position, no response |
| Auto-wrap/horizontal scroll: coordinate conversion correct | Click and cursor don't match |
| Clicking menu/status bar does not mistakenly move cursor | Mistaken move |

---

### 9.2 Drag Selection

| Test Step | Action |
|----------|------|
| 1 | Press left button at one position, drag to another, release |
| 2 | Observe selection |

| Success Boundary | Failure Boundary |
|----------|----------|
| Selection is range from press to release | Wrong range, no selection |
| Selected content highlighted | No highlight |
| Cross-line drag correct | Selection broken, wrong line |

---

### 9.3 Menu Click

| Test Step | Action |
|----------|------|
| 1 | Left-click menu item (e.g., File → Open) |
| 2 | Observe if dialog opens or command executes |

| Success Boundary | Failure Boundary |
|----------|----------|
| Clicking menu item has response | No response, wrong command |
| Clicking blank area closes menu | Menu cannot close |

---

## 10. Mouse Support (Extended)

### 10.1 Double-Click Select Word

| Test Step | Action |
|----------|------|
| 1 | Double-click on a word |

| Success Boundary | Failure Boundary |
|----------|----------|
| Entire word selected | Only partial chars, over-select |

---

### 10.2 Triple-Click Select Line

| Test Step | Action |
|----------|------|
| 1 | Triple-click anywhere |

| Success Boundary | Failure Boundary |
|----------|----------|
| Entire line selected | Only partial |

---

### 10.3 Right-Click Menu

| Test Step | Action |
|----------|------|
| 1 | Right-click in edit area |
| 2 | Select menu item to execute |

| Success Boundary | Failure Boundary |
|----------|----------|
| Context menu appears | No menu or wrong position |
| Menu items executable (Cut/Copy/Paste/Select All/Find Selected) | Execution ineffective |

---

### 10.4 Mouse Wheel

| Test Step | Action |
|----------|------|
| 1 | Scroll mouse wheel up/down |

| Success Boundary | Failure Boundary |
|----------|----------|
| Document scrolls up/down | No scroll or wrong direction |
| Cursor position stays unchanged | Cursor moves with scroll |

---

## 11. Encoding and Syntax Highlighting

### 11.1 UTF-8/GBK Multi-byte Editing

| Test Step | Action |
|----------|------|
| 1 | Open file with Chinese (UTF-8 encoded) |
| 2 | Move with arrow keys between characters, verify moves by codepoint not byte |
| 3 | Type Chinese, save and reopen, content consistent |

| Success Boundary | Failure Boundary |
|----------|----------|
| Chinese chars display width 2 (two columns) | Wrong display alignment |
| Cursor moves by character (one Chinese char = one left/right) | Cursor moves by byte, stops mid-char |
| GBK file auto-detected and displays correctly | Garbled |

---

### 11.2 Tab Expand Display

| Test Step | Action |
|----------|------|
| 1 | Type Tab, observe number of spaces expanded |
| 2 | Modify tab_size in config.ini, observe expand width change |

| Success Boundary | Failure Boundary |
|----------|----------|
| Tab expands to configured tab_size spaces | Fixed at 8 spaces |
| File still stores single `\t` character | File rewritten with spaces |

---

### 11.3 Syntax Highlighting

| Test Step | Action |
|----------|------|
| 1 | Open .c / .py / .js etc. files with syntax definitions |
| 2 | Observe keywords, strings, comments colored per config |

| Success Boundary | Failure Boundary |
|----------|----------|
| Keywords, strings, comments colored per INI config | No highlight or wrong colors |
| Block comments correctly highlighted across lines | Highlight lost after line break |

---

### 11.4 Manual Syntax Language Switch

| Test Step | Action |
|----------|------|
| 1 | Open any file |
| 2 | View > Set Language... |
| 3 | Select a language or "(None)" |
| 4 | Observe highlight change |

| Success Boundary | Failure Boundary |
|----------|----------|
| Language list dialog appears | No dialog or empty list |
| After selection, highlight changes immediately | Highlight unchanged or crash |
| Selecting "(None)" disables highlight | Residual highlight |

---

### 11.5 Status Bar Encoding/Language Display

| Test Step | Action |
|----------|------|
| 1 | Observe right side of status bar |
| 2 | Open files with different encoding/language |

| Success Boundary | Failure Boundary |
|----------|----------|
| Shows "UTF-8" or "GBK" | Encoding not shown |
| Shows current syntax language (e.g., "C") | Language name not shown |

---

### 11.6 Configurable Extension→Syntax Mapping

| Test Step | Action |
|----------|------|
| 1 | Add `syntax_ext_xyz=c.ini` to `config.ini` |
| 2 | Create `hello.xyz` and open it in the editor |
| 3 | Observe whether C syntax highlighting is applied |
| 4 | Remove the config entry, reopen the file, confirm highlighting is gone |

| Success Boundary | Failure Boundary |
|----------|----------|
| `.xyz` file uses C syntax highlighting (keywords, comments correctly colored) | No highlight, wrong syntax used |
| Non-existent ini name (e.g. `noexist.ini`) fails gracefully (no highlight, no crash) | Crash, error dialog |
| After removing config, same file reverts to default (no highlight or other matching ini) | Still uses deleted config's highlighting |
| Case-insensitive: `syntax_ext_XYZ` and `syntax_ext_xyz` produce same result | Case-sensitive mismatch |

---

### 11.7 File Browser Path Quick-Jump

| Test Step | Action |
|----------|------|
| 1 | Open file (Ctrl+O), press `/` or `\` in the browser dialog |
| 2 | Confirm path input dialog appears, pre-filled with current dir + typed character |
| 3 | Enter a valid directory path and press Enter |
| 4 | Press `~` to jump to the user home directory |
| 5 | Press `Ctrl+G` to open path input dialog (pre-filled with current dir, no appended char) |
| 6 | Click the path bar at the top of the dialog to trigger path input |

| Success Boundary | Failure Boundary |
|----------|----------|
| `/` / `\` triggers path input dialog, pre-filled with current dir + typed char | No response, char treated as filter |
| `~` jumps to user home directory (%APPDATA% / $HOME) | Jump fails or wrong path |
| `Ctrl+G` triggers path input dialog, pre-filled with current dir | No response |
| Clicking path bar triggers path input dialog, pre-filled with current dir | No response |
| Entering a valid directory navigates there, list refreshes | No refresh or crash |
| Entering a non-existent path shows "Path not found" message | Crash, silent failure |
| In save mode, entering full file path (directory exists) directly confirms save | No confirmation or path parse error |

---

## 12. Performance and Stability

### 12.1 Startup Time

| Test Method | Multiple launches, time until UI ready |
|----------|--------------------------|
| Success Boundary | < 2 seconds (requirement 3.2) |
| Failure Boundary | ≥ 2 seconds |

---

### 12.2 Open Large File

| Test Method | Open ~100KB file, time it |
|----------|-------------------------|
| Success Boundary | < 1 second |
| Failure Boundary | ≥ 1 second, freeze, memory anomaly |

---

### 12.3 Keyboard Response

| Test Method | Rapid continuous keypresses, observe for dropped keys or noticeable delay |
|----------|----------------------------------------|
| Success Boundary | Every keypress has response, no noticeable delay |
| Failure Boundary | Dropped keys, stutter, > 200ms delay |

---

### 12.4 Crash and Exception

| Test Method | Various boundary ops: empty doc, single line, huge file, rapid continuous ops |
|----------|--------------------------------------------------|
| Success Boundary | No crash, no memory leak (memory stable after long run) |
| Failure Boundary | Crash, deadlock, memory keeps growing |

---

## 13. Test Checklist (Quick Reference)

| Category | Pass | Fail |
|------|------|------|
| File | New (Untitle)/browse open/browse save/save as/exit flow correct | Crash, data loss, dialog not shown |
| Edit | Add/delete/modify, newline, undo/redo correct | Content mess, crash |
| Navigation | Arrow keys, Home/End, page up/down correct | Out of bounds, cursor invisible |
| Selection | Selection, clipboard correct | Wrong range, wrong paste |
| Find | Find/replace results correct | Missed finds, wrong replace |
| Display | Menu/status bar/scroll/wrap/horizontal scroll correct | Misalignment, no refresh |
| Mouse | Click, drag, double-click word, triple-click line, wheel, right-click menu | No response, wrong coordinates |
| Encoding | UTF-8/GBK detection and conversion, multi-byte cursor move | Garbled, cursor wrong |
| Syntax | Highlight, Tab expand, manual language switch, status bar encoding/language, extension override mapping | No highlight, wrong color |
| Performance | Startup<2s, open 100KB<1s, keys responsive | Timeout, freeze |

**Automated tests:** ~329 cases, covering document, clipboard, search, viewport, editor (including Untitle default filename), config (including syntax_ext_* parsing), util (including path_is_dir / dir_read_entries), utf8, encoding, syntax (including syntax_match_ext override mapping).

---

*Document version: v1.6*  
*Corresponds to functional requirements document v1.6*  
*Phase 8 update: added Untitle default filename, TUI file/directory browser (open/save/save as), path quick-jump (/ \ ~ Ctrl+G and path bar click), configurable extension syntax mapping (syntax_ext_*) test cases; automated test count updated to 329*
