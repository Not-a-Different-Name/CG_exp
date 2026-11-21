"""Record reproducible feature clips and verify their encoded metadata with ffprobe."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT/'build/Release/viewer.exe')
    parser.add_argument('--out', type=Path, default=ROOT/'report-assets/review/video')
    args = parser.parse_args()
    if not all(shutil.which(tool) for tool in ('ffmpeg', 'ffprobe')):
        parser.error('ffmpeg and ffprobe must be on PATH; run tools/setup_dependencies.ps1 -InstallFfmpeg and reopen the terminal')
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    cases = {
        'robot-wave': (4, ['--pose', 'wave']),
        'robot-path': (10, ['--path', '--path-guide']),
        'animals': (16, []),
        'chase': (8, ['--chase']),
    }
    report = []
    for name, (seconds, flags) in cases.items():
        target = out/(name+'.mp4')
        command = [str(args.exe.resolve()), '--record', str(target), '--anim', 'none',
                   '--seconds', str(seconds), '--fps', '24', '--size', '960x600', *flags]
        result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=180)
        log = (result.stdout+result.stderr).decode('utf-8', errors='replace')
        (out/(name+'.log')).write_text(log, encoding='utf-8')
        if result.returncode or '[gl] error:' in log or 'video written:' not in log:
            raise RuntimeError(f'{name}: recording failed: {log}')
        metadata = json.loads(subprocess.check_output([
            'ffprobe', '-v', 'error', '-show_entries',
            'stream=codec_name,width,height,r_frame_rate,nb_frames:format=duration',
            '-of', 'json', str(target)], text=True))
        stream = metadata['streams'][0]
        expected = dict(codec_name='h264', width=960, height=600,
                        r_frame_rate='24/1', nb_frames=str(seconds*24))
        if any(stream.get(k) != v for k, v in expected.items()):
            raise RuntimeError(f'{name}: unexpected metadata: {metadata}')
        if abs(float(metadata['format']['duration'])-seconds) > 0.05:
            raise RuntimeError(f'{name}: unexpected duration: {metadata}')
        # Sample the beginning, midpoint and final second for visual review.
        for label, timestamp in [('first', 0), ('middle', seconds/2), ('last', seconds-1)]:
            subprocess.run(['ffmpeg', '-v', 'error', '-ss', str(timestamp), '-i', str(target),
                            '-frames:v', '1', '-y', str(out/(name+'-'+label+'.png'))], check=True)
        report.append(dict(case=name, seconds=seconds, stream=stream, command=command))
        print('PASS', name, stream['nb_frames'], 'frames', flush=True)
    (out/'results.json').write_text(json.dumps(report, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
