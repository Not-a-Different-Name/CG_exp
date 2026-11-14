#!/usr/bin/env bash
# 一键构建（git-bash / MSYS2 / WSL 里跑）：
#
#   bash build.sh          默认 64 位
#   bash build.sh x64      64 位
#   bash build.sh Win32    32 位
#
# CMake 不在 PATH 里也没关系 —— 脚本会去找 Visual Studio 2022 自带的那份。
#
# 输出统一用 ASCII：和 build.bat 保持一致，也避开各种控制台代码页的坑。
# 中文说明都在 README 和源码注释里。
set -e
cd "$(dirname "$0")"

ARCH="${1:-x64}"

# 每个架构用独立的构建目录：CMake 缓存和生成器平台绑死，
# 同一个目录里从 x64 切 Win32 会报 generator platform does not match。
if [ "$ARCH" = "Win32" ] || [ "$ARCH" = "win32" ]; then
    ARCH="Win32"
    BUILDDIR="build32"
else
    ARCH="x64"
    BUILDDIR="build"
fi

CMAKE=""

# 1) 先用 PATH 里的
if command -v cmake >/dev/null 2>&1; then
    CMAKE="$(command -v cmake)"
fi

# 2) 再找 VS2022 自带的（Community / Professional / Enterprise / BuildTools）
if [ -z "$CMAKE" ]; then
    for ed in Community Professional Enterprise Preview BuildTools; do
        cand="/c/Program Files/Microsoft Visual Studio/2022/$ed/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
        if [ -x "$cand" ]; then CMAKE="$cand"; break; fi
    done
fi

if [ -z "$CMAKE" ]; then
    echo
    echo "[ERROR] cmake not found."
    echo
    echo "  Install Visual Studio 2022 with the \"Desktop development with C++\""
    echo "  workload, or put cmake on your PATH and retry."
    echo
    exit 1
fi

# 输出统一用 ASCII：和 build.bat 保持一致，也避开各种控制台代码页的坑。
# 中文说明都在 README 和源码注释里。
echo "Using CMake:  $CMAKE"
echo "Architecture: $ARCH  (build dir: $BUILDDIR)"
echo

if ! "$CMAKE" -S . -B "$BUILDDIR" -G "Visual Studio 17 2022" -A "$ARCH"; then
    echo
    echo "[ERROR] configure failed."
    echo "  If you configured this directory with a different generator or"
    echo "  architecture before, delete $BUILDDIR/CMakeCache.txt and retry."
    exit 1
fi

if ! "$CMAKE" --build "$BUILDDIR" --config Release; then
    echo
    echo "[ERROR] build failed."
    exit 1
fi

echo
echo "============================================================"
echo " Build complete  ($ARCH)"
echo "   executable: $BUILDDIR/Release/viewer.exe"
echo "   run:        ./$BUILDDIR/Release/viewer.exe"
echo "   forest:     ./$BUILDDIR/Release/viewer.exe Models/bunny_10k.obj --scene forest --anim roam"
echo "============================================================"
