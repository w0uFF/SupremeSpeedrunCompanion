"""Validate controlled HD cancellation and simulated-loss evidence separately from success."""
from dataclasses import dataclass
import re
from fixture_repeat import fields
from fixture_lifecycle import FAULT_COUNTS, split

FAULTS = {2: 'user-cancel', 4: 'simulated-device-loss'}


@dataclass(frozen=True)
class Fault:
    reason: str
    selected: int
    accepted: int
    written: int
    discarded: int
    pending: int
    high: int
    artifact_bytes: int | None


def fault(text, reason, artifact_bytes):
    if reason not in (*FAULTS.values(), 'sink-write-failure', 'sink-finalize-failure'):
        raise ValueError('unsupported fixture fault')
    if any(line.startswith(('LiveCapture ', 'LiveHandoff ', 'LiveDecode ', 'LivePerformance ', 'LiveResources '))
           for line in text.splitlines()):
        raise ValueError('aborted scope advertised success')
    lines = [line for line in text.splitlines() if line.startswith('HDFaultOutcome ')]
    state = 'failed' if reason in ('sink-write-failure','sink-finalize-failure') else 'cancelled'
    match = re.fullmatch(r'HDFaultOutcome '+re.escape(reason)+' '+state+r' ([0-9]{1,12})'*8, lines[0]) if len(lines)==1 else None
    if not match:
        raise ValueError('fault outcome')
    selected, accepted, written, discarded, pending, high, dropped, invalid = map(int,match.groups())
    finalize = reason=='sink-finalize-failure'
    if (selected!=120 or accepted!=(120 if finalize else 119) or not 0<written<=accepted
            or written+discarded!=accepted or pending!=(0 if finalize else 1) or not 1<=high<=8 or dropped or invalid
            or (reason=='sink-write-failure' and (written,discarded)!=(118,1))
            or (finalize and (written,discarded)!=(120,0))):
        raise ValueError('fault accounting')
    pairs = [tuple(map(int,m)) for m in re.findall(r'^LiveFrame ([0-9]{1,12}) ([0-9]{1,20})$',text,re.M)]
    if (len(pairs)!=selected or any(i!=n for n,(i,_) in enumerate(pairs))
            or any(b[1]<=a[1] for a,b in zip(pairs,pairs[1:])) or pairs[-1][1]-pairs[0][1]>900_000_000):
        raise ValueError('fault selected frame order')
    geometry = fields(text,'HDWindowGeometry',8)
    if geometry[:6]!=(1280,720,1280,720,1280,720) or geometry[6]<1280 or geometry[7]<720:
        raise ValueError('fault fixture geometry')
    exists,size = fields(text,'HDFaultArtifact',2)
    if (exists not in (0,1) or size>16*1024*1024 or (not exists and size)
            or (size if exists else None)!=artifact_bytes):
        raise ValueError('fault artifact identity')
    return Fault(reason,selected,accepted,written,discarded,pending,high,artifact_bytes)


def evidence(text):
    blocks, resources = split(text, FAULT_COUNTS)
    gui = [fields(line,'HDFaultGuiSample',4) for line in text.splitlines() if line.startswith('HDFaultGuiSample ')]
    if len(gui)!=len(resources):
        raise ValueError('GUI resource sample count')
    for (phase,pid,gdi,user), r in zip(gui,resources):
        if phase!=r.phase or pid!=r.pid or gdi>65536 or user>65536:
            raise ValueError('GUI resource identity or bounds')
    for i,block in enumerate(blocks,1):
        if i not in FAULTS and any(line.startswith(('HDFaultOutcome ', 'HDFaultArtifact ')) for line in block.splitlines()):
            raise ValueError('unexpected fault in successful scope')
    return blocks,resources,tuple(gui)
