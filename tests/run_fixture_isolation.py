"""Run each resource-isolation mode in a separate child; compare only observations."""
from pathlib import Path
import subprocess
import sys
import tempfile
from fixture_isolation import MODES, evidence


def main():
    if len(sys.argv)!=3:
        raise SystemExit('usage: run_fixture_isolation.py WGC_PROBE MEDIA_PROBE')
    with tempfile.TemporaryDirectory() as directory:
        root=Path(directory)
        for mode in MODES:
            target=root/mode
            target.mkdir()
            child=subprocess.run([sys.argv[1],'--isolate-retention',mode,str(target)],
                                 capture_output=True,text=True,timeout=120)
            print(child.stdout,end='',flush=True)
            print(child.stderr,end='',flush=True)
            child.check_returncode()
            rows,outcomes=evidence(child.stdout,mode)
            expected=set()
            for outcome in outcomes:
                file=target/f'worker-{outcome.cycle}.mp4'
                if file.is_symlink() or (file.exists() and not file.is_file()) or file.exists()!=bool(outcome.exists):
                    raise ValueError('isolation artifact existence/type')
                if outcome.exists:
                    expected.add(file.name)
                    if file.stat().st_size!=outcome.size:
                        raise ValueError('isolation artifact size')
                if mode=='worker-write':
                    timing=root/'timeline.txt'
                    timing.write_text(''.join(f'{i*400000} 400000\n' for i in range(60)),encoding='ascii')
                    decode=subprocess.run([sys.argv[2],str(file),'--decode-live',str(timing)],
                                          capture_output=True,text=True,timeout=50)
                    print(decode.stdout,end='',flush=True)
                    print(decode.stderr,end='',flush=True)
                    decode.check_returncode()
                    if 'Decoded 60 H264 frames, 160x96, monotonic timestamps and luma pattern verified' not in decode.stdout:
                        raise ValueError('isolation worker decode')
                block=rows[(outcome.cycle-1)*4:outcome.cycle*4]
                print(f'IsolationDelta {mode} {outcome.cycle} handles_after={block[1].handles-block[0].handles} handles_settled={block[3].handles-block[0].handles} private_settled={block[3].private-block[0].private}',flush=True)
            if {p.name for p in target.iterdir()}!=expected:
                raise ValueError('unexpected isolation files')
        print('IsolationComplete 5 modes 15 scopes 180 independently decoded synthetic frames; descriptive, not ownership or leak proof')


if __name__=='__main__': main()
