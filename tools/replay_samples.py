#!/usr/bin/env python3
"""Read-only candidate replay position summary, never an official timing or integrity verdict."""
import argparse
import json
import math
from pathlib import Path
import struct
from replay_header import inspect, MAX_BYTES, BLOCK


def summarize(data):
    header = inspect(data)
    first = data.index(b'\0', 24, 280) + 13
    count = header['record_blocks']

    def position(index):
        # Terminal record semantics are unknown: exclude even when nonzero.
        if not 0 <= index < count - 1:
            return None
        xyz = struct.unpack_from('<3f', data, first + index * BLOCK + 44)
        return list(xyz) if all(math.isfinite(x) for x in xyz) else None

    low, high = [math.inf] * 3, [-math.inf] * 3
    finite = 0
    for i in range(count - 1):
        xyz = position(i)
        if xyz is None:
            continue
        finite += 1
        for axis, value in enumerate(xyz):
            low[axis] = min(low[axis], value)
            high[axis] = max(high[axis], value)
    markers = {key: {'record_index': header[field], 'xyz': position(header[field])}
               for key, field in [('start_header', 'start_ticks'),
                                  ('checkpoint1_header', 'checkpoint1_ticks'),
                                  ('finish_header', 'finish_ticks')]}
    return {'schema': 1, 'scope': 'candidate_replay_positions', 'header': header,
            'sample_period_ms_candidate': 10, 'coordinate_units': 'unknown',
            'nonterminal_records': count - 1, 'finite_position_records': finite,
            'nonfinite_position_records': count - 1 - finite,
            'bounds_xyz': {'min': low, 'max': high} if finite else None,
            'markers': markers,
            'checkpoint1_elapsed_ms_candidate': (header['checkpoint1_ticks'] - header['start_ticks']) * 10,
            'terminal_record_excluded': True, 'geometry_crossings_verified': False}


def summarize_file(path):
    with Path(path).open('rb') as handle:
        return summarize(handle.read(MAX_BYTES + 1))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('file', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(summarize_file(args.file), indent=2, allow_nan=False))
    except (ValueError, OSError) as exc:
        parser.exit(2, str(exc) + '\n')


if __name__ == '__main__':
    main()
