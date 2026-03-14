cmake_minimum_required(VERSION 3.16)
project(TextEditor C)

# ============================================================
# 全局设置
# ============================================================
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default to Debug when no build type is specified (convenient for development).
# Override with: cmake -DCMAKE_BUILD_TYPE=Release ...
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo)
endif()

# 将编译产物统一放到 build/bin、build/lib，方便管理
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ============================================================
# 编译器选项
# ============================================================
if(MSVC)
    # /utf-8                   源文件与执行字符集均为 UTF-8（含中文注释必须加）
    # /D_CRT_SECURE_NO_WARNINGS 关闭 strncpy 等"不安全"警告
    # /W3                      警告级别 3
    add_compile_options(/utf-8 /W3)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    # GCC / Clang
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ============================================================
# 头文件搜索路径（全局）
# ============================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/include   # types.h, editor.h, document.h ...
    ${CMAKE_SOURCE_DIR}/src       # platform/platform.h
)

# ============================================================
# 平台专属源文件
# ============================================================
if(WIN32)
    set(PLATFORM_SRC src/platform/platform_win.c)
elseif(UNIX OR APPLE)
    set(PLATFORM_SRC src/platform/platform_nix.c)
else()
    # DOS / 其他嵌入式环境
    set(PLATFORM_SRC src/platform/platform_dos.c)
endif()

# ============================================================
# 核心库（主程序和测试共用，排除 main.c / display.c / input.c）
# ============================================================
set(CORE_SRCS
    src/editor.c
    src/document.c
    src/viewport.c
    src/search.c
    src/clipboard.c
    src/config.c
    src/util.c
    src/utf8.c         # Phase 5：UTF-8 工具库
    src/encoding.c     # Phase 6B：GBK/UTF-8 编码检测与转换
    src/regex_simple.c # Phase 6C1：轻量正则引擎
    src/syntax.c       # Phase 6C2：语法高亮引擎
)

# ============================================================
# 目标 1：主程序 edit
# ============================================================
add_executable(edit
    src/main.c
    src/display.c
    src/input.c
    src/menu.c
    src/dialog.c
    ${CORE_SRCS}
    ${PLATFORM_SRC}
)

if(WIN32)
    # shell32: SHGetFolderPath（用户配置目录）
    # user32:  部分 Windows API 依赖
    target_link_libraries(edit PRIVATE shell32 user32)
endif()

# ============================================================
# 目标 2：单元测试 test_runner
# ============================================================
add_executable(test_runner
    tests/test_runner.c
    tests/test_document.c
    tests/test_clipboard.c
    tests/test_search.c
    tests/test_viewport.c
    tests/test_util.c
    tests/test_config.c
    tests/test_editor.c          # editor_scroll_view / sel_word / sel_line
    tests/test_utf8.c            # Phase 5：UTF-8 工具库单元测试
    tests/test_encoding.c        # Phase 6B：编码检测与转换
    tests/test_syntax.c          # Phase 6C：语法高亮引擎
    tests/mocks/mock_display.c   # 替代真实 display.c / platform
    ${CORE_SRCS}
    # 注意：不包含 input.c、display.c、menu.c、platform 文件
    #   - input.c   依赖平台输入函数，测试不调用输入接口
    #   - display.c 由 mock_display.c 替代
    #   - menu.c    依赖 display + input，测试不需要 UI
)

# TEST 宏让 mock_display.c 中的函数实际生效
target_compile_definitions(test_runner PRIVATE TEST)

# 测试代码还需要能找到 tests/ 下的 test_runner.h
target_include_directories(test_runner PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

if(WIN32)
    target_link_libraries(test_runner PRIVATE shell32 user32)
endif()

# ============================================================
# CTest 集成（cmake --build . --target test 或 ctest）
# ============================================================
enable_testing()
add_test(NAME UnitTests COMMAND test_runner)
