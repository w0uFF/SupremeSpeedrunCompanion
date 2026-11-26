"""Bounded actual WGC sequence -> timestamped video-only encoding -> decode."""
import re
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

SIZE = 160 * 96 * 3 // 2
with tempfile.TemporaryDirectory() as directory:
    raw = Path(directory) / 'timed.nv12'
    output = Path(directory) / 'timed.mp4'
    result = subprocess.run([sys.argv[1], '--timed', str(raw)], capture_output=True, text=True, timeout=15)
    print(result.stdout, end='')
    print(result.stderr, end='')
    if result.returncode == 77:
        sys.exit(77)
    result.check_returncode()
    data = raw.read_bytes()
    assert data[:8] == b'SSCTIME1' and len(data) == 8 + 12*(8+SIZE)
    times = [struct.unpack_from('<Q',data,8+i*(8+SIZE))[0] for i in range(12)]
    pts = [t-times[0] for t in times]
    gaps = [b-a for a,b in zip(pts,pts[1:])]
    assert all(g>0 for g in gaps)
    duration = sum(min(g,1000000) for g in gaps)+400000
    end = pts[-1]+400000
    capture = re.search(r'^TimedWGC (\d+) (\d+) (\d+) (\d+) (\d+)$',result.stdout,re.M)
    assert capture
    count, observed, skipped, first, last = map(int,capture.groups())
    assert count == 12 and observed == count+skipped and (first,last)==(times[0],times[-1])
    result = subprocess.run([sys.argv[2],str(output),'--timed-video',str(raw)],capture_output=True,text=True,timeout=30)
    print(result.stdout,end='')
    print(result.stderr,end='')
    result.check_returncode()
    handoff = re.search(r'^TimedHandoff (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)$',result.stdout,re.M)
    assert handoff
    count,first,actual_end,actual_duration,gap,high = map(int,handoff.groups())
    assert (count,first,actual_end,actual_duration,gap)==(12,0,end,duration,end-duration)
    assert 1<=high<=24
    decoded=[tuple(map(int,x)) for x in re.findall(r'^TimedPTS (\d+) (\d+) (\d+)$',result.stdout,re.M)]
    assert len(decoded)==12 and all(i==j and e==pts[j] and abs(a-e)<=20000 for j,(i,e,a) in enumerate(decoded))
    assert output.stat().st_size>0
    print('Actual WGC selected frames and timestamps encoded/decoded; no audio, no gameplay coverage claim')
