"""Compare candidate split ticks, never infer matching track or official IGT."""
from replay_header import inspect


def compare(first, second, *, same_track_confirmed=False):
    if same_track_confirmed is not True:
        raise ValueError('caller must establish matching track/difficulty externally')
    a, b = inspect(first), inspect(second)

    def splits(h):
        return {'start_to_cp1': h['checkpoint1_ticks'] - h['start_ticks'],
                'cp1_to_finish': h['finish_ticks'] - h['checkpoint1_ticks'],
                'start_to_finish': h['finish_ticks'] - h['start_ticks']}
    left, right = splits(a), splits(b)
    return {'schema': 1, 'scope': 'candidate_header_split_comparison', 'unit': 'candidate_ticks',
            'first': left, 'second': right, 'second_minus_first': {k: right[k] - left[k] for k in left},
            'identity_verified_by_tool': False, 'official_time_verified': False}
