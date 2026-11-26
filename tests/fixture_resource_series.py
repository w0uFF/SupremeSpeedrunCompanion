"""Bounded process-memory samples from a selected-frame capture fixture."""
from dataclasses import dataclass
import re


@dataclass(frozen=True)
class Sample:
    frame: int
    wall_us: int
    peak_bytes: int
    working_bytes: int
    private_bytes: int
    queue_high: int


def expected_frames(count):
    if type(count) is not int or not 120 <= count <= 1200 or count % 120:
        raise ValueError('unsupported fixture count')
    return (1, *range(120, count+1, 120))


def parse(text, count, final_peak, final_high):
    expected = expected_frames(count)
    lines = [line for line in text.splitlines() if line.startswith('LiveResourceSample ')]
    if len(lines) != len(expected):
        raise ValueError('resource sample count')
    result = []
    for line, frame in zip(lines, expected):
        match = re.fullmatch(r'LiveResourceSample ([0-9]{1,12}) ([0-9]{1,12}) ([0-9]{1,12}) ([0-9]{1,12}) ([0-9]{1,12}) ([0-9]{1,12})', line)
        if match is None:
            raise ValueError('invalid resource sample')
        s = Sample(*map(int, match.groups()))
        if (s.frame != frame or s.wall_us > 90_000_000 or not 0 < s.working_bytes <= s.peak_bytes <= final_peak
                or not 0 < s.private_bytes <= 2**32 or not 0 <= s.queue_high <= final_high <= 8):
            raise ValueError('resource sample bounds')
        if result and (s.wall_us <= result[-1].wall_us or s.peak_bytes < result[-1].peak_bytes
                       or s.queue_high < result[-1].queue_high):
            raise ValueError('resource sample ordering')
        result.append(s)
    return tuple(result)
