"""Pure Speedrun.com v1 request preview; never a submission or transport."""
from datetime import date
import json
import re
from urllib.parse import urlsplit

# Local serializer bound, not a competition restriction.
MAX_MS = 7 * 24 * 60 * 60 * 1000


def api_id(value):
    if not isinstance(value, str) or re.fullmatch(r'[a-z0-9]{8}', value) is None:
        raise ValueError('unsupported API identifier')
    return value


def preview(category, platform, video, run_date, igt_ms, *, level=None):
    """Explicit IDs supplied by caller; no metadata lookup, approval or dispatch."""
    if type(igt_ms) is not int or not 0 <= igt_ms <= MAX_MS:
        raise ValueError('unsupported IGT for preview')
    if not isinstance(video, str) or not 1 <= len(video) <= 2048 or any(ord(c) <= 32 or ord(c) >= 127 for c in video):
        raise ValueError('invalid video URL')
    url = urlsplit(video)
    if url.scheme != 'https' or not url.hostname or url.username is not None or url.password is not None or '\\' in video:
        raise ValueError('expected HTTPS video URL without credentials')
    if not isinstance(run_date, str) or date.fromisoformat(run_date).isoformat() != run_date:
        raise ValueError('expected ISO date')
    run = {'category': api_id(category), 'platform': api_id(platform), 'date': run_date,
           'times': {'ingame': igt_ms / 1000}, 'video': video, 'emulated': False}
    if level is not None:
        run['level'] = api_id(level)
    # No player override, verified flag, invented region/variables or credential fields.
    return {'scope': 'offline_request_preview', 'method': 'POST', 'path': '/api/v1/runs',
            'body_json': json.dumps({'run': run}, separators=(',', ':'), sort_keys=True),
            'metadata_verified': False, 'dispatch_allowed': False}
