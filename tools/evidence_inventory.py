"""Offline inventory for explicitly selected, closed evidence files; not an export or verdict."""
import hashlib
from pathlib import Path
import re
import stat
from baseline_snapshot import linked

MAX_BYTES = 16 * 1024**3
ROLES = {'video', 'replay', 'telemetry', 'timing', 'diagnostic'}


def safe_name(name):
    if (not isinstance(name, str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,126}', name)
            or name.endswith('.') or name.lower().split('.')[0] in
            {'con', 'prn', 'aux', 'nul', *(f'com{i}' for i in range(1, 10)), *(f'lpt{i}' for i in range(1, 10))}
            or name.lower() in ('manifest.json', 'sha256sums.txt')):
        raise ValueError('unsafe or reserved evidence filename')
    return name


def inventory(root, selected):
    """Return deterministic metadata only. Caller owns trusted staging and finalization."""
    root = Path(root)
    if linked(root) or not root.is_dir():
        raise ValueError('expected real staging directory')
    if not isinstance(selected, dict) or not 1 <= len(selected) <= 64:
        raise ValueError('invalid selection')
    names = [safe_name(name) for name in selected]
    if len({name.lower() for name in names}) != len(names):
        raise ValueError('case-insensitive name collision')
    result = []
    for name in sorted(names):
        role = selected[name]
        if not isinstance(role, str) or role not in ROLES:
            raise ValueError('unknown evidence role')
        path = root / name
        if linked(path):
            raise ValueError('linked evidence')
        before = path.stat()
        if not stat.S_ISREG(before.st_mode) or before.st_size > MAX_BYTES:
            raise ValueError('unsupported evidence file')
        total = 0
        digest = hashlib.sha256()
        with path.open('rb') as handle:
            while block := handle.read(1024 * 1024):
                total += len(block)
                if total > MAX_BYTES:
                    raise ValueError('evidence exceeds bound')
                digest.update(block)
        after = path.stat()
        if (total != before.st_size or before.st_size != after.st_size
                or before.st_mtime_ns != after.st_mtime_ns or before.st_ino != after.st_ino):
            raise ValueError('evidence changed during inventory')
        result.append({'path': name, 'size': total, 'sha256': digest.hexdigest(), 'role': role})
    return {'schema': 1, 'scope': 'explicit_file_inventory', 'files': result,
            'complete_package': False, 'eligibility_verified': False}
