# DOS 文本编辑器

[English](README.md) | 中文

仿 MS-DOS Edit 的字符界面文本编辑器，采用 C17 编写，无第三方依赖。可在 Windows 命令提示符、Linux 终端和 FreeDOS 下运行。

## 功能特性

- 纯 C17，零第三方依赖
- Windows Console API / POSIX termios / DOS BIOS 多平台后端
- 双缓冲渲染（低闪烁，脏区差分刷新）
- 字符换行与水平滚动两种显示模式
- 可视化垂直滚动条（最右列），支持点击和拖拽
- 可视化水平滚动条（底行，仅水平滚动模式），支持点击和拖拽
- 多级撤销 / 重做
- 查找与替换（支持大小写敏感/不敏感）
- 完整鼠标支持：单击、双击（选词）、三击（选行）、拖拽、滚轮、右键菜单
- 菜单栏（Alt+字母或鼠标点击打开；下拉项支持鼠标点击）
- 右键上下文菜单：剪切 / 复制 / 粘贴 / 全选 / 查找选中内容
- 行号显示（通过“查看”菜单切换）
- 内部剪贴板（跨平台，不依赖系统剪贴板）
- **UTF-8 多字节编辑** — 支持 CJK 等 Unicode 字符，码点感知光标移动，宽字符（2 列）显示
- **GBK / UTF-8 自动检测** — 打开时检测 UTF-8 或 GBK，内部统一转 UTF-8，保存时按原编码回写
- **Tab 展开** — `\t` 按可配置的制表位展开为空格（仅显示；文件中仍存为 `\t`）
- **语法高亮** — 基于外部 INI 文件的规则着色；内置 C/C++、Python、Markdown、JavaScript、HTML、CSS、JSON、Batch；无需重新编译即可自定义
- **手动切换语言** — 查看 > 设置语言... 打开可滚动列表，为当前文件选择语法高亮或关闭
- **目录浏览文件选择器** — 打开 / 保存时弹出 TUI 文件浏览对话框，可浏览目录、选择文件；支持键盘、鼠标及路径快速跳转（`/`、`\`、`~`、`Ctrl+G` 或点击路径栏）
- **可配置扩展名→语法映射** — 在 `config.ini` 中通过 `syntax_ext_<ext>=<ini文件>` 自定义任意扩展名对应的语法高亮，无需修改代码
- **默认文件名** — 未打开任何文件时，状态栏显示 `Untitle`

---

## 代码规模

| 类别 | 文件数 | 行数 |
|------|--------|------|
| C 源码（`src/`、`tests/`） | 31 | ~7,500 |
| 头文件（`include/`、`src/platform/`） | 17 | ~1,050 |
| 语法规则（INI） | 8 | ~440 |
| **合计** | 56 | **~9,000** |

*不含 `cmake-build/`。约 303 个自动化单元测试。*

---

## 构建（CMake）

### 环境要求

- CMake 3.16+
- Visual Studio 2026 Enterprise（Windows）或 GCC/Clang（Linux/macOS）

### Windows 快速构建

```bat
cmake_build.bat          # 配置 + 构建（Release）
cmake_build.bat test     # 构建 + 运行全部 ~303 个单元测试
cmake_build.bat clean    # 删除 cmake-build/
cmake_build.bat rebuild  # 清理后重新构建
```

输出：`cmake-build\bin\Release\edit.exe`

### 手动 CMake — Windows（Visual Studio 2026 未自动检测时）

```bat
cmake -S . -B cmake-build -G "Visual Studio 18 2026" -A x64 ^
      -DCMAKE_GENERATOR_INSTANCE="C:\Program Files\Microsoft Visual Studio\18\Enterprise"
cmake --build cmake-build --config Release
cmake --build cmake-build --config Release --target test_runner
cmake-build\bin\Release\test_runner.exe
```

### 手动 CMake — 跨平台

```bash
cmake -S . -B cmake-build
cmake --build cmake-build --config Release
cmake --build cmake-build --config Release --target test_runner
cmake-build/bin/Release/test_runner
```

---

## 使用说明

```bat
cmake-build\bin\Release\edit.exe [文件名]
```

若 `文件名` 不存在，首次保存时将以此名创建新文件。

打开具有已知扩展名（`.c`、`.cpp`、`.h`、`.py`、`.md` 等）的文件时，编辑器会自动应用语法高亮。高亮规则从可执行文件同目录的 `syntax/` 文件夹加载 — 可复制或编辑 INI 文件以自定义颜色。

---

## 键盘快捷键

| 操作 | 快捷键 |
|------|--------|
| 新建 | Ctrl+N |
| 打开 | Ctrl+O |
| 保存 | Ctrl+S 或 F2 |
| 另存为 | Ctrl+Shift+S |
| 退出 | Alt+X |
| 撤销 | Ctrl+Z |
| 重做 | Ctrl+Y |
| 剪切 | Ctrl+X |
| 复制 | Ctrl+C |
| 粘贴 | Ctrl+V |
| 全选 | Ctrl+A |
| 查找 | Ctrl+F |
| 查找下一个 | F3 |
| 查找上一个 | Shift+F3 |
| 替换 | Ctrl+H |
| 切换换行/横滚 | Ctrl+W |
| 水平滚动（横滚模式） | Ctrl+← / Ctrl+→ |
| 切换插入/覆盖 | Insert |
| 帮助 | F1 |

---

## 鼠标操作

| 操作 | 效果 |
|------|------|
| 左键单击 | 将光标移动到点击位置 |
| 双击 | 选中光标下的词 |
| 三击 | 选中整行 |
| 左键拖拽 | 扩展选择；拖到上下边缘时自动滚动 |
| 滚轮 | 滚动视口（光标位置不变），每格 3 行 |
| 右键单击 | 打开上下文菜单：剪切 / 复制 / 粘贴 / 全选 / 查找选中内容 |
| 点击菜单栏项 | 打开下拉菜单 |
| 点击下拉项 | 执行命令 |
| 点击垂直滚动条轨道 | 上/下翻页 |
| 拖拽垂直滚动条滑块 | 滚动到任意位置 |
| 点击水平滚动条轨道 | 左/右平移半页（仅横滚模式） |
| 拖拽水平滚动条滑块 | 平移到任意水平位置（仅横滚模式） |

### 界面布局

```
┌─ 菜单栏 ──────────────────────────────────────────┐  行 0
│ File(F)  Edit(E)  Search(S)  View(V)  Help(H)        │
├─ 编辑区 ──────────────────────────────────── │垂│  │  行 1…N-2
│ （文件内容，行号可选）                       │直│  │
│                                             │滚│  │
│                                             │条│  │
├─ 水平滚动条（仅 WRAP_NONE 模式）─────────────┤ ├──│  行 N-2
│ [======#####=================================]  │+│  │
├─ 状态栏 ────────────────────────────────────┴─┤  │  行 N-1
│  文件名  Ln:1 Col:1 / 42 行  INS  NWRP          │
└──────────────────────────────────────────────────┘
```

- **垂直滚动条**（最右列）：`|` = 轨道，`#` = 滑块。始终可见。
- **水平滚动条**（倒数第二行，仅 WRAP_NONE）：`=` = 轨道，`#` = 滑块。`+` 为两滚动条交汇角。

---

## 项目结构

```
TextEditor/
├── src/
│   ├── main.c            # 事件循环、键盘与鼠标分发、入口
│   ├── editor.c          # 光标、选择、文本编辑、滚动、语法上下文
│   ├── document.c        # 行数组、撤销重做、编码字段
│   ├── display.c         # 双缓冲 Unicode 字符单元屏幕层
│   ├── viewport.c        # 布局、Tab 感知坐标映射、滚动条、语法渲染
│   ├── input.c           # 归一化输入事件（键盘 + 鼠标 + 滚轮）
│   ├── menu.c            # 菜单栏渲染、下拉导航、命令
│   ├── dialog.c          # 模态对话框：输入、确认、查找、替换、帮助、
│   │                     #   右键上下文菜单
│   ├── search.c          # 增量查找与替换（前向/后向）
│   ├── clipboard.c       # 内部剪贴板（跨平台）
│   ├── config.c          # INI 配置文件解析（config.ini）
│   ├── util.c            # 内存、字符串、路径工具
│   ├── utf8.c            # UTF-8 编解码、宽度计算、码点光标移动
│   ├── encoding.c        # GBK/UTF-8 自动检测与转换（Windows API）
│   ├── regex_simple.c    # 轻量 NFA 正则：. * + ? [] \d\w\s ^ $ \b
│   ├── syntax.c          # INI 驱动的语法高亮引擎
│   └── platform/
│       ├── platform.h        # 平台抽象接口
│       ├── platform_win.c    # Windows Console API（Unicode、滚轮）
│       ├── platform_nix.c    # POSIX termios + ANSI 转义
│       └── platform_dos.c    # DOS INT 10h/16h（骨架）
├── include/              # 公共头文件（types.h、editor.h …）
│   ├── types.h           # Cell、WrapMode、颜色/属性宏
│   ├── editor.h          # Editor 结构（含 SyntaxContext）、完整 API
│   ├── document.h        # Document 结构（含 encoding）、完整 API
│   ├── viewport.h        # Viewport、Selection、viewport_render（+ SyntaxContext）
│   ├── syntax.h          # SyntaxDef、SyntaxRule、SyntaxContext、高亮 API
│   ├── encoding.h        # FileEncoding、encoding_detect、gbk↔utf8 转换
│   ├── regex_simple.h    # regex_match / regex_search
│   └── utf8.h            # utf8_decode/encode/cp_width/byte_to_col …
├── syntax/               # 外部语法高亮规则（INI 格式）
│   ├── c.ini             # C/C++ 关键字、字符串、注释、预处理器
│   ├── python.ini        # Python 关键字、字符串、注释、装饰器
│   ├── markdown.ini      # Markdown 标题、粗体/斜体、代码块、链接
│   ├── javascript.ini    # JavaScript/TypeScript 关键字、模板、装饰器
│   ├── html.ini          # HTML/XML/SVG 标签、属性、实体、注释
│   ├── css.ini           # CSS/SCSS/Less 选择器、属性、十六进制颜色
│   ├── json.ini          # JSON 关键字、字符串；jsonc 块/行注释
│   └── batch.ini         # Batch 批处理脚本关键字、echo、set、if、for
├── tests/                # 约 303 个单元测试，零第三方
│   ├── test_runner.c     # 测试框架入口与断言
│   ├── test_document.c
│   ├── test_clipboard.c
│   ├── test_search.c
│   ├── test_viewport.c   # 坐标映射 + 垂直/水平滚动条命中测试
│   ├── test_util.c
│   ├── test_config.c
│   ├── test_editor.c     # scroll_view、sel_word、sel_line、find_selected 流程
│   ├── test_utf8.c       # UTF-8 编解码/宽度/光标移动（18 用例）
│   ├── test_encoding.c   # encoding_detect + GBK↔UTF-8 往返（10 用例）
│   ├── test_syntax.c     # highlight_line：关键字/注释/字符串/数字/优先级
│   └── mocks/
│       └── mock_display.c    # 显示层占位，供无头测试构建
├── docs/                 # 项目文档（规范、需求、测试）
├── CMakeLists.txt
├── cmake_build.bat       # Windows 快速构建 / 测试 / 清理 / 重建
└── config.ini            # 运行时配置（换行模式、行号等）
```

---

## 配置（config.ini）

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `wrap_mode` | `0` | `0` = 字符换行，`1` = 水平滚动 |
| `show_lineno` | `false` | 在编辑区显示行号 |
| `tab_size` | `4` | 制表位宽度（仅显示；文件中仍存为 `\t`） |
| `max_file_size` | `524288` | 可打开的最大文件大小（字节） |
| `syntax_ext_<ext>` | —— | 自定义扩展名语法映射，如 `syntax_ext_xyz=c.ini` |

---

## 语法高亮

规则从可执行文件同目录的 `syntax/<语言>.ini` 文件加载。编辑器根据文件扩展名自动匹配；若无匹配则禁用高亮。

### 规则类型

| 类型 | 说明 |
|------|------|
| `keyword` | 词边界关键字列表 |
| `string` | 引号字符串字面量（可配置分隔符与转义） |
| `line_comment` | 从前缀到行尾（如 `//`、`#`） |
| `block_comment` | 多行定界块（如 `/* … */`） |
| `number` | 整数、十六进制（`0x…`）、浮点数，可带后缀 |
| `line_start` | 若行以指定前缀开头则整行匹配（如 `#include`） |
| `regex` | 使用内置轻量正则引擎的自定义模式 |

### 支持的正则特性

`. * + ? [abc] [a-z] [^…] \d \w \s ^ $ \b`

### INI 示例片段

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

在 `syntax/` 中新增或编辑 INI 文件即可支持新语言 — **无需重新编译**。

---

## 路线图

- [x] Phase 1：编辑、文件 I/O、菜单、基础鼠标（单击 + 拖拽）
- [x] Phase 2：选择、剪贴板、查找替换、撤销重做
- [x] Phase 3：换行/滚动模式、行号、状态栏
- [x] Phase 4：完整鼠标支持（滚轮、双击/三击、右键菜单、滚动条）、水平滚动条
- [x] Phase 5：UTF-8/GBK 多字节编辑、宽字符显示、码点感知光标
- [x] Phase 6：Tab 展开、GBK 自动检测与转换、轻量正则、INI 驱动语法高亮（C/Python/Markdown）
- [x] Phase 7：状态栏编码/语言指示、语法增量刷新、手动切换语言对话框、更多语法 INI（JavaScript、HTML、CSS、JSON、Batch）
- [x] Phase 8：TUI 目录浏览文件选择器（打开/保存）、路径快速跳转、可配置扩展名语法映射、默认文件名 Untitle

---

## 文档

| 文档 | 中文 | English |
|------|------|---------|
| 代码结构 | [代码文件结构](docs/代码文件结构.md) | [CODE_STRUCTURE](docs/CODE_STRUCTURE.md) |
| 功能需求 | [功能需求文档](docs/功能需求文档.md) | [FUNCTIONAL_REQUIREMENTS](docs/FUNCTIONAL_REQUIREMENTS.md) |
| 功能测试规范 | [功能测试规范](docs/功能测试规范.md) | [FUNCTIONAL_TEST_SPEC](docs/FUNCTIONAL_TEST_SPEC.md) |
