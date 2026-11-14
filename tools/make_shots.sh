#!/usr/bin/env bash
# 重新生成 shots/ 下实验报告用的全部截图。
# 用法:  bash tools/make_shots.sh
#
# 单帧截图模式会隐藏窗口、从后缓冲读像素，所以批处理不会满屏闪窗口。
#
# 注意：程序默认进的是森林场景，而报告的前 14 张图要的是展示台
# （实验四立方体 / 茶壶 / 球那一套），所以这里每张都显式写 --scene。
set -e
cd "$(dirname "$0")/.."

V=./build/Release/viewer.exe
[ -x "$V" ] || { echo "先构建: 见 README「二、构建」"; exit 1; }

_studio() { "$V" --shot "shots/$1.bmp" --scene studio "${@:2}" >/dev/null 2>&1; }
_forest() { "$V" --shot "shots/$1.bmp" --scene forest "${@:2}" >/dev/null 2>&1; }

emit() {                       # emit <名字> <生成函数> [参数...]
    local name="$1"; shift
    local fn="$1";   shift     # 必须先把函数名取出来再 shift，
                               # 否则 "$@" 里还带着函数名，会被当成第一个参数重复传一遍
    "$fn" "$name" "$@"
    python -X utf8 tools/bmp2png.py "shots/$name.bmp" "shots/$name.png" >/dev/null
    echo "  shots/$name.png"
}

echo "生成截图..."

# --- 展示台场景：投影与绘制模式 ---
emit 01_perspective   _studio --proj persp                    --no-hud
emit 02_front         _studio --proj ortho --view front       --no-hud
emit 03_top           _studio --proj ortho --view top         --no-hud
emit 04_left          _studio --proj ortho --view left        --no-hud
emit 05_cavalier      _studio --proj cav   --view front       --no-hud
emit 06_cabinet       _studio --proj cab   --view front       --no-hud
emit 07_wireframe     _studio --proj persp --render wire      --no-hud
emit 08_points        _studio --proj persp --render points    --no-hud

# --- 四视口 / 帮助界面（保留 HUD）---
emit 09_quadview      _studio --quad
emit 10_help          _studio --show-help

# --- 窗口变化图形不变形：四个极端长宽比 ---
emit 11_wide_1600x300 _studio --proj persp --size 1600x300          --no-hud
emit 12_tall_300x1000 _studio --proj persp --size 300x1000          --no-hud
emit 13_ortho_wide    _studio --proj ortho --view front --size 1600x300 --no-hud
emit 14_ortho_tall    _studio --proj ortho --view front --size 300x1000 --no-hud

# --- 森林场景（程序默认就是它）---
emit 15_forest        _forest                       --no-hud
emit 16_forest_top    _forest --proj ortho --view top --no-hud

rm -f shots/*.bmp
echo "完成。"
