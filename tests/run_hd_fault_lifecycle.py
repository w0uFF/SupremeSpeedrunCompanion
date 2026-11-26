"""Actual own-window fault/restart sequence in one capture process."""
from pathlib import Path
import subprocess
import sys
import tempfile
from fixture_display import hosted_hd_mode
from fixture_lifecycle import FAULT_COUNTS, timeline, verify_clip
from fixture_fault_lifecycle import FAULTS, evidence, fault


def main():
    if len(sys.argv)!=3:
        raise SystemExit('usage: run_hd_fault_lifecycle.py WGC_PROBE MEDIA_PROBE')
    with tempfile.TemporaryDirectory() as directory:
        root=Path(directory)
        with hosted_hd_mode(True):
            result=subprocess.run([sys.argv[1],'--live-hd720-fault-lifecycle',str(root)],capture_output=True,text=True,timeout=300)
        print(result.stdout,end='',flush=True)
        print(result.stderr,end='',flush=True)
        result.check_returncode()
        clips,resources,gui=evidence(result.stdout)
        for i,(clip,count) in enumerate(zip(clips,FAULT_COUNTS),1):
            path=root/f'segment-{i}.mp4'
            if i in FAULTS:
                if path.is_symlink() or (path.exists() and not path.is_file()):
                    raise ValueError('unexpected fault artifact type')
                f=fault(clip,FAULTS[i],path.stat().st_size if path.exists() else None)
                print(f'HDFaultVerified {i} {f.reason} selected={f.selected} written={f.written} discarded={f.discarded} pending={f.pending}',flush=True)
                continue
            intervals=timeline(clip,count)
            timing=root/f'timeline-{i}.txt'
            timing.write_text(''.join(f'{p} {d}\n' for p,d in intervals),encoding='ascii')
            decode=subprocess.run([sys.argv[2],str(path),'--decode-live-hd720',str(timing)],capture_output=True,text=True,timeout=50)
            print(decode.stdout,end='',flush=True)
            print(decode.stderr,end='',flush=True)
            decode.check_returncode()
            t=verify_clip(clip,decode.stdout,count,path.stat().st_size)
            if t.peak_bytes>resources[i].peak:
                raise ValueError('post-scope peak below capture peak')
            print(f'HDFaultSuccessVerified {i} {count} {t.pts_delta_100ns}',flush=True)
        for r,(_,_,gdi,user) in zip(resources[1:],gui[1:]):
            print(f'HDFaultLifecycleDelta {r.phase} private_bytes={r.private-resources[0].private} handles={r.handles-resources[0].handles} gdi={gdi-gui[0][2]} user={user-gui[0][3]}')
        print('HDFaultLifecycleComplete 5 2400 2; simulated device loss, not physical device recovery')


if __name__=='__main__': main()
