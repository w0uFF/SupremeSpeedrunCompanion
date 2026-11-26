"""Fresh private Unicode database path, no existing database changes."""
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory() as directory:
    db = Path(directory) / 'run-épreuve.sqlite'
    subprocess.run([sys.argv[1], str(db)], check=True, timeout=15)
    before = db.read_bytes()
    result = subprocess.run([sys.argv[1], str(db)], timeout=15)
    if result.returncode != 2 or db.read_bytes() != before:
        raise RuntimeError('existing database not preserved')
