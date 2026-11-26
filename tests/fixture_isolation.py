"""Strict, descriptive evidence for independent process resource-isolation fixtures."""
from dataclasses import dataclass
import re

MODES = ('window', 'device', 'pool', 'worker-idle', 'worker-write')
CYCLES = 3
REFINEMENT_MODES = ('support-only', 'item-only', 'framepool-only', 'pool')
REFINEMENT_CYCLES = 6


@dataclass(frozen=True)
class Sample:
    cycle: int
    stage: int
    pid: int
    peak: int
    working: int
    private: int
    handles: int
    gdi: int
    user: int


@dataclass(frozen=True)
class Outcome:
    cycle: int
    state: str
    accepted: int
    written: int
    discarded: int
    exists: int
    size: int


def evidence(text, mode, cycles=CYCLES):
    allowed = MODES if cycles == CYCLES else REFINEMENT_MODES if cycles == REFINEMENT_CYCLES else ()
    if mode not in allowed or len(text)>64_000:
        raise ValueError('isolation mode or output bound')
    rows=[line for line in text.splitlines() if line.startswith('Isolation')]
    if len(rows)!=2+5*cycles or rows[0]!=f'IsolationBegin {mode}' or rows[-1]!=f'IsolationEnd {mode} {cycles}':
        raise ValueError('isolation marker sequence')
    samples, outcomes=[], []
    pid=peak=0
    for cycle in range(1,cycles+1):
        block=rows[1+(cycle-1)*5:1+cycle*5]
        for stage,line in enumerate((block[0],*block[2:])):
            m=re.fullmatch(r'IsolationSample'+r' ([0-9]{1,12})'*9,line)
            if not m:
                raise ValueError('isolation sample')
            s=Sample(*map(int,m.groups()))
            if (s.cycle,s.stage)!=(cycle,stage) or not s.pid or (pid and s.pid!=pid) or s.peak<peak or not 0<s.working<=s.peak or not 0<s.private<=2**32 or not s.handles or max(s.gdi,s.user)>65536:
                raise ValueError('isolation sample bounds or identity')
            pid,peak=s.pid,s.peak
            samples.append(s)
        m=re.fullmatch(r'IsolationOutcome ([0-9]{1,2}) (none|cancelled|finalized)'+r' ([0-9]{1,12})'*5,block[1])
        if not m:
            raise ValueError('isolation outcome')
        o=Outcome(int(m[1]),m[2],*map(int,m.groups()[2:]))
        expected=('finalized',60,60,0) if mode=='worker-write' else ('cancelled',0,0,0) if mode=='worker-idle' else ('none',0,0,0)
        if o.cycle!=cycle or (o.state,o.accepted,o.written,o.discarded)!=expected or o.exists not in (0,1) or o.size>16*1024*1024 or (not o.exists and o.size) or (mode=='worker-write' and not o.size) or (mode not in ('worker-idle','worker-write') and o.exists):
            raise ValueError('isolation outcome bounds')
        outcomes.append(o)
    return tuple(samples),tuple(outcomes)
