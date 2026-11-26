"""Strict per-clip timeline and same-process lifecycle evidence for a fixed fixture."""
from dataclasses import dataclass
import re
from fixture_repeat import fields, trial

COUNTS = (600, 1200, 600)
FAULT_COUNTS = (600, 120, 1200, 120, 600)


@dataclass(frozen=True)
class Resource:
    phase: int
    pid: int
    peak: int
    working: int
    private: int
    handles: int


def split(text, counts=COUNTS):
    if counts not in (COUNTS, FAULT_COUNTS):
        raise ValueError('unsupported lifecycle sequence')
    if len(text) > (768_000 if counts == FAULT_COUNTS else 512_000):
        raise ValueError('lifecycle log bound')
    blocks, resources = [], []
    active = None
    for line in text.splitlines():
        if line.startswith('HDLifecycleSample '):
            r = Resource(*fields(line, 'HDLifecycleSample', 6))
            if r.phase != len(resources) or r.phase > len(counts) or not r.pid or not 0 < r.working <= r.peak or not 0 < r.private <= 2**32 or not r.handles:
                raise ValueError('lifecycle sample bounds')
            if resources and (r.pid != resources[0].pid or r.peak < resources[-1].peak):
                raise ValueError('lifecycle identity or peak regression')
            if (r.phase == 0 and active is not None) or (r.phase and active != r.phase):
                raise ValueError('lifecycle sample order')
            resources.append(r)
        elif line.startswith('HDLifecycleBegin '):
            n,count=fields(line,'HDLifecycleBegin',2)
            if active is not None or n!=len(blocks)+1 or n>len(counts) or count!=counts[n-1] or len(resources)!=n:
                raise ValueError('lifecycle begin')
            active=n
            blocks.append([])
        elif line.startswith('HDLifecycleEnd '):
            n,=fields(line,'HDLifecycleEnd',1)
            if active!=n or len(resources)!=n+1:
                raise ValueError('lifecycle end')
            active=None
        elif line.startswith('HDLifecycle'):
            raise ValueError('unexpected lifecycle marker')
        elif active is not None:
            if len(resources)!=active:
                raise ValueError('clip evidence after scope sample')
            blocks[-1].append(line)
    if active is not None or len(blocks)!=len(counts) or len(resources)!=len(counts)+1:
        raise ValueError('incomplete lifecycle')
    return tuple('\n'.join(b) for b in blocks),tuple(resources)


def timeline(text,count):
    if count not in COUNTS:
        raise ValueError('unsupported clip count')
    pairs=[tuple(map(int,m)) for m in re.findall(r'^LiveFrame ([0-9]{1,12}) ([0-9]{1,20})$',text,re.M)]
    if len(pairs)!=count or any(i!=n for n,(i,_) in enumerate(pairs)):
        raise ValueError('selected frame count/order')
    pts=[t-pairs[0][1] for _,t in pairs]
    gaps=[b-a for a,b in zip(pts,pts[1:])]
    if any(g<=0 for g in gaps) or not (300_000_000 if count==1200 else 150_000_000)<=pts[-1]<=900_000_000:
        raise ValueError('selected timestamp span')
    durations=[min(g,1_000_000) for g in gaps]+[400_000]
    n,obs,excluded,overlap=fields(text,'LiveCapture',4)
    if n!=count or obs!=count+excluded or not 0<overlap<count:
        raise ValueError('capture counters')
    n,first,end,coverage,gap,high,drop,discard=fields(text,'LiveHandoff',8)
    expected_end=pts[-1]+durations[-1]
    if (n,first,end,coverage,gap,drop,discard)!=(count,0,expected_end,sum(durations),expected_end-sum(durations),0,0) or not 1<=high<=8 or gap<=0:
        raise ValueError('handoff counters')
    return tuple(zip(pts,durations))


def decoded(text, intervals):
    rows=[tuple(map(int,m)) for m in re.findall(r'^TimedPTS ([0-9]+) ([0-9]+) ([0-9]+)$',text,re.M)]
    count=len(intervals)
    if len(rows)!=count or any(i!=n or e!=intervals[n][0] or abs(e-a)>20_000 for n,(i,e,a) in enumerate(rows)):
        raise ValueError('decoded PTS')
    if f'Decoded {count} H264 frames, 1280x720, monotonic timestamps and luma pattern verified' not in text:
        raise ValueError('decoded pixel evidence')
    return max(abs(e-a) for _,e,a in rows)


def verify_clip(text, decode_text, count, size):
    intervals=timeline(text,count)
    delta=decoded(decode_text,intervals)
    result=trial(text+f'\nLiveDecode count={count} max_abs_delta_100ns={delta}\n',count)
    if result.output_bytes!=size:
        raise ValueError('output size mismatch')
    return result
