"""Own-window capture to synthetic-timestamp encode/decode; temporary data only."""
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory() as directory:
    raw = Path(directory) / 'captured.nv12'
    output = Path(directory) / 'captured.mp4'
    result = subprocess.run([sys.argv[1], str(raw)], timeout=15)
    if result.returncode == 77:
        sys.exit(77)
    result.check_returncode()
    if raw.stat().st_size != 2 * 160 * 96 * 3 // 2:
        raise RuntimeError('invalid captured NV12 length')
    subprocess.run([sys.argv[2], str(output), '--captured-av', str(raw)], check=True, timeout=30)
    if not output.is_file() or output.stat().st_size == 0:
        raise RuntimeError('missing captured video output')
    print('Own-window captured pixel fixture encoded and decoded; synthetic timestamps and audio, not live capture')
