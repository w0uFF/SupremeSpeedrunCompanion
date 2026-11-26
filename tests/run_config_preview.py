"""Read actual Win32 profile and mods.json files; verify both are unchanged."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import ssc_profiles as s

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    profiles = root / 'profiles-épreuve'
    mods = root / 'mods-épreuve'
    profiles.mkdir()
    mods.mkdir()
    profile_file = profiles / 'profiles.json'
    mods_file = mods / 'mods.json'
    s.ProfileStore(profile_file).save(s.initial(), create=True)
    state = {'active': {'order': ['ssc'], 'enabled': ['ssc'], 'configuration': {'ssc': {
        'profile_name': 'Speedrun Tryhard', 'use_launcher_overrides': True, 'load_diagnostic': True,
        'fps': '60', 'package_directory': 'D:\\Runs\\épreuve\\🏂', 'bitrate_mbps': 12.0}}}}
    mods_file.write_bytes(json.dumps(state, ensure_ascii=False, indent=2).encode('utf-8'))
    before = [profile_file.read_bytes(), mods_file.read_bytes()]
    result = subprocess.run([sys.argv[1], str(profiles), str(mods)], capture_output=True, timeout=10)
    assert result.returncode == 0, result.stderr
    actual = json.loads(result.stdout)
    expected = s.resolve(s.initial(), state)
    assert actual == {key: expected[key] for key in actual}, (actual, expected)
    assert [profile_file.read_bytes(), mods_file.read_bytes()] == before
    assert sorted(x.name for x in profiles.iterdir()) == ['profiles.json']
    assert sorted(x.name for x in mods.iterdir()) == ['mods.json']
    mods_file.write_bytes(b'{"active":{},"active":{}}')
    result = subprocess.run([sys.argv[1], str(profiles), str(mods)], capture_output=True, timeout=10)
    assert result.returncode == 3 and not result.stdout
    assert mods_file.read_bytes() == b'{"active":{},"active":{}}'
    assert profile_file.read_bytes() == before[0]
    mods_file.unlink()
    result = subprocess.run([sys.argv[1], str(profiles), str(mods)], capture_output=True, timeout=10)
    assert result.returncode == 3 and not result.stdout and not mods_file.exists()
print('Win32 native configuration preview: actual files, UTF-8, read-only, malformed/missing refusal passed')
