"""Refresh or verify SHA-256 hashes of the files staged in Git."""
import argparse
import hashlib
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def staged_hashes():
    entries = subprocess.check_output(['git', 'ls-files', '--stage', '-z'], cwd=ROOT)
    files = {}
    for entry in entries.decode('utf-8').split('\0'):
        if not entry:
            continue
        metadata, name = entry.split('\t', 1)
        mode, oid, stage = metadata.split()
        if stage != '0':
            raise SystemExit(f'Resolve the merge conflict before hashing: {name}')
        if name != 'MANIFEST.sha256' and mode != '160000':
            files[name] = oid
    names = sorted(files)
    blobs = subprocess.check_output(
        ['git', 'cat-file', '--batch'],
        input=''.join(files[name] + '\n' for name in names).encode('ascii'),
        cwd=ROOT,
    )
    cursor = 0
    for name in names:
        end = blobs.index(b'\n', cursor)
        oid, kind, size = blobs[cursor:end].split()
        if oid.decode('ascii') != files[name] or kind != b'blob':
            raise SystemExit(f'Cannot read staged file: {name}')
        start = end + 1
        cursor = start + int(size) + 1
        yield f'{hashlib.sha256(blobs[start:cursor - 1]).hexdigest()}  {name}\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    lines = list(staged_hashes())
    content = ''.join(lines)
    target = ROOT/'MANIFEST.sha256'
    if args.check:
        if not target.is_file() or target.read_text(encoding='utf-8') != content:
            raise SystemExit('FAIL: MANIFEST.sha256 is stale; run tools/update_manifest.py')
        print(f'PASS: {len(lines)} staged files verified')
    else:
        target.write_text(content, encoding='utf-8')
        print(f'Updated: {len(lines)} staged files')


if __name__ == '__main__':
    main()
