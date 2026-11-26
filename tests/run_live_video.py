"""Concurrent own-window producer/worker, independent timeline and decode; resize refusal."""
import re
from pathlib import Path
import subprocess
import sys
import tempfile

marathon=len(sys.argv)==4 and sys.argv[3]=='--marathon'
extended=len(sys.argv)==4 and sys.argv[3]=='--extended'
long_fixture=len(sys.argv)==4 and sys.argv[3]=='--long'
expected_count=1200 if marathon else 600 if extended else 240 if long_fixture else 60
with tempfile.TemporaryDirectory() as directory:
    root=Path(directory)
    output=root/'live.mp4'
    result=subprocess.run([sys.argv[1],'--live-marathon' if marathon else '--live-extended' if extended else '--live-long' if long_fixture else '--live',str(output)],capture_output=True,text=True,timeout=95 if marathon else 50 if extended else 25)
    print(result.stdout,end='')
    print(result.stderr,end='')
    if result.returncode==77:
        sys.exit(77)
    result.check_returncode()
    pairs=[tuple(map(int,x)) for x in re.findall(r'^LiveFrame (\d+) (\d+)$',result.stdout,re.M)]
    assert len(pairs)==expected_count and [i for i,_ in pairs]==list(range(expected_count))
    pts=[t-pairs[0][1] for _,t in pairs]
    gaps=[b-a for a,b in zip(pts,pts[1:])]
    assert all(g>0 for g in gaps) and pts[-1]>=(300000000 if marathon else 150000000 if extended else 50000000 if long_fixture else 10000000)
    durations=[min(g,1000000) for g in gaps]+[400000]
    end=pts[-1]+400000
    coverage=sum(durations)
    count,observed,excluded,overlap=map(int,re.search(r'^LiveCapture (\d+) (\d+) (\d+) (\d+)$',result.stdout,re.M).groups())
    assert count==expected_count and observed==count+excluded and 0<overlap<expected_count
    count,first,actual_end,actual_coverage,gap,high,dropped,discarded=map(int,re.search(r'^LiveHandoff (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)$',result.stdout,re.M).groups())
    assert (count,first,actual_end,actual_coverage,gap,dropped,discarded)==(expected_count,0,end,coverage,end-coverage,0,0)
    assert 1<=high<=8 and output.stat().st_size>0
    if long_fixture or extended or marathon:
        assert gap>0, 'controlled capture pause did not produce an observed handoff gap'
    if marathon:
        samples,wall,cpu,median,p95,p99,maximum,drain=map(int,re.search(r'^LivePerformance (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)
    timeline.write_text(''.join(f'{p} {d}\n' for p,d in zip(pts,durations)),encoding='ascii')
    result=subprocess.run([sys.argv[2],str(output),'--decode-live',str(timeline)],capture_output=True,text=True,timeout=50 if marathon else 30)
    print(result.stdout,end='')
    print(result.stderr,end='')
    result.check_returncode()
    decoded=[tuple(map(int,x)) for x in re.findall(r'^TimedPTS (\d+) (\d+) (\d+)$',result.stdout,re.M)]
    assert len(decoded)==expected_count and all(i==j and e==pts[j] and abs(e-a)<=20000 for j,(i,e,a) in enumerate(decoded))
    print(f'LiveDecode count={expected_count} max_abs_delta_100ns={max(abs(e-a) for _,e,a in decoded)}')
    if not long_fixture and not extended and not marathon:
        result=subprocess.run([sys.argv[1],'--live-resize',str(root/'resize.mp4')],capture_output=True,text=True,timeout=50 if extended else 25)
        print(result.stdout,end='')
        print(result.stderr,end='')
        assert result.returncode==1 and 'LiveResizeRejected ' in result.stderr and 'LiveHandoff ' not in result.stdout
        print('Actual window resize aborted fixture without finalized success')
        result=subprocess.run([sys.argv[1],'--live-simulated-loss',str(root/'loss.mp4')],capture_output=True,text=True,timeout=25)
        print(result.stdout,end='')
        print(result.stderr,end='')
        assert result.returncode==1 and 'LiveSimulatedDeviceLoss ' in result.stderr and 'LiveHandoff ' not in result.stdout
        print('Injected device-removal status cancelled own-window fixture; actual device loss not induced')
    else:
        print('Long actual capture fixture: controlled pause retained as a handoff gap; no gameplay claim')
,result.stdout,re.M).groups())
        assert samples==expected_count and wall>0 and 0<=drain<=wall
        assert 0<=median<=p95<=p99<=maximum
        print('Measured isolated fixture only: selected processing latency in us; CPU in 100ns; no gameplay threshold')
    timeline=root/'timeline.txt'
    timeline.write_text(''.join(f'{p} {d}\n' for p,d in zip(pts,durations)),encoding='ascii')
    result=subprocess.run([sys.argv[2],str(output),'--decode-live',str(timeline)],capture_output=True,text=True,timeout=30)
    print(result.stdout,end='')
    print(result.stderr,end='')
    result.check_returncode()
    decoded=[tuple(map(int,x)) for x in re.findall(r'^TimedPTS (\d+) (\d+) (\d+)$',result.stdout,re.M)]
    assert len(decoded)==expected_count and all(i==j and e==pts[j] and abs(e-a)<=20000 for j,(i,e,a) in enumerate(decoded))
    print(f'LiveDecode count={expected_count} max_abs_delta_100ns={max(abs(e-a) for _,e,a in decoded)}')
    if not long_fixture and not extended:
        result=subprocess.run([sys.argv[1],'--live-resize',str(root/'resize.mp4')],capture_output=True,text=True,timeout=50 if extended else 25)
        print(result.stdout,end='')
        print(result.stderr,end='')
        assert result.returncode==1 and 'LiveResizeRejected ' in result.stderr and 'LiveHandoff ' not in result.stdout
        print('Actual window resize aborted fixture without finalized success')
        result=subprocess.run([sys.argv[1],'--live-simulated-loss',str(root/'loss.mp4')],capture_output=True,text=True,timeout=25)
        print(result.stdout,end='')
        print(result.stderr,end='')
        assert result.returncode==1 and 'LiveSimulatedDeviceLoss ' in result.stderr and 'LiveHandoff ' not in result.stdout
        print('Injected device-removal status cancelled own-window fixture; actual device loss not induced')
    else:
        print('Long actual capture fixture: controlled pause retained as a handoff gap; no gameplay claim')
