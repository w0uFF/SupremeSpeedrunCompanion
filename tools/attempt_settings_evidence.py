"""Validate optional, redacted settings evidence. Consistency only, not runtime proof."""
import json
from ssc_profiles import canonical, settings, unique

NAME = 'attempt-settings.json'
MAX_BYTES = 8192


def validate(raw, manifest):
    if len(raw) > MAX_BYTES:
        raise ValueError('attempt settings member too large')
    try:
        data = json.loads(raw.decode('ascii'), object_pairs_hook=unique)
    except (UnicodeError, RecursionError) as exc:
        raise ValueError('invalid attempt settings encoding') from exc
    if (not isinstance(data, dict) or set(data) != {'schema', 'scope', 'run_id', 'session_id', 'source', 'settings'}
            or type(data['schema']) is not int or data['schema'] != 1
            or data['scope'] != 'attempt_settings_snapshot'
            or data['run_id'] != manifest.get('run_id') or data['session_id'] != manifest.get('session_id')
            or data['source'] not in ('profile', 'launcher', 'session')):
        raise ValueError('attempt settings identity/schema mismatch')
    values = settings(data['settings'])
    if values['package_directory'] != '':
        raise ValueError('private package path in attempt evidence')
    if canonical(dict(data, settings=values)) != raw:
        raise ValueError('noncanonical attempt settings')
