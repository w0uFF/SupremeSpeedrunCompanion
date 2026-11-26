"""Cross-language canonical schema-1 file contract, using the actual C++ executable."""
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import ssc_profiles as s

exe = sys.argv[1]
book = s.initial()
book = s.edit(book, 'replace', 'Training', values=dict(s.defaults(), package_directory='D:\\Runs\\épreuve\\🏂'))
book = s.edit(book, 'select', 'Speedrun Tryhard')
raw = s.canonical(s.validate_book(book))
result = subprocess.run([exe, '--roundtrip'], input=raw, capture_output=True, timeout=10)
assert result.returncode == 0 and result.stdout == raw, (result.returncode, result.stderr)
for bad in (raw.replace(b'"schema":1', b'"schema":2'), raw.replace(b'"schema":1', b'"schema":1,"schema":1'),
            raw.replace(b'"fps":"30"', b'"fps":"120"'), raw.replace(b'\\ud83c\\udfc2', b'\\ud800'),
            raw.replace(b'"auto_record":false', b'"auto_record":0'), b' ' * (s.MAX_BYTES + 1)):
    result = subprocess.run([exe, '--roundtrip'], input=bad, capture_output=True, timeout=10)
    assert result.returncode == 2, (result.returncode, result.stderr)
print('Python/C++ canonical profile roundtrip byte-identical; six invalid inputs rejected')
