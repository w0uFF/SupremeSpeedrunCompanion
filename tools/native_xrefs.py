#!/usr/bin/env python3
"""Read-only linear-sweep candidate references for trusted local x86 PE files.
Requires GNU objdump for scan; no executable is loaded or run by this tool.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess
from inventory import pe

MAX_BYTES = 8 * 1024 * 1024
IDS = ('ID_MAIN_MENU', 'ID_ARCADE_MENU', 'ID_ARCADE_TIME_ATTACK_SEQUENCE',
       'ID_CHAMPIONSHIP_SEQUENCE', 'ID_PLAY_AGAIN', 'ID_ARCADE_CHOOSE_CHARACTER',
       'ID_ARCADE_CHOOSE_BOARD', 'ID_ARCADE_CHOOSE_TRACK', 'ID_ARCADE_RESULTS',
       'ID_CHAMPIONSHIP_COMPLETED_CEREMONY') + tuple('ID_DAYTIME_SELECTION_' + v for v in
       ('DAY', 'CLOUDY', 'SUNSET', 'NIGHT')) + tuple('ID_' + k + '_SELECTION_' + k + '_' + str(i)
       for k in ('TRACK', 'ENVIRONMENT') for i in range(1, 4))
IMPORTS = {'main_menu.dll', 'supreme_game.dll', 'hmg_kernel.dll'}
LINE = re.compile(r'^\s*([0-9a-f]+):\s+(?:(?:[0-9a-f]{2})\s+)+\s*([a-z][a-z0-9.]*\s*.*)$')
HEX = re.compile(r'(?<![\w])0x[0-9a-f]+(?![\w])')


def sections(data):
    p = struct.unpack_from('<I', data, 60)[0]
    count = struct.unpack_from('<H', data, p + 6)[0]
    offset = p + 24 + struct.unpack_from('<H', data, p + 20)[0]
    if not 0 < count <= 96 or offset + count * 40 > len(data):
        raise ValueError('unsupported section table')
    for i in range(count):
        _, _, rva, size, raw, *_, flags = struct.unpack_from('<8sIIIIIIHHI', data, offset + i * 40)
        if raw + size > len(data):
            raise ValueError('section exceeds file')
        yield rva, raw, size, flags


def parse_lines(text):
    for line in text.splitlines():
        m = LINE.match(line)
        if m:
            instruction = m[2].strip()
            yield int(m[1], 16), instruction, {int(v, 16) for v in HEX.findall(instruction)}


def scan(path, objdump='objdump'):
    path = Path(path).resolve()
    with path.open('rb') as handle:
        data = handle.read(MAX_BYTES + 1)
    if len(data) > MAX_BYTES:
        raise ValueError('binary exceeds research bound')
    try:
        # Validate section bounds before handing trusted reference data to the PE inventory.
        table = list(sections(data))
        info = pe(data)
    except (struct.error, IndexError, UnicodeError) as exc:
        raise ValueError('unsupported PE layout') from exc
    if info['machine'] != '0x14c':
        raise ValueError('expected x86 PE32')
    base = int(info['image_base'], 16)
    targets = {}
    strings = {key: [] for key in IDS}
    for rva, raw, size, flags in table:
        if flags & 0x20000000:
            continue
        area = data[raw:raw + size]
        for key in IDS:
            needle = key.encode() + b'\0'
            start = 0
            while (at := area.find(needle, start)) >= 0:
                start = at + 1
                if at and area[at - 1] in b'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_':
                    continue
                va = base + rva + at
                strings[key].append(hex(va - base))
                targets.setdefault(va, []).append(('string', key))
    for descriptor in info['imports']:
        if descriptor['dll'].lower() not in IMPORTS:
            continue
        for i, name in enumerate(descriptor['symbols']):
            va = base + int(descriptor['iat_rva'], 16) + 4 * i
            targets.setdefault(va, []).append(('iat', descriptor['dll'] + '!' + name))
    output = subprocess.run([objdump, '-d', '-Mintel', str(path)], check=True,
                            capture_output=True, text=True, timeout=60).stdout
    with path.open("rb") as handle:
        unchanged = handle.read(MAX_BYTES + 1) == data
    if not unchanged:
        raise ValueError('reference changed during scan')
    hits = []
    for va, instruction, operands in parse_lines(output):
        for address in sorted(operands & targets.keys()):
            for kind, name in targets[address]:
                hits.append({'instruction_rva': hex(va - base), 'target_rva': hex(address - base),
                             'kind': kind, 'name': name, 'instruction': instruction})
    return {'scope': 'static_linear_sweep_candidates', 'sha256': info['sha256'],
            'image_base': info['image_base'], 'strings': strings, 'references': hits}


def compare(before, after):
    """Set difference by instruction/target RVA and symbol, not instruction text or runtime behavior."""
    def keys(report, kind):
        if report['scope'] != 'static_linear_sweep_candidates':
            raise ValueError('unsupported report scope')
        return {(r['instruction_rva'], r['target_rva'], r['name']) for r in report['references']
                if r['kind'] == kind}
    result = {}
    for kind in ('string', 'iat'):
        old, new = keys(before, kind), keys(after, kind)
        result[kind] = {'before': len(old), 'after': len(new), 'common': len(old & new),
                        'removed': sorted(old - new), 'added': sorted(new - old)}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--compare', type=Path, help='second trusted local PE')
    parser.add_argument('--objdump', default='objdump')
    args = parser.parse_args()
    try:
        first = scan(args.binary, args.objdump)
        if args.compare:
            second = scan(args.compare, args.objdump)
            first = {'before_sha256': first['sha256'], 'after_sha256': second['sha256'],
                     'scope': 'static_candidate_comparison', 'difference': compare(first, second)}
        print(json.dumps(first, indent=2))
    except (ValueError, OSError, subprocess.SubprocessError) as exc:
        parser.exit(2, str(exc) + '\n')


if __name__ == '__main__':
    main()
