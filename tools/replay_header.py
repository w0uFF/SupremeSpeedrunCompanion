#!/usr/bin/env python3
"""Conservative, read-only structural probe for supplied track replays.
Not an eligibility check, replay playback, or live IGT source.
Layout evidence: supplied launcher supreme_replay_analyzer/src/analyzer.rs
and SupremeLight reference files; independent bounded implementation.
"""
import argparse
import json
from pathlib import Path
import struct

MAX_BYTES = 8 * 1024 * 1024
BLOCK = 109


def inspect(data):
    if len(data) > MAX_BYTES or len(data) < 38 or data[:4] != b'\x05\0\0\0':
        raise ValueError('unsupported header or size')
    name_end = data.find(b'\0', 24, 24 + 256)
    if name_end <= 24 or name_end + 13 > len(data):
        raise ValueError('missing bounded name terminator or timing header')
    start = name_end + 13
    blocks, remainder = divmod(len(data) - start, BLOCK)
    if not blocks or remainder:
        raise ValueError('incomplete or unsupported record layout')
    recording, race_start, checkpoint, finish = (struct.unpack_from('<H', data, i)[0]
        for i in (4, name_end+1, name_end+5, name_end+9))
    if not 0 <= race_start <= checkpoint <= finish <= recording:
        raise ValueError('unsupported timing order')
    if blocks != recording + 1:
        raise ValueError('unsupported block count')
    return {'schema': 1, 'scope': 'reference_track_replay_header_candidate',
            'recording_ticks': recording, 'start_ticks': race_start,
            'checkpoint1_ticks': checkpoint, 'finish_ticks': finish,
            'candidate_displayed_ms': (finish-race_start)*10,
            'record_blocks': blocks, 'zero_last_block': data[-BLOCK:] == bytes(BLOCK),
            'official_time_verified': False}


def inspect_file(path):
    with Path(path).open('rb') as handle:
        return inspect(handle.read(MAX_BYTES + 1))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('file', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect_file(args.file), indent=2))
    except (OSError, ValueError) as exc:
        print(json.dumps({'status': 'unsupported_or_unreadable', 'reason': str(exc)}))
        return 2
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
