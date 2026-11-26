"""Compare native SSC launcher projection with the existing offline Python reference."""
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import ssc_profiles as s

book = {'schema': 1, 'active': 'Training', 'profiles': {
    'Training': s.defaults(), 'Tryhard': dict(s.defaults(), auto_record=True)}}
for ordered, enabled, override in ((False, True, True), (True, False, True), (True, True, False), (True, True, True)):
    state = {'active': {'order': ['ssc', 'autre 🏂'] if ordered else ['autre 🏂'],
                        'enabled': ['ssc'] if enabled else [],
                        'configuration': {'ssc': {'load_diagnostic': True, 'profile_name': 'Tryhard',
                            'use_launcher_overrides': override, 'bitrate_mbps': 12.0, 'fps': '60',
                            'package_directory': 'D:/Runs/épreuve/🏂'},
                            'autre 🏂': {'arbitrary': [None, -1.25, 1e80]}}},
             'presets': {'ignored': {'order': ['ssc'], 'enabled': ['ssc']}}}
    raw = json.dumps(state, ensure_ascii=False).encode('utf-8')
    expected = s.resolve(book, state)
    result = subprocess.run([sys.argv[1], '--resolve'], input=raw, capture_output=True, timeout=10)
    assert result.returncode == 0, result.stderr
    actual = json.loads(result.stdout)
    assert actual == {key: expected[key] for key in actual}, (actual, expected)
for raw in (b'{"active":{"configuration":{"ssc":{"bitrate_mbps":12.5}}}}',
            b'{"active":{"configuration":{"ssc":{"load_diagnostic":1}}}}',
            b'{"active":{"configuration":{"ssc":{"profile_name":"Missing"}}}}',
            b'{"active":{"order":["\xff"]}}', b'{"active":{},"active":{}}'):
    result = subprocess.run([sys.argv[1], '--resolve'], input=raw, capture_output=True, timeout=10)
    assert result.returncode == 2, (result.returncode, result.stderr)
print('Native/Python launcher projection: four precedence/gating cases and five invalid inputs passed')
