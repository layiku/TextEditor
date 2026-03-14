@echo off
:: ============================================================
:: cmake_build.bat — 使用 CMake + Visual Studio 2026 构建
:: 用法：
::   cmake_build          配置 + 构建主程序
::   cmake_build test     配置 + 构建 + 运行所有测试
::   cmake_build clean    删除构建目录
::   cmake_build rebuild  清理后重新构建
:: ============================================================
setlocal

set BUILD_DIR=cmake-build
set CONFIG=Release

if "%1"=="clean"   goto :clean
if "%1"=="rebuild" goto :rebuild
if "%1"=="test"    goto :test

:: ---- 默认：配置 + 构建主程序 ----
:build
call :do_configure
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :do_build edit
exit /b %ERRORLEVEL%

:: ---- 测试：构建 + 运行 ----
:test
call :do_configure
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :do_build test_runner
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo.
echo [运行] 执行单元测试...
"%BUILD_DIR%\bin\%CONFIG%\test_runner.exe"
exit /b %ERRORLEVEL%

:: ---- 清理 ----
:clean
echo [清理] 删除 %BUILD_DIR%\...
if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
echo [完成]
exit /b 0

:: ---- 重建 ----
:rebuild
if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
goto :build

:: ==== 子程序 ====

:do_configure
if not exist %BUILD_DIR% (
    echo [CMake] 配置项目（生成器: Visual Studio 18 2026, x64）...
    cmake -S . -B %BUILD_DIR% -G "Visual Studio 18 2026" -A x64
    if %ERRORLEVEL% neq 0 (
        echo [失败] CMake 配置出错。
        exit /b %ERRORLEVEL%
    )
)
exit /b 0

:do_build
echo [CMake] 构建目标: %1（%CONFIG%）...
cmake --build %BUILD_DIR% --config %CONFIG% --target %1
if %ERRORLEVEL% neq 0 (
    echo [失败] 构建出错，请查看上方错误信息。
    exit /b %ERRORLEVEL%
)
echo [成功] 输出: %BUILD_DIR%\bin\%CONFIG%\%1.exe
exit /b 0
