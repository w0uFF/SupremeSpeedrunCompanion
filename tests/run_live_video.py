"""Concurrent own-window producer/worker, independent timeline and decode; resize refusal."""
import re
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory() as directory:
    root=Path(directory)
    output=root/'live.mp4'
    result=subprocess.run([sys.argv[1],'--live',str(output)],capture_output=True,text=True,timeout=25)
    print(result.stdout,end='')
    print(result.stderr,end='')
    if result.returncode==77:
        sys.exit(77)
    result.check_returncode()
    pairs=[tuple(map(int,x)) for x in re.findall(r'^LiveFrame (\d+) (\d+)$',result.stdout,re.M)]
    assert len(pairs)==60 and [i for i,_ in pairs]==list(range(60))
    pts=[t-pairs[0][1] for _,t in pairs]
    gaps=[b-a for a,b in zip(pts,pts[1:])]
    assert all(g>0 for g in gaps) and pts[-1]>=10000000
    durations=[min(g,1000000) for g in gaps]+[400000]
    end=pts[-1]+400000
    coverage=sum(durations)
    count,observed,excluded,overlap=map(int,re.search(r'^LiveCapture (\d+) (\d+) (\d+) (\d+)$',result.stdout,re.M).groups())
    assert count==60 and observed==count+excluded and 0<overlap<60
    count,first,actual_end,actual_coverage,gap,high,dropped,discarded=map(int,re.search(r'^LiveHandoff (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)$',result.stdout,re.M).groups())
    assert (count,first,actual_end,actual_coverage,gap,dropped,discarded)==(60,0,end,coverage,end-coverage,0,0)
    assert 1<=high<=8 and output.stat().st_size>0
    timeline=root/'timeline.txt'
    timeline.write_text(''.join(f'{p} {d}\n' for p,d in zip(pts,durations)),encoding='ascii')
    result=subprocess.run([sys.argv[2],str(output),'--decode-live',str(timeline)],capture_output=True,text=True,timeout=30)
    print(result.stdout,end='')
    print(result.stderr,end='')
    result.check_returncode()
    decoded=[tuple(map(int,x)) for x in re.findall(r'^TimedPTS (\d+) (\d+) (\d+)$',result.stdout,re.M)]
    assert len(decoded)==60 and all(i==j and e==pts[j] and abs(e-a)<=20000 for j,(i,e,a) in enumerate(decoded))
    print(f'LiveDecode count=60 max_abs_delta_100ns={max(abs(e-a) for _,e,a in decoded)}')
    result=subprocess.run([sys.argv[1],'--live-resize',str(root/'resize.mp4')],capture_output=True,text=True,timeout=25)
    print(result.stdout,end='')
    print(result.stderr,end='')
    assert result.returncode==1 and 'LiveResizeRejected ' in result.stderr and 'LiveHandoff ' not in result.stdout
    print('Actual window resize aborted fixture without finalized success; device loss not induced')
