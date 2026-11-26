#!/usr/bin/env python3
"""Reproduce P0-T10 reference reports on extracted, trusted reference directories.
No writes to inputs. Output JSON to stdout; never load native code.
"""
import argparse
import hashlib
import json
from pathlib import Path
from inventory import pe
from replay_header import inspect_file
from reference_assets import inspect as inspect_assets


def scan(game, launcher, proxy):
    template = launcher/'supreme_hook_proxy/winmm_x86.dll'
    a, b = (pe(p.read_bytes()) for p in (template, proxy))
    sa, sb = ({e['name'] for e in x['exports']} for x in (a,b))
    exports = {'template_sha256': a['sha256'], 'proxy_sha256': b['sha256'],
               'template_named_exports': len(sa), 'proxy_named_exports': len(sb),
               'missing': sorted(sa-sb), 'extra_count': len(sb-sa),
               'proxy_has_dllmain': 'DllMain' in sb, 'exact_correspondence_verified': False}
    replays = []
    for p in sorted(game.rglob('*.dat')):
        if p.parent.name.lower() != 'replays':
            continue
        h = hashlib.sha256()
        with p.open('rb') as f:
            for block in iter(lambda: f.read(1024*1024), b''):
                h.update(block)
        item = {'path': p.relative_to(game).as_posix(), 'sha256': h.hexdigest()}
        try:
            item.update(inspect_file(p)); item['status'] = 'layout_match'
        except (ValueError, OSError) as exc:
            item.update(status='unsupported', reason=str(exc))
        replays.append(item)
    files = {name:(game/name).read_bytes() for name in ('Supreme.exe','Main_Menu.dll','Supreme_Game.dll')}
    ids = ('ID_MAIN_MENU','ID_ARCADE_TIME_ATTACK_SEQUENCE','ID_ARCADE_CHOOSE_CHARACTER',
           'ID_ARCADE_CHOOSE_BOARD','ID_ARCADE_CHOOSE_TRACK','ID_ARCADE_RESULTS','ID_PLAY_AGAIN',
           'ID_CHAMPIONSHIP_SEQUENCE','ID_CHAMPIONSHIP_COMPLETED_CEREMONY',
           'ID_DAYTIME_SELECTION_CLOUDY','ID_DAYTIME_SELECTION_DAY','ID_DAYTIME_SELECTION_NIGHT',
           'ID_DAYTIME_SELECTION_SUNSET')
    matches = {name:{k: v.count(name.encode()+b'\0') for k,v in files.items()} for name in ids}
    return {'export_comparison': exports, 'assets': inspect_assets(game), 'replays': replays, 'menu_string_matches':matches}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('game','launcher','proxy'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    print(json.dumps(scan(args.game,args.launcher,args.proxy),indent=2))
