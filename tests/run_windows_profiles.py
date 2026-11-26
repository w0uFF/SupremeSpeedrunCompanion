"""Native persistence, separate processes, Python interoperability and failure preservation."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import ssc_profiles as s

exe = sys.argv[1]
with tempfile.TemporaryDirectory() as root:
    directory = Path(root) / 'épreuve'
    directory.mkdir()
    file = directory / 'profiles.json'
    def run(action, status=0):
        result = subprocess.run([exe, action, str(directory)], capture_output=True, timeout=10)
        assert result.returncode == status, (action, result.returncode, result.stderr)
        return result.stdout
    run('directory')
    run('create')
    before = file.read_bytes()
    assert s.read_json(file)['active'] == 'Training'
    run('create', 3)
    assert file.read_bytes() == before
    run('replace')
    native = s.ProfileStore(file).load()
    assert native['profiles']['Training']['fps'] == '60'
    assert native['profiles']['Training']['input_overlay'] is True
    python_book = s.edit(s.initial(), 'replace', 'Training', values=dict(s.defaults(), package_directory='D:\\Runs\\épreuve\\🏂'))
    s.ProfileStore(file).save(python_book)
    assert run('read') == file.read_bytes()
    run('replace')
    assert s.ProfileStore(file).load()['profiles']['Training']['package_directory'] == 'D:\\Runs\\épreuve\\🏂'
    malformed = b'{"schema":999}'
    file.write_bytes(malformed)
    run('replace', 3)
    assert file.read_bytes() == malformed
    assert list(directory.iterdir()) == [file]
print('Native Win32 profile store: exclusive create, replace, reopen, Python interchange, malformed preservation, Unicode path and existing-directory checks passed')
