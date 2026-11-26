"""Two fresh processes, same runner; all existing HD checks must pass twice."""
from pathlib import Path
import subprocess
import sys
from fixture_repeat import trial, summarize


def main():
    if len(sys.argv) != 3:
        raise SystemExit('usage: run_hd_repeat.py WGC_PROBE MEDIA_PROBE')
    wrapper = Path(__file__).with_name('run_live_video.py')
    results = []
    for i in range(1, 3):
        print(f'HDRepeatBegin {i}', flush=True)
        child = subprocess.run([sys.executable, str(wrapper), *sys.argv[1:], '--hd720-extended'],
                               capture_output=True, text=True, timeout=165)
        print(child.stdout, end='', flush=True)
        print(child.stderr, end='', flush=True)
        if child.returncode == 77:
            raise RuntimeError('required repeat blocked; no two-trial evidence')
        child.check_returncode()
        results.append(trial(child.stdout))
        print(f'HDRepeatEnd {i}', flush=True)
    for name, (low, high) in summarize(results).items():
        print(f'HDRepeatRange {name} {low} {high}', flush=True)


if __name__=='__main__': main()
