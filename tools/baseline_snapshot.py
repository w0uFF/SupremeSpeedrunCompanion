#!/usr/bin/env python3
"""Read-only paired snapshots of known files in a closed disposable game copy."""
import argparse
import hashlib
import json
from pathlib import Path
import stat

CORE = ('Supreme.exe', 'Display_Config.exe', 'winmm.dll', 'Main_Menu.dll',
        'Supreme_Game.dll', 'HMG_Kernel.dll')
SAVES = ('Defaults.txt', 'Control_Config.txt', 'Keyboard_Bindings.bin') + tuple(
    f'{region}_Tracks_{difficulty}_high_times.txt'
    for region in ('Alpine', 'Forest', 'Village')
    for difficulty in ('Easy', 'Medium', 'Hard'))
FILES = CORE + tuple('Saved_Data/' + name for name in SAVES)
MAX_BYTES = 64 * 1024 * 1024
MAX_REPORT = 64 * 1024
SCOPE = 'closed_game_fixed_files'


def linked(path):
    """Reject symlinks and Windows reparse points, including junctions."""
    info = path.lstat()
    return stat.S_ISLNK(info.st_mode) or bool(
        getattr(info, 'st_file_attributes', 0) & 0x400)


def fingerprint(root, name):
    path = root
    try:
        for part in name.split('/'):
            path /= part
            if linked(path):
                return {'status': 'unsupported_link'}
        before = path.stat()
        if not stat.S_ISREG(before.st_mode):
            return {'status': 'unsupported_type'}
        if before.st_size > MAX_BYTES:
            return {'status': 'too_large'}
        digest = hashlib.sha256()
        total = 0
        with path.open('rb') as handle:
            while block := handle.read(1024 * 1024):
                total += len(block)
                if total > MAX_BYTES:
                    return {'status': 'too_large'}
                digest.update(block)
        after = path.stat()
        if (total != before.st_size or before.st_size != after.st_size
                or before.st_mtime_ns != after.st_mtime_ns
                or before.st_ino != after.st_ino):
            return {'status': 'changed_during_read'}
        return {'status': 'present', 'size': total, 'sha256': digest.hexdigest()}
    except FileNotFoundError:
        return {'status': 'missing'}
    except OSError:
        return {'status': 'unreadable'}


def snapshot(root):
    root = Path(root)
    if linked(root) or not root.is_dir():
        raise ValueError('expected a real disposable game directory')
    return {'schema': 1, 'scope': SCOPE, 'files': {
        name: fingerprint(root, name) for name in FILES}}


def validate(report):
    if (not isinstance(report, dict) or type(report.get('schema')) is not int
            or report['schema'] != 1 or report.get('scope') != SCOPE
            or not isinstance(report.get('files'), dict)
            or set(report['files']) != set(FILES)):
        raise ValueError('unsupported snapshot schema or file set')
    for item in report['files'].values():
        if not isinstance(item, dict):
            raise ValueError('invalid file entry')
        status = item.get('status')
        if status == 'present':
            size, digest = item.get('size'), item.get('sha256')
            if (set(item) != {'status', 'size', 'sha256'} or type(size) is not int
                    or not 0 <= size <= MAX_BYTES or not isinstance(digest, str)
                    or len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest)):
                raise ValueError('invalid file fingerprint')
        elif (status not in ('missing', 'unsupported_link', 'unsupported_type', 'too_large',
                             'changed_during_read', 'unreadable') or set(item) != {'status'}):
            raise ValueError('invalid file status')


def compare(before, after):
    validate(before)
    validate(after)
    changes = {}
    for name in FILES:
        a, b = before['files'][name], after['files'][name]
        if a['status'] == b['status'] == 'present':
            changes[name] = 'unchanged' if a == b else 'changed'
        elif a['status'] == b['status'] == 'missing':
            changes[name] = 'absent_both'
        elif a['status'] == 'missing' and b['status'] == 'present':
            changes[name] = 'created'
        elif a['status'] == 'present' and b['status'] == 'missing':
            changes[name] = 'removed'
        else:
            changes[name] = 'unknown'
    return {'schema': 1, 'scope': 'fixed_file_comparison', 'changes': changes,
            'core_observation': ('changed' if any(changes[n] in ('changed', 'created', 'removed') for n in CORE)
                                 else 'unchanged' if all(changes[n] == 'unchanged' for n in CORE)
                                 else 'incomplete'),
            'runtime_verified': False, 'loader_gate_passed': False}


def read_report(path):
    with Path(path).open('rb') as handle:
        raw = handle.read(MAX_REPORT + 1)
    if len(raw) > MAX_REPORT:
        raise ValueError('snapshot report exceeds bound')
    return json.loads(raw)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('snapshot').add_argument('game_root', type=Path)
    diff = sub.add_parser('compare')
    diff.add_argument('before', type=Path)
    diff.add_argument('after', type=Path)
    args = parser.parse_args()
    try:
        report = (snapshot(args.game_root) if args.command == 'snapshot' else
                  compare(read_report(args.before), read_report(args.after)))
        print(json.dumps(report, indent=2))
    except (ValueError, OSError, RecursionError):
        parser.exit(2, 'Invalid or unreadable baseline input.\n')


if __name__ == '__main__':
    main()
