"""Same-process actual HD recording, injected write failure, restart and stage diagnostics."""
from pathlib import Path
import subprocess
import sys
import tempfile
from fixture_display import hosted_hd_mode
from fixture_lifecycle import WRITE_COUNTS, timeline, verify_clip
from fixture_retention import evidence, failure


def main():
    if len(sys.argv) not in (3,4) or (len(sys.argv)==4 and sys.argv[3] not in ('--finalize','--target-closed')):
        raise SystemExit('usage: run_hd_write_failure.py WGC_PROBE MEDIA_PROBE [--finalize|--target-closed]')
    target_closed=len(sys.argv)==4 and sys.argv[3]=='--target-closed'
    finalize=len(sys.argv)==4 and sys.argv[3]=='--finalize'
    mode='--live-hd720-target-closed-lifecycle' if target_closed else '--live-hd720-finalize-failure-lifecycle' if finalize else '--live-hd720-write-failure-lifecycle'
    label='HDTarget' if target_closed else 'HDFinalize' if finalize else 'HDWrite'
    with tempfile.TemporaryDirectory() as directory:
        root=Path(directory)
        with hosted_hd_mode(True):
            result=subprocess.run([sys.argv[1],mode,str(root)],
                                  capture_output=True,text=True,timeout=210)
        print(result.stdout,end='',flush=True)
        print(result.stderr,end='',flush=True)
        result.check_returncode()
        clips,resources,gui,stages=evidence(result.stdout,target_closed)
        if target_closed:
            names={p.name for p in root.iterdir()}
            if not {'segment-1.mp4','segment-3.mp4'}<=names<={'segment-1.mp4','segment-2.mp4','segment-3.mp4'}:
                raise ValueError('unexpected closure output files')
        for i,(clip,count) in enumerate(zip(clips,WRITE_COUNTS),1):
            path=root/f'segment-{i}.mp4'
            if i==2:
                if path.is_symlink() or (path.exists() and not path.is_file()):
                    raise ValueError('unexpected artifact type')
                f=failure(clip,path.stat().st_size if path.exists() else None,finalize,target_closed)
                print(f'{label}FailureVerified {f.selected} {f.accepted} {f.written} {f.discarded} {f.pending}',flush=True)
            else:
                timing=root/f'timeline-{i}.txt'
                timing.write_text(''.join(f'{p} {d}\n' for p,d in timeline(clip,count)),encoding='ascii')
                decode=subprocess.run([sys.argv[2],str(path),'--decode-live-hd720',str(timing)],capture_output=True,text=True,timeout=50)
                print(decode.stdout,end='',flush=True)
                print(decode.stderr,end='',flush=True)
                decode.check_returncode()
                t=verify_clip(clip,decode.stdout,count,path.stat().st_size)
                if t.peak_bytes>resources[i].peak:
                    raise ValueError('post-scope peak below capture peak')
                print(f'{label}SuccessVerified {i} {count} {t.pts_delta_100ns}',flush=True)
            rows=stages[i-1]
            print(f'HDStageHandleDelta {i} window={rows[1].handles-rows[0].handles} device={rows[2].handles-rows[1].handles} pool={rows[3].handles-rows[2].handles} worker={rows[4].handles-rows[3].handles} close={rows[5].handles-rows[4].handles} join={rows[6].handles-rows[5].handles} scope={rows[7].handles-rows[6].handles} settle1={rows[8].handles-rows[7].handles} settle3={rows[9].handles-rows[8].handles}',flush=True)
        reason='observed target Closed after destruction' if target_closed else 'injected sink failure'
        print(f'{label}LifecycleComplete 3 1200 1; {reason}, descriptive retention evidence')


if __name__=='__main__': main()
