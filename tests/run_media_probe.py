"""Run the standalone Windows probe in a private temporary directory."""
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory() as directory:
    output = Path(directory) / 'synthetic.mp4'
    subprocess.run([sys.argv[1], str(output), *sys.argv[2:]], check=True, timeout=90)
    if not output.is_file() or output.stat().st_size == 0:
        raise RuntimeError('missing media output')
