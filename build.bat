@echo off
setlocal enabledelayedexpansion
rem ===========================================================================
rem  一键构建（Windows cmd / PowerShell）：双击本文件，或在本目录执行
rem
rem      build.bat            默认编译 64 位
rem      build.bat x64        64 位
rem      build.bat Win32      32 位（指导书默认的「Debug Win32」就是这个）
rem
rem  CMake 不在 PATH 里也没关系 —— 脚本会去找 Visual Studio 2022 自带的那份。
rem
rem  注意：这里所有 echo 输出都用 ASCII。本文件是 UTF-8，而中文 Windows 的
rem  cmd 默认代码页是 GBK，UTF-8 的中文在 GBK 控制台里会变成乱码（而且是位置
rem  相关的：某个字节刚好落在 ASCII 边界上就崩）。注释里留中文不影响执行。
rem ===========================================================================
cd /d "%~dp0"

set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"

rem 每个架构用独立的构建目录：CMake 缓存和生成器平台绑死，
rem 同一个目录里先从 x64 再切 Win32 会报 generator platform does not match。
if /i "%ARCH%"=="Win32" (
    set "BUILDDIR=build32"
) else (
    set "BUILDDIR=build"
)

set "CMAKE="

rem 1) Prefer whatever is on PATH
where cmake >nul 2>nul
if not errorlevel 1 set "CMAKE=cmake"

rem 2) Otherwise look for the copy bundled with Visual Studio 2022
if not defined CMAKE (
    for %%E in (Community Professional Enterprise Preview BuildTools) do (
        if not defined CMAKE (
            set "CAND=C:\Program Files\Microsoft Visual Studio\2022\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if exist "!CAND!" set "CMAKE=!CAND!"
        )
    )
)

if not defined CMAKE (
    echo.
    echo [ERROR] cmake.exe not found.
    echo.
    echo   Install Visual Studio 2022 with the "Desktop development with C++"
    echo   workload, or put cmake on your PATH and retry.
    echo.
    exit /b 1
)

echo Using CMake: %CMAKE%
echo Architecture: %ARCH%   ^(build dir: %BUILDDIR%^)
echo.

"%CMAKE%" -S . -B "%BUILDDIR%" -G "Visual Studio 17 2022" -A %ARCH%
if errorlevel 1 (
    echo.
    echo [ERROR] configure failed.
    echo   If you configured this directory with a different generator or
    echo   architecture before, delete %BUILDDIR%\CMakeCache.txt and retry.
    exit /b 1
)

"%CMAKE%" --build "%BUILDDIR%" --config Release
if errorlevel 1 (
    echo.
    echo [ERROR] build failed.
    exit /b 1
)

echo.
echo ============================================================
echo  Build complete  (%ARCH%)
echo    executable: %BUILDDIR%\Release\viewer.exe
echo    run:        %BUILDDIR%\Release\viewer.exe
echo    forest:     %BUILDDIR%\Release\viewer.exe Models\bunny_10k.obj --scene forest --anim roam
echo ============================================================
endlocal
