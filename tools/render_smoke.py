"""Actual OpenGL integration checks. Run after building; requires a GPU context."""
from pathlib import Path
import argparse, subprocess, sys, struct, json
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('exe', nargs='?', type=Path, default=root/'build/Release/viewer.exe')
parser.add_argument('--out', type=Path, default=root/'out/review/render')
options = parser.parse_args()
exe = options.exe.resolve()
out = options.out.resolve(); out.mkdir(parents=True,exist_ok=True)
cases = {
 'after': ['--no-hud'],
 'classic': ['--classic','--no-hud'],
 'no-shadows': ['--no-shadows','--no-hud'],
 'studio': ['--scene','studio','--no-hud'],
 'quad': ['--scene','studio','--quad'],
 'ortho': ['--scene','studio','--proj','ortho','--view','front'],
 'cavalier': ['--scene','studio','--proj','cav','--view','front'],
 'cabinet': ['--scene','studio','--proj','cab','--view','front'],
 'wire': ['--scene','studio','--render','wire'],
 'points': ['--scene','studio','--render','points'],
 'wide': ['--size','1600x300','--no-hud'],
 'textured': ['Models/Rabbit/10037_Rabbit_v1_L3.obj','--scene','studio','--no-hud'],
 'robot-wave': ['--pose','wave','--no-hud'],
 'robot-walk': ['--robot-walk','--no-hud'],
 'forest-quad': ['--quad'],
 'forest-front': ['--proj','ortho','--view','front'],
 'forest-top': ['--proj','ortho','--view','top'],
 'forest-cavalier': ['--proj','cav','--view','front'],
 'path-guide': ['--path','--path-guide','--no-hud'],
 'chase': ['--chase','--no-hud'],
 'grazing': ['--time','12','--no-hud'],
 'help': ['--show-help'],
 'import-skin': ['--character','Models/TrailSurveyor/TrailSurveyor.png','--pose','wave','--time','0'],
 'import-model': ['--character','Models/TrailSurveyor/TrailSurveyor.obj','--path','--time','0.4'],
}
report=[]
for name,args in cases.items():
 shot=out/(name+'.bmp')
 result=subprocess.run([str(exe),'--shot',str(shot),'--time','3',*args],cwd=root,capture_output=True,timeout=60)
 log=(result.stdout+result.stderr).decode('utf-8',errors='replace')
 (out/(name+'.log')).write_text(log,encoding='utf-8')
 assert result.returncode==0,(name,result.returncode,log)
 assert 'GLSL initialized' in log,(name,log)
 assert '[gl] error:' not in log,(name,log)
 if name == 'import-skin':
  assert '[character] imported .png:' in log,(name,log)
 if name == 'import-model':
  assert '[character] imported .obj:' in log,(name,log)
 data=shot.read_bytes(); w,h=struct.unpack_from('<ii',data,18)
 assert len(data)==struct.unpack_from('<I',data,2)[0] and len(set(data[54:]))>32,name
 subprocess.run([sys.executable,str(root/'tools/bmp2png.py'),str(shot)],check=True,capture_output=True)
 report.append({'case':name,'width':w,'height':h,'exit':result.returncode})
 print('PASS',name,f'{w}x{h}',flush=True)
assert (out/'after.bmp').read_bytes()!=(out/'no-shadows.bmp').read_bytes(),'shadows have no effect'
assert (out/'after.bmp').read_bytes()!=(out/'classic.bmp').read_bytes(),'shader has no effect'
(out/'smoke-results.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
