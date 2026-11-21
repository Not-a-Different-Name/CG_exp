"""Build a portable Windows x64 binary + offline VS source distribution."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile
from datetime import date

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--vs', type=Path, help='VS installation root; detected with vswhere by default')
    parser.add_argument('--name', default='CG-Experiment-ReadyToRun-VS-'+date.today().strftime('%Y%m%d'))
    args = parser.parse_args()
    if Path(args.name).name != args.name or any(c in args.name for c in '/\\:'):
        parser.error('--name must be a single filename component')
    if args.vs is None:
        import os
        vswhere = Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
        args.vs = Path(subprocess.check_output([str(vswhere), '-latest', '-products', '*',
            '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
            '-property', 'installationPath'], text=True).strip())
    if not (args.vs/'MSBuild/Current/Bin/MSBuild.exe').is_file():
        parser.error('VS C++ tools were not found; specify --vs explicitly')
    subprocess.run([str(args.vs/'MSBuild/Current/Bin/MSBuild.exe'),
                    str(ROOT/'viewer.sln'), '/t:Build', '/p:Configuration=Release',
                    '/p:Platform=x64', '/verbosity:minimal', '/nologo'], check=True)
    redists = sorted((args.vs/'VC/Redist/MSVC').glob('[0-9]*/x64/Microsoft.VC143.CRT'),
                     key=lambda p: tuple(map(int, p.parents[1].name.split('.'))))
    if not redists:
        raise RuntimeError('Microsoft redistributable CRT files were not found')
    output = ROOT/'dist'
    output.mkdir(exist_ok=True)
    archive = output/(args.name+'.zip')
    with tempfile.TemporaryDirectory(prefix='cg-share-') as temp:
        package = Path(temp)/'CG-Experiment'
        package.mkdir()
        for name in ['src', 'Models', 'third_party', 'tests', 'tools', 'docs', 'report-assets']:
            shutil.copytree(ROOT/name, package/name,
                            ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
        if (ROOT/'video').is_dir():
            shutil.copytree(ROOT/'video', package/'video')
        for name in ['viewer.sln', 'viewer.vcxproj', 'viewer.vcxproj.filters',
                     'CMakeLists.txt', 'CMakePresets.json', 'build.bat', 'build.sh',
                     'README.md', 'CG-Experiment-code.md', '.gitignore', '.gitattributes']:
            shutil.copy2(ROOT/name, package/name)
        for name in ['viewer.exe', 'freeglut.dll']:
            shutil.copy2(ROOT/'bin/x64/Release'/name, package/name)
        for dll in redists[-1].glob('*.dll'):
            shutil.copy2(dll, package/dll.name)
        # Preserve images linked by README without shipping old raw captures/logs.
        for image in (ROOT/'shots').rglob('*.png'):
            destination = package/image.relative_to(ROOT)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(image, destination)
        for name, options in [('Start-Forest.bat', '--scene forest'),
                              ('Start-Bunny-Hop.bat', '--scene forest --anim roam'),
                              ('Start-Studio.bat', '--scene studio')]:
            (package/name).write_bytes((
                '@echo off\r\ncd /d "%~dp0"\r\n'
                'start "" "%~dp0viewer.exe" '+options+'\r\n').encode('ascii'))
        (package/'START-HERE.txt').write_text('''CG-Experiment 运行与编译说明

请先完整解压，再运行；不要在 ZIP 压缩包内部直接双击 exe。

一、直接运行（无需安装 Visual Studio / Python / CMake）
双击 viewer.exe 或 Start-Forest.bat：打开森林场景，机器人从林间小径开始巡游。
双击 Start-Bunny-Hop.bat：兔子自动蹦跳漫游。
双击 Start-Studio.bat：打开课程展示台。
请保留 exe 旁边的 DLL 文件和 Models 文件夹。

空格：播放/暂停兔子动画；手动操作会停止动画。
右键拖动/WASD/QE：相机；左键拖动：物体变换。
F2：增强/经典渲染；F3：阴影；H：操作页，点页签或按 1/2/3 切换。Esc 返回场景或退出。
机器人：Shift+P 暂停/继续巡游，Shift+B 显示路径，Shift+G 追赶；n 换姿势，b 原地走/停。
Tab 选 Robot，按 t/r/y 后左键拖动做整体平移/旋转/缩放。
i 导入 64×64 Classic PNG 皮肤或 Y-up OBJ；Shift+I 导入 Slim 皮肤。
k 在内置机器人、已加载的 PNG 和 OBJ 间切换；样例在 Models/TrailSurveyor/。

运行环境：Windows 10/11 64 位，正常安装的显卡驱动。
包内已带 freeglut 和微软可再分发的 VC++ 运行库。
普通运行、蹦跳动画、截图不需要 ffmpeg；仅导出 MP4 需要另装 ffmpeg 并加入 PATH。

二、在 Visual Studio 中编译
安装 Visual Studio 2022，并勾选“使用 C++ 的桌面开发”、MSVC v143 和 Windows SDK。
双击本目录 viewer.sln，选择 Release / x64，右键 viewer 设为启动项目，按 F5。
全部项目依赖已附带，不需要下载第三方库或配置绝对路径。
也提供 Debug 和 Win32 配置；编译产物在 bin/<平台>/<配置>/viewer.exe。
根目录 viewer.exe 是此次预编译版，修改源码后请运行 VS 新生成的程序。
可选 CMake 构建方式见 README.md；课程参考资料在 docs/。

三、文件说明
src/：完整渲染器源码；Models/：兔子模型和贴图。
third_party/：freeglut 的头文件、Win32/x64 库及 DLL，以及 stb_image。
tests/ 和 tools/：自检、截图验证及打包脚本。
MANIFEST.sha256：包内文件校验值。
未包含个人 VS 配置、编译缓存或本机 SSH 等配置。
''', encoding='utf-8-sig')
        manifest = []
        for file in sorted(package.rglob('*')):
            if file.is_file():
                manifest.append(hashlib.sha256(file.read_bytes()).hexdigest()+
                                '  '+file.relative_to(package).as_posix())
        (package/'MANIFEST.sha256').write_text('\n'.join(manifest)+'\n', encoding='utf-8')
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for file in sorted(package.rglob('*')):
                if file.is_file():
                    z.write(file, file.relative_to(Path(temp)).as_posix())
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix('.zip.sha256').write_text(digest+'  '+archive.name+'\n', encoding='ascii')
    print(json.dumps({'archive': str(archive), 'bytes': archive.stat().st_size,
                      'sha256': digest}, indent=2))


if __name__ == '__main__':
    main()
