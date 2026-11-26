#!/usr/bin/env python3
"""Inventory extracted, trusted reference game assets only. No game execution."""
import argparse
import json
from pathlib import Path

REGIONS = ('Alpine', 'Forest', 'Village')
DIFFICULTIES = ('Easy', 'Medium', 'Hard')
WEATHER = {'day', 'cloudy', 'night', 'sunset'}


def inspect(root):
    data = root / 'Data'
    tracks = []
    for region in REGIONS:
        for difficulty in DIFFICULTIES:
            path = data / 'Levels' / region / 'Tracks' / difficulty
            weather = sorted(p.name.lower() for p in path.iterdir()
                             if p.is_dir() and p.name.lower() in WEATHER) if path.is_dir() else []
            tracks.append({'track': f'{region.lower()}_{difficulty.lower()}',
                           'asset_path': path.relative_to(root).as_posix(),
                           'resource_config': (path/'Resource_Config.txt').is_file(),
                           'weather_directories': weather})
    def folders(base, marker):
        return sorted(p.name for p in base.iterdir() if p.is_dir() and (p/marker).is_file()) if base.is_dir() else []
    return {'schema': 1, 'scope': 'on_disk_assets_not_runtime_availability', 'tracks': tracks,
            'character_asset_folders': folders(data/'Characters','menu_character_cnf.txt'),
            'board_asset_folders': folders(data/'Boards','menu_board_cnf.txt'),
            'native_capabilities': []}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    print(json.dumps(inspect(parser.parse_args().root), indent=2))
