"""Read-only fixed-file menu and loader observations; never a gate pass."""
import argparse
import json
from pathlib import Path

from baseline_snapshot import fingerprint, linked

PROFILE = Path(__file__).resolve().parents[1] / 'docs/engine/build-profile.json'
MENU = {
    'Data/Main_Menu/ID_Lists/Button_List.txt': '00296b830ade9ebaeae3eff893283cdcbc7f961cabd6b357e0ca1f0c1f4bebe1',
    'Data/Main_Menu/ID_Lists/Page_List.txt': '42e0145b8488c79c28e509db001102548fbcf90680e422874b297b413418d7c5',
    'Data/Main_Menu/ID_Lists/Sequence_List.txt': 'e8c49309bb9dd987597ef2a3fe48969c9fe36261aaee892fb27650bcf1093cc6',
    'Supreme/Main_Menu/Main_Menu/ID_MAIN_MENU.txt': '6a53a33199d2d0028d13e7c7de7cecb86852e9154677ce2e6d7a86bfcbb3b4dc',
    'Supreme/Main_Menu/Main_Menu/Sequences/ID_ARCADE_TIME_ATTACK_SEQUENCE.txt': '9dde9fec31cf8082691a9694ab9f53b6f0549afc5d92cf5107a5f8f8d809b265',
}
BINARIES = ('Supreme.exe', 'Main_Menu.dll', 'Supreme_Game.dll', 'Display_Config.exe', 'winmm.dll')


def observe(root):
    root = Path(root)
    if linked(root) or not root.is_dir():
        raise ValueError('expected a real closed disposable game directory')
    profile = json.loads(PROFILE.read_text(encoding='utf-8'))
    menu = {name: fingerprint(root, name) for name in MENU}
    for name, item in menu.items():
        if item['status'] == 'present':
            item['reference_match'] = item['sha256'] == MENU[name]
    binaries = {name: fingerprint(root, name) for name in BINARIES}
    for name, item in binaries.items():
        if item['status'] == 'present':
            item['variant'] = next((v for v, digest in profile['files'][name].items()
                                    if digest == item['sha256']), 'unknown')
    proxy = binaries['winmm.dll']
    loader = ('display_v180_reference_only' if proxy.get('variant') == 'display_v180'
              else 'missing' if proxy['status'] == 'missing' else 'unreviewed_or_unavailable')
    return {'schema': 1, 'scope': 'fixed_menu_and_loader_files_only',
            'profile_id': profile['id'], 'menu': menu, 'binaries': binaries,
            'reference_menu_match': all(item.get('reference_match') is True for item in menu.values()),
            'loader_observation': loader, 'loader_provenance_verified': False,
            'native_registration_verified': False, 'runtime_verified': False,
            'loader_gate_passed': False, 'engine_capabilities': []}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_root', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(observe(args.game_root), indent=2))
    except (OSError, ValueError, KeyError):
        parser.exit(2, 'Invalid or unreadable readiness input.\n')


if __name__ == '__main__':
    main()
