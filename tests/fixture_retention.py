"""Bounded stage evidence for a real sink with injected write failure."""
from fixture_repeat import fields
from fixture_lifecycle import WRITE_COUNTS, split, Resource
from fixture_fault_lifecycle import fault


def evidence(text):
    blocks, resources = split(text, WRITE_COUNTS)
    gui = [fields(line, 'HDFaultGuiSample', 4) for line in text.splitlines() if line.startswith('HDFaultGuiSample ')]
    if len(gui)!=4 or any((p,pid)!=(r.phase,r.pid) or gdi>65536 or user>65536
                          for (p,pid,gdi,user),r in zip(gui,resources)):
        raise ValueError('GUI sample identity')
    stages=[]
    peak=resources[0].peak
    for i,block in enumerate(blocks,1):
        rows=tuple(Resource(*fields(line,'HDStageResources',6)) for line in block.splitlines() if line.startswith('HDStageResources '))
        if len(rows)!=10:
            raise ValueError('stage count')
        for n,r in enumerate(rows):
            if r.phase!=n or r.pid!=resources[0].pid or not 0<r.working<=r.peak or r.peak<peak or not 0<r.private<=2**32 or not r.handles:
                raise ValueError('stage identity or bounds')
            peak=r.peak
        if resources[i].peak<peak:
            raise ValueError('post-scope peak')
        peak=resources[i].peak
        if i!=2 and any(line.startswith(('HDFaultOutcome ', 'HDFaultArtifact ')) for line in block.splitlines()):
            raise ValueError('unexpected fault evidence')
        stages.append(rows)
    return blocks,resources,tuple(gui),tuple(stages)


def failure(text, artifact_bytes, finalize=False):
    return fault(text, 'sink-finalize-failure' if finalize else 'sink-write-failure', artifact_bytes)
