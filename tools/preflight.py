#!/usr/bin/env python3
"""Read-only on-disk compatibility classifier. No game code is loaded or executed."""
import argparse
import hashlib
import json
from pathlib import Path

PROFILE = Path(__file__).resolve().parents[1] / 'docs/engine/build-profile.json'


def inspect(root, profile):
    root = Path(root)
    results = {}
    for name, variants in profile['files'].items():
        path = root / name
        try:
            if path.is_symlink():
                results[name] = {'status': 'unreadable', 'reason': 'symbolic link'}
                continue
            if not path.is_file():
                results[name] = {'status': 'missing'}
                continue
            with path.open('rb') as handle:
                h = hashlib.sha256()
                for block in iter(lambda: handle.read(1024 * 1024), b''):
                    h.update(block)
            digest = h.hexdigest()
            variant = next((v for v, expected in variants.items() if expected == digest), None)
            results[name] = {'status': variant or 'unknown', 'sha256': digest}
        except OSError as exc:
            results[name] = {'status': 'unreadable', 'reason': type(exc).__name__}
    core_ok = all(results[name]['status'] == 'reference' for name in profile['engine_files'])
    exe = results['Supreme.exe']['status']
    launcher = results['Display_Config.exe']['status']
    proxy = results['winmm.dll']['status']
    combination = 'unknown_or_incomplete'
    if core_ok and exe == 'original' and launcher == 'bundled' and proxy == 'missing':
        combination = 'supremelight_reference'
    elif core_ok and exe in ('original', 'keyboard_v01') and launcher == proxy == 'display_v180':
        combination = 'display_v180_' + exe
    return {'schema': 1, 'profile_id': profile['id'], 'combination': combination,
            'native_integration': 'blocked_pending_loader_gate', 'engine_capabilities': [],
            'scope': 'on_disk_reference_files_only', 'files': results}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_root', type=Path)
    args = parser.parse_args()
    if not args.game_root.is_dir():
        parser.error('game_root must be an existing directory')
    result = inspect(args.game_root, json.loads(PROFILE.read_text(encoding='utf-8')))
    print(json.dumps(result, indent=2))
    return 2 if result['combination'] == 'unknown_or_incomplete' else 0


if __name__ == '__main__':
    raise SystemExit(main())
