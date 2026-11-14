"""Actual OpenGL integration checks. Run after building; requires a GPU context."""
from pathlib import Path
import subprocess, sys, struct, json
root = Path(__file__).resolve().parents[1]
exe = Path(sys.argv[1]).resolve() if len(sys.argv)>1 else root/'build/Release/viewer.exe'
out = root/'shots/refactor'; out.mkdir(parents=True,exist_ok=True)
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
}
report=[]
for name,args in cases.items():
 shot=out/(name+'.bmp')
 result=subprocess.run([str(exe),'--shot',str(shot),*args],cwd=root,capture_output=True,timeout=60)
 log=(result.stdout+result.stderr).decode('utf-8',errors='replace')
 (out/(name+'.log')).write_text(log,encoding='utf-8')
 assert result.returncode==0,(name,result.returncode,log)
 assert 'GLSL initialized' in log,(name,log)
 data=shot.read_bytes(); w,h=struct.unpack_from('<ii',data,18)
 assert len(data)==struct.unpack_from('<I',data,2)[0] and len(set(data[54:]))>32,name
 subprocess.run([sys.executable,str(root/'tools/bmp2png.py'),str(shot)],check=True,capture_output=True)
 report.append({'case':name,'width':w,'height':h,'exit':result.returncode})
 print('PASS',name,f'{w}x{h}',flush=True)
assert (out/'after.bmp').read_bytes()!=(out/'no-shadows.bmp').read_bytes(),'shadows have no effect'
assert (out/'after.bmp').read_bytes()!=(out/'classic.bmp').read_bytes(),'shader has no effect'
(out/'smoke-results.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
