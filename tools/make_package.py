#!/usr/bin/env python3
"""把全部源代码打包成一份 Markdown 文件（方便贴进实验报告附录或作为单一文件提交）。

用法:  python tools/make_package.py [输出路径]
默认输出: CG-Experiment-code.md
"""
import io
import os
import sys

# 按「自底向上」的阅读顺序排列，而不是字母序：
# 先公共类型和数学工具，再数据层（解析/网格/纹理），再渲染层，最后主流程。
FILES = [
    ("src/gl_api.h", "cpp", "OpenGL runtime entry points"),
    ("src/shaders.h", "cpp", "GLSL per-pixel lighting and PCF shadows"),
    ("src/renderer.h", "cpp", "Renderer interface"),
    ("src/renderer.cpp", "cpp", "Shadow pass and forward shading"),
    ("tests/obj_loader_test.cpp", "cpp", "OBJ regression tests"),
    ("tools/render_smoke.py", "python", "GPU rendering regression checks"),
    ("CMakeLists.txt",        "cmake",  "构建配置（含 32 位防护）"),
    ("CMakePresets.json",     "json",   "CMake 预设：只提供 x64，避免选错"),
    ("build.bat",             "bat",    "一键构建（Windows cmd / PowerShell）"),
    ("build.sh",              "bash",   "一键构建（git-bash / MSYS2）"),
    ("viewer.vcxproj",        "xml",    "经典 VS 工程（自包含，双击 viewer.sln 即可）"),
    (".gitignore",            "text",   "Git 忽略规则"),

    ("src/types.h",           "cpp",    "全局模式枚举"),

    ("src/math3d.h",          "cpp",    "向量 / 矩阵 / 四元数工具（header-only）"),

    ("src/obj_loader.h",      "cpp",    "OBJ 导入接口（实验十）"),
    ("src/obj_loader.cpp",    "cpp",    "OBJ / MTL 解析：四边形三角化 + 顶点去重"),

    ("src/mesh.h",            "cpp",    "网格接口"),
    ("src/mesh.cpp",          "cpp",    "网格：归一化、上轴判别、客户端顶点数组绘制"),

    ("src/texture.h",         "cpp",    "纹理接口"),
    ("src/texture.cpp",       "cpp",    "stb_image → OpenGL 纹理"),

    ("src/camera.h",          "cpp",    "观察变换相机接口"),
    ("src/camera.cpp",        "cpp",    "相机：环绕 / 漫游 / 四元数旋转 / 退化保护（实验七）"),

    ("src/projection.h",      "cpp",    "投影接口"),
    ("src/projection.cpp",    "cpp",    "透视 / 正交 / 斜平行投影 + 保宽高比"),

    ("src/lighting.h",        "cpp",    "光照接口"),
    ("src/lighting.cpp",      "cpp",    "光照与材质（展示台点光源 / 森林太阳光）"),

    ("src/forest.h",          "cpp",    "程序化森林接口"),
    ("src/forest.cpp",        "cpp",    "森林：地面 + 三种树型，烘焙成一个顶点色网格"),

    ("src/scene.h",           "cpp",    "场景与物体接口"),
    ("src/scene.cpp",         "cpp",    "两套场景：展示台（实验四/五）与森林"),

    ("src/anim.h",            "cpp",    "脚本动画接口"),
    ("src/anim.cpp",          "cpp",    "动画（时间的纯函数）：自转/环绕/投影/变换/森林漫步"),

    ("src/recorder.h",        "cpp",    "录制接口"),
    ("src/recorder.cpp",      "cpp",    "逐帧数据管道进 ffmpeg 编码 mp4"),

    ("src/hud.h",             "cpp",    "叠加文字接口"),
    ("src/hud.cpp",           "cpp",    "屏幕叠加文字与帮助面板"),

    ("src/input.h",           "cpp",    "输入接口"),
    ("src/input.cpp",         "cpp",    "键盘 / 鼠标 / 滚轮 / 右键菜单（实验六）"),

    ("src/app.h",             "cpp",    "程序状态接口"),
    ("src/app.cpp",           "cpp",    "渲染主流程、HUD、截图、四视口、录制调度"),

    ("src/main.cpp",          "cpp",    "入口：GLUT 初始化、命令行解析、回调注册、自检"),

    ("tools/bmp2png.py",      "python", "截图 BMP → PNG（纯标准库）"),
    ("tools/make_shots.sh",   "bash",   "一键重新生成 shots/ 全部截图"),
    ("tools/make_package.py", "python", "本脚本：生成这份代码文档"),
]


# 完整包：两条构建路径都有（VS 工程 + CMake/脚本）
ZIP_ITEMS = [
    "src", "third_party", "tools", "tests", "Models", "shots", "video",
    "CMakeLists.txt", "CMakePresets.json", "build.bat", "build.sh",
    "viewer.sln", "viewer.vcxproj", "viewer.vcxproj.filters",
    "README.md", "CG-Experiment-code.md", ".gitignore",
]

# 纯 VS 包：去掉 CMake 那套工具链，只留「双击 viewer.sln → F5」需要的东西。
# 注意 viewer.vcxproj 里已经不引用 CMakeLists.txt 了，所以这个包里不会出现失效项。
ZIP_ITEMS_VS = [
    "src", "third_party", "tools", "tests", "Models", "shots", "video",
    "viewer.sln", "viewer.vcxproj", "viewer.vcxproj.filters",
    "README.md", "CG-Experiment-code.md",
]


def build_zip(root, out_path, items=None):
    """用 Python 的 zipfile 而不是 PowerShell 的 Compress-Archive。

    后者在 Windows PowerShell 5.1 上会把路径分隔符写成反斜杠（`src\\app.h`），
    而 zip 规范要求正斜杠 —— Linux/macOS 的 unzip 会把这些文件解成一个名字里
    带反斜杠的怪文件。zipfile 永远写 `/`。
    """
    import zipfile
    count = 0
    with zipfile.ZipFile(out_path, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for item in (items if items is not None else ZIP_ITEMS):
            full = os.path.join(root, item.replace("/", os.sep))
            if os.path.isfile(full):
                z.write(full, item)
                count += 1
            elif os.path.isdir(full):
                for dirpath, dirnames, filenames in os.walk(full):
                    dirnames.sort()
                    for fn in sorted(filenames):
                        p = os.path.join(dirpath, fn)
                        rel = os.path.relpath(p, root).replace(os.sep, "/")
                        z.write(p, rel)
                        count += 1
    return count


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "CG-Experiment-code.md")

    present = []
    total_lines = 0
    for rel, lang, desc in FILES:
        full = os.path.join(root, rel.replace("/", os.sep))
        if not os.path.exists(full):
            print("  跳过（不存在）: %s" % rel)
            continue
        with io.open(full, encoding="utf-8") as f:
            text = f.read()
        lines = text.count("\n") + (0 if text.endswith("\n") else 1)
        total_lines += lines
        present.append((rel, lang, desc, text, lines))

    readme = ""
    readme_path = os.path.join(root, "README.md")
    if os.path.exists(readme_path):
        with io.open(readme_path, encoding="utf-8") as f:
            readme = f.read()

    w = io.open(out_path, "w", encoding="utf-8", newline="\n")

    w.write("# 三维模型查看器 —— 完整源代码\n\n")
    w.write("计算机图形学总结实验（实验四 / 五 / 六 / 七 / 十 + 斜平行投影 + 程序化森林）\n\n")
    w.write("用 **OpenGL + GLSL** 实现逐像素光照与 PCF 阴影，保留 GLUT 与经典管线对照。\n\n")
    w.write("---\n\n")

    w.write("## 统计\n\n")
    w.write("| 项 | 值 |\n|---|---|\n")
    w.write("| 源文件 | %d 个 |\n" % len(present))
    w.write("| 代码总行数 | %d 行（不含本页的 README 部分）| \n" % total_lines)
    w.write("| 模型 | `Models/bunny_10k.obj`（Stanford Bunny，10000 面，Y-up）|\n")
    w.write("| 依赖 | 已 vendor：freeglut（x64）+ stb_image v2.30，**离线可构建** |\n\n")

    w.write("## 构建与运行\n\n")
    w.write("```bash\n")
    w.write('CM="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/'
            'CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"\n')
    w.write('"$CM" -S . -B build -G "Visual Studio 17 2022" -A x64\n')
    w.write('"$CM" --build build --config Release\n')
    w.write("./build/Release/viewer.exe\n")
    w.write("```\n\n")
    w.write("> CMake 不在 PATH 里，要用 VS 自带的那份；`freeglut.dll` 构建时会自动拷到 exe 旁。\n\n")

    w.write("## 文件清单\n\n")
    w.write("| # | 文件 | 行数 | 说明 |\n|---|---|---|---|\n")
    for i, (rel, _lang, desc, _text, lines) in enumerate(present, 1):
        w.write("| %d | `%s` | %d | %s |\n" % (i, rel, lines, desc))
    w.write("\n---\n\n")

    w.write("## 源代码\n\n")
    for i, (rel, lang, desc, text, lines) in enumerate(present, 1):
        w.write("### %d. `%s`\n\n" % (i, rel))
        w.write("%s（%d 行）\n\n" % (desc, lines))
        w.write("```%s\n" % lang)
        body = text if text.endswith("\n") else text + "\n"
        w.write(body)
        w.write("```\n\n")

    if readme.strip():
        w.write("---\n\n")
        w.write("## 附：README.md\n\n")
        w.write(readme if readme.endswith("\n") else readme + "\n")

    w.close()
    size = os.path.getsize(out_path)
    print("已生成: %s" % out_path)
    print("  %d 个文件, %d 行代码, %.1f KB" % (len(present), total_lines, size / 1024.0))

    for fname, items, note in [
        ("CG-Experiment.zip",    ZIP_ITEMS,    "完整包（VS 工程 + CMake/脚本两条路径）"),
        ("CG-Experiment-vs.zip", ZIP_ITEMS_VS, "纯 VS 包（只要 viewer.sln，无 CMake）"),
    ]:
        zp = os.path.join(root, fname)
        n = build_zip(root, zp, items)
        print("已生成: %s" % zp)
        print("  %s" % note)
        print("  %d 个条目, %.1f MB" % (n, os.path.getsize(zp) / 1024.0 / 1024.0))


if __name__ == "__main__":
    main()
