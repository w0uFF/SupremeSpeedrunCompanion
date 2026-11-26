"""One WGC process, three capture lifecycles and independent decode of each."""
from pathlib import Path
import subprocess
import sys
import tempfile
from fixture_display import hosted_hd_mode
from fixture_lifecycle import COUNTS, split, timeline, verify_clip


def main():
    if len(sys.argv)!=3:
        raise SystemExit('usage: run_hd_lifecycle.py WGC_PROBE MEDIA_PROBE')
    with tempfile.TemporaryDirectory() as directory:
        root=Path(directory)
        with hosted_hd_mode(True):
            result=subprocess.run([sys.argv[1],'--live-hd720-lifecycle',str(root)],capture_output=True,text=True,timeout=250)
        print(result.stdout,end='',flush=True)
        print(result.stderr,end='',flush=True)
        result.check_returncode()
        clips,resources=split(result.stdout)
        for i,(clip,count) in enumerate(zip(clips,COUNTS),1):
            intervals=timeline(clip,count)
            path=root/f'segment-{i}.mp4'
            timing=root/f'timeline-{i}.txt'
            timing.write_text(''.join(f'{p} {d}\n' for p,d in intervals),encoding='ascii')
            decode=subprocess.run([sys.argv[2],str(path),'--decode-live-hd720',str(timing)],capture_output=True,text=True,timeout=50)
            print(decode.stdout,end='',flush=True)
            print(decode.stderr,end='',flush=True)
            decode.check_returncode()
            t=verify_clip(clip,decode.stdout,count,path.stat().st_size)
            print(f'HDLifecycleVerified {i} {count} {t.pts_delta_100ns}',flush=True)
        for r in resources[1:]:
            print(f'HDLifecycleDelta {r.phase} private_bytes={r.private-resources[0].private} working_bytes={r.working-resources[0].working} handles={r.handles-resources[0].handles}')
        print('HDLifecycleComplete 3 2400; descriptive same-process evidence, not leak proof')


if __name__=='__main__': main()
