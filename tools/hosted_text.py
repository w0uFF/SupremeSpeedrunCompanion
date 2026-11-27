"""Offline publishing text composition; never authorizes or sends a request."""
from dataclasses import dataclass
from urllib.parse import urlsplit
import unicodedata

MAX_BYTES = 65536
VIDEO = 'Uploaded automatically by Supreme Speedrun Companion (SSC)'
NOTES = 'Submitted automatically by Supreme Speedrun Companion (SSC)'
MANUAL = 'Recorded using Supreme Speedrun Companion (SSC)'


@dataclass(frozen=True)
class PersonalText:
    notes: str = ''
    description: str = ''


def _public_link(value):
    # Local prototype restriction, not provider acceptance or accessibility.
    if (not isinstance(value, str) or not 1 <= len(value) <= 2048
            or any(not 33 <= ord(c) <= 126 for c in value)
            or any(c in value for c in ('\\', '?', '#'))):
        raise ValueError('expected a public HTTPS package link without query or fragment')
    url = urlsplit(value)
    if (url.scheme != 'https' or not url.hostname or not url.path
            or url.username is not None or url.password is not None
            or url.port not in (None, 443)):
        raise ValueError('expected a public HTTPS package link without credentials')
    return value


def _personal(value):
    if not isinstance(value, str) or len(value) > MAX_BYTES:
        raise ValueError('invalid personal text')
    value = value.replace('\r\n', '\n').replace('\r', '\n')
    if any(unicodedata.category(c) in ('Cc', 'Cs') and c not in '\n\t' for c in value):
        raise ValueError('unsupported control character in personal text')
    if len(value.encode('utf-8')) > MAX_BYTES:
        raise ValueError('personal text exceeds local byte bound')
    return value


def _compose(personal, attribution, url, limit):
    if type(limit) is not int or not 1 <= limit <= MAX_BYTES:
        raise ValueError('invalid text byte limit')
    protected = attribution + '\n\nSupplementary evidence package:\n' + url
    personal = _personal(personal)
    combined = personal + '\n\n' + protected if personal else protected
    if len(combined.encode('utf-8')) > limit:
        raise ValueError('text exceeds limit; shorten personal text or choose manual fallback')
    return combined, protected


def compose(evidence_url, *, mode='easy', saved=PersonalText(), advanced=None,
            automatic_video, automatic_submission, notes_limit, description_limit):
    """Resolve text only for an SSC-recorded run; flags are caller claims.
    Easy uses saved defaults; Advanced accepts a complete edited text pair.
    Limits are explicit UTF-8 byte budgets, not inferred provider limits.
    """
    if (mode not in ('easy', 'advanced') or not isinstance(saved, PersonalText)
            or (advanced is not None and not isinstance(advanced, PersonalText))
            or (mode == 'easy' and advanced is not None)):
        raise ValueError('invalid submission mode or text preferences')
    if type(automatic_video) is not bool or type(automatic_submission) is not bool:
        raise ValueError('explicit publication attribution flags required')
    url = _public_link(evidence_url)
    selected = advanced if mode == 'advanced' and advanced is not None else saved
    notes, protected_notes = _compose(selected.notes, NOTES if automatic_submission else MANUAL, url, notes_limit)
    description, protected_description = _compose(selected.description, VIDEO if automatic_video else MANUAL, url, description_limit)
    return {'mode': mode, 'notes': notes, 'description': description,
            'protected_notes': protected_notes, 'protected_description': protected_description,
            'metadata_verified': False, 'dispatch_allowed': False}
