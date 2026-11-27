"""Offline SSC settings contract and private profile store; no game or launcher writes."""
import copy
import hashlib
import json
import os
from pathlib import Path, PureWindowsPath
import re
import tempfile

SCHEMA = 1
MOD = 'ssc'
MAX_BYTES = 256 * 1024
HOTKEYS = ('Unassigned', 'F5', 'F6', 'F7', 'F8', 'F9', 'F10', 'F11', 'F12')
# id: (launcher control, default, choices or inclusive integer bounds)
FIELDS = {
    'auto_record': ('boolean', False, ()),
    'game_audio': ('boolean', False, ()),
    'resolution': ('dropdown', '720p', ('720p', '1080p', '1440p', '2160p')),
    'fps': ('dropdown', '30', ('30', '60')),
    'bitrate_mbps': ('number', 5, (3, 40)),
    'encoder': ('dropdown', 'auto', ('auto', 'software', 'hardware')),
    'input_overlay': ('boolean', False, ()),
    'igt_overlay': ('boolean', True, ()),
    'rta_overlay': ('boolean', True, ()),
    'overlay_opacity': ('number', 100, (10, 100)),
    'package_directory': ('text', '', ()),
    'retention': ('dropdown', 'finished', ('finished', 'all_attempts', 'pb_only')),
    'min_free_gib': ('number', 5, (1, 100)),
    'auto_package': ('boolean', False, ()),
    'remember_loadout': ('boolean', True, ()),
    'restart_key': ('dropdown', 'Unassigned', HOTKEYS),
    'record_key': ('dropdown', 'Unassigned', HOTKEYS),
    'overlay_key': ('dropdown', 'Unassigned', HOTKEYS),
}


def defaults():
    return {key: spec[1] for key, spec in FIELDS.items()}


def validate_directory(value):
    if not isinstance(value, str) or len(value.encode('utf-16-le', errors='surrogatepass')) // 2 > 240 or any(ord(c) < 32 or ord(c) == 127 or 0xd800 <= ord(c) <= 0xdfff for c in value):
        raise ValueError('invalid package directory')
    if not value:
        return value  # application-managed path, resolved only by future native adapter
    path = PureWindowsPath(value)
    if not re.fullmatch(r'[A-Za-z]:', path.drive) or not path.is_absolute() or len(path.parts) < 2:
        raise ValueError('use an absolute local drive directory, not a root or network share')
    components = re.split(r'[\\/]', value[3:])
    for part in components:
        if (not part or part in ('.', '..') or part.endswith((' ', '.')) or any(c in '<>:"|?*' for c in part)
                or re.fullmatch(r'(?i:CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|COM[1-9¹²³]|LPT[1-9¹²³])', part.split('.')[0].rstrip(' '))):
            raise ValueError('invalid package directory component')
    return str(path)


def settings(values):
    if not isinstance(values, dict) or set(values) != set(FIELDS):
        raise ValueError('settings fields mismatch')
    result = {}
    for key, (kind, _, allowed) in FIELDS.items():
        value = values[key]
        if kind == 'boolean' and type(value) is not bool:
            raise ValueError(key)
        if kind == 'dropdown' and (not isinstance(value, str) or value not in allowed):
            raise ValueError(key)
        if kind == 'number':
            if type(value) not in (int, float) or not allowed[0] <= value <= allowed[1] or int(value) != value:
                raise ValueError(key)
            value = int(value)
        if kind == 'text':
            value = validate_directory(value)
        result[key] = value
    keys = [result[k] for k in ('restart_key', 'record_key', 'overlay_key') if result[k] != 'Unassigned']
    if len(keys) != len(set(keys)):
        raise ValueError('duplicate hotkey')
    return result


def initial():
    base = defaults()
    tryhard = dict(base, auto_record=True, game_audio=True, resolution='1080p', fps='60', bitrate_mbps=15,
                   input_overlay=True, auto_package=True)
    return {'schema': SCHEMA, 'active': 'Training', 'profiles': {
        'Training': base, 'Speedrun Tryhard': tryhard, 'All Tracks': dict(tryhard),
        'Low Performance': dict(tryhard, resolution='720p', fps='30', bitrate_mbps=5)}}


def profile_name(name):
    if not isinstance(name, str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9 _-]{0,47}', name) or name.endswith(' '):
        raise ValueError('invalid profile name')
    return name


def validate_book(book):
    if (not isinstance(book, dict) or set(book) != {'schema', 'active', 'profiles'}
            or type(book['schema']) is not int or book['schema'] != SCHEMA):
        raise ValueError('unsupported profile schema')
    profiles = book['profiles']
    if not isinstance(profiles, dict) or not 1 <= len(profiles) <= 64:
        raise ValueError('profile count')
    cleaned = {profile_name(k): settings(v) for k, v in profiles.items()}
    if len({k.casefold() for k in cleaned}) != len(cleaned) or profile_name(book['active']) not in cleaned:
        raise ValueError('profile identity or active selection')
    return {'schema': SCHEMA, 'active': book['active'], 'profiles': cleaned}


def edit(book, action, name, *, target=None, values=None):
    result = validate_book(book)
    profiles = result['profiles']
    profile_name(name)
    if action == 'create':
        if name in profiles:
            raise ValueError('profile exists')
        profiles[name] = settings(values)
    elif action == 'replace':
        if name not in profiles:
            raise ValueError('missing profile')
        profiles[name] = settings(values)
    elif action in ('duplicate', 'rename'):
        profile_name(target)
        if name not in profiles or target in profiles:
            raise ValueError('profile source or target')
        profiles[target] = copy.deepcopy(profiles[name])
        if action == 'rename':
            del profiles[name]
            if result['active'] == name:
                result['active'] = target
    elif action == 'select':
        if name not in profiles:
            raise ValueError('missing profile')
        result['active'] = name
    elif action == 'delete':
        if name not in profiles or name == result['active']:
            raise ValueError('cannot delete active or missing profile')
        del profiles[name]
    else:
        raise ValueError('unknown action')
    return validate_book(result)


def unique(pairs):
    out = {}
    for key, value in pairs:
        if key in out:
            raise ValueError('duplicate JSON key')
        out[key] = value
    return out


def read_json(path):
    with Path(path).open('rb') as handle:
        raw = handle.read(MAX_BYTES + 1)
    if len(raw) > MAX_BYTES:
        raise ValueError('JSON size limit')
    return json.loads(raw.decode('utf-8-sig'), object_pairs_hook=unique)


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=True, allow_nan=False).encode('ascii')


class ProfileStore:
    """One owner, trusted private directory; no concurrent writer guarantees."""
    def __init__(self, path):
        self.path = Path(path)

    def load(self):
        if self.path.is_symlink():
            raise ValueError('profile symlink')
        return validate_book(read_json(self.path))

    def save(self, book, *, create=False, expected=None):
        raw = canonical(validate_book(book))
        if len(raw) > MAX_BYTES or self.path.is_symlink():
            raise ValueError('profile destination or size')
        if not create:
            current = self.load()  # Never overwrite malformed, unknown-version or absent state.
            if expected is not None and current != validate_book(expected):
                raise ValueError('profile state changed; reopen before saving')
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=self.path.parent, prefix='.ssc-', delete=False) as handle:
                temporary = Path(handle.name)
                handle.write(raw)
                handle.flush()
                os.fsync(handle.fileno())
            if create:
                os.link(temporary, self.path)  # exclusive creation; no replacement
            else:
                os.replace(temporary, self.path)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)


def export_profile(book, name, destination):
    """Share one profile, without its machine-local package directory; never replace a file."""
    book = validate_book(book)
    name = profile_name(name)
    if name not in book['profiles']:
        raise ValueError('missing profile')
    values = dict(book['profiles'][name], package_directory='')
    ProfileStore(destination).save({'schema': SCHEMA, 'active': name, 'profiles': {name: values}}, create=True)


def import_profile(book, source, *, target=None):
    """Import one profile into a copy of the book, preserving the active selection."""
    source = Path(source)
    if source.is_symlink():
        raise ValueError('profile symlink')
    incoming = validate_book(read_json(source))
    if len(incoming['profiles']) != 1:
        raise ValueError('expected a single exported profile')
    name = incoming['active']
    return edit(book, 'create', target if target is not None else name,
                values=dict(incoming['profiles'][name], package_directory=''))


def launcher_manifest():
    controls = [dict(id='load_diagnostic', name='Load inert diagnostic DLL', help_text='Development only; default off.', type='boolean'),
                dict(id='profile_name', name='SSC profile (blank = active)', help_text='Exact saved SSC profile name.', type='text'),
                dict(id='use_launcher_overrides', name='Use launcher settings for next session', help_text='Off: use selected SSC profile. On: use all controls below.', type='boolean')]
    for key, (kind, _, allowed) in FIELDS.items():
        param = dict(id=key, name=key.replace('_', ' ').capitalize(), help_text='Prototype setting; no gameplay effect.', type=kind)
        if kind == 'dropdown':
            param['choices'] = list(allowed)
        if kind == 'number':
            param.update(min=allowed[0], max=allowed[1], step=1)
        controls.append(param)
    return {'name': 'Supreme Speedrun Companion (development prototype)', 'version': '0.0.38-p0',
            'description': 'Inert diagnostic only. No recording, hooks or game changes. Do not install before G1 review.',
            'dlls': [{'path': 'SSCNative.dll', 'initialize': 'SSCInitialize', 'enable_if': 'load_diagnostic'}],
            'parameters': controls, 'configuration': dict(defaults(), load_diagnostic=False, profile_name='', use_launcher_overrides=False),
            'rules': []}


def resolve(book, mods_state):
    """Read-only projection of launcher active SSC settings, not a loader plan."""
    book = validate_book(book)
    if not isinstance(mods_state, dict) or not isinstance(mods_state.get('active'), dict):
        raise ValueError('invalid launcher state')
    active = mods_state['active']
    enabled = active.get('enabled', [])
    order = active.get('order', [])
    config = active.get('configuration', {})
    if not isinstance(enabled, list) or not all(isinstance(x, str) for x in enabled) or not isinstance(order, list) or not all(isinstance(x, str) for x in order) or not isinstance(config, dict):
        raise ValueError('invalid launcher active state')
    overrides = config.get(MOD, {})
    baseline = launcher_manifest()['configuration']
    if not isinstance(overrides, dict) or not set(overrides) <= set(baseline):
        raise ValueError('unknown SSC launcher setting')
    merged = dict(baseline, **overrides)
    for key in ('load_diagnostic', 'use_launcher_overrides'):
        if type(merged[key]) is not bool:
            raise ValueError(key)
    if not isinstance(merged['profile_name'], str):
        raise ValueError('invalid profile selection')
    name = merged['profile_name'] or book['active']
    if name not in book['profiles']:
        raise ValueError('selected profile missing')
    launch_values = settings({k: merged[k] for k in FIELDS})
    effective = launch_values if merged['use_launcher_overrides'] else book['profiles'][name]
    snapshot = {'schema': SCHEMA, 'profile': name, 'settings': effective,
                'source': 'launcher' if merged['use_launcher_overrides'] else 'profile',
                'diagnostic_requested': MOD in enabled and MOD in order and merged['load_diagnostic'], 'native_capabilities': False}
    return dict(snapshot, local_snapshot_sha256=hashlib.sha256(canonical(snapshot)).hexdigest())
