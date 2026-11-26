"""Compare independent extended-HD child evidence, never average away a failure."""
from dataclasses import dataclass
import re
from fixture_resource_series import parse


@dataclass(frozen=True)
class Trial:
    wall_us: int
    cpu_100ns: int
    p95_us: int
    peak_bytes: int
    private_delta: int
    warm_private_delta: int
    output_bytes: int
    queue_high: int
    pts_delta_100ns: int


def fields(text, tag, count):
    lines = [line for line in text.splitlines() if line.startswith(tag + ' ')]
    if len(lines) != 1:
        raise ValueError('missing or duplicate ' + tag)
    match = re.fullmatch(tag + (' ([0-9]{1,12})' * count), lines[0])
    if match is None:
        raise ValueError('invalid ' + tag)
    return tuple(map(int, match.groups()))


def trial(text, expected_count=600):
    if expected_count not in (600, 1200):
        raise ValueError('unsupported HD trial count')
    if len(text) > 256_000:
        raise ValueError('fixture output bound')
    count, wall, cpu, median, p95, p99, maximum, drain = fields(text, 'LivePerformance', 8)
    sw, sh, w, h, peak, working, private, size, bound = fields(text, 'LiveResources', 9)
    n, first, end, coverage, gap, high, drop, discard = fields(text, 'LiveHandoff', 8)
    decoded = re.findall(r'^LiveDecode count=([0-9]+) max_abs_delta_100ns=([0-9]+)$', text, re.M)
    if (count != expected_count or n != count or first != 0 or not (15_000_000 if expected_count == 600 else 30_000_000) <= wall <= 90_000_000
            or not 0 < cpu or not 0 <= median <= p95 <= p99 <= maximum <= wall
            or not 0 <= drain <= wall or not 1280 <= sw <= 1536 or not 720 <= sh <= 1024
            or (w, h) != (1280, 720) or not 0 < working <= peak or private <= 0
            or not 0 < size <= 16*1024*1024 or bound != 8*1280*720*3//2
            or not 0 < coverage < end <= 900_400_000 or gap != end-coverage
            or not 1 <= high <= 8 or drop or discard or len(decoded) != 1
            or int(decoded[0][0]) != count or int(decoded[0][1]) > 20_000):
        raise ValueError('extended HD evidence bounds')
    samples = parse(text, count, peak, high)
    if samples[-1].wall_us > wall:
        raise ValueError('sample after fixture completion')
    # The first sample includes initialization; the 120-frame checkpoint is a
    # descriptive warm-up reference, not a calibrated performance threshold.
    return Trial(wall, cpu, p95, peak, samples[-1].private_bytes-samples[0].private_bytes,
                 samples[-1].private_bytes-samples[1].private_bytes, size, high, int(decoded[0][1]))


def summarize(trials):
    if len(trials) != 2 or any(not isinstance(t, Trial) for t in trials):
        raise ValueError('exactly two successful trials required')
    return {name: (min(getattr(t, name) for t in trials), max(getattr(t, name) for t in trials))
            for name in Trial.__dataclass_fields__}
