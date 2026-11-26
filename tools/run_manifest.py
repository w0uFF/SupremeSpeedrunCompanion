"""Provisional schema 2 run metadata; caller values are not verified observations."""
from datetime import datetime
import re

NULLABLE_TEXT = ('game_build_fingerprint', 'adapter_version', 'profile_id', 'profile_version',
                 'baseline_id', 'track_id', 'track_name', 'difficulty', 'weather', 'rider_id',
                 'board_id', 'clock_source', 'event_semantics_version')
REQUIRED = ('run_id', 'session_id', 'created_utc', 'mod_build', 'origin', 'recording_mode',
            'attempt_status', 'capture_status')
OPTIONAL = NULLABLE_TEXT + ('igt_final_ms', 'rta_elapsed_ns', 'warnings')
FIXED = {'integrity_status': 'not_assessed', 'policy_eligibility': 'unknown',
         'video_coverage': None, 'telemetry_coverage': None, 'timing_provenance': 'caller_supplied_unverified'}
WARNING = 'Offline metadata; timing, identity, coverage and eligibility are not verified.'


def short_text(value, bound=128):
    return isinstance(value, str) and 0 < len(value) <= bound and all(c.isprintable() for c in value)


def build(metadata, files):
    if not isinstance(metadata, dict) or not set(REQUIRED) <= set(metadata) <= set(REQUIRED + OPTIONAL):
        raise ValueError('unsupported run metadata fields')
    data = {key: metadata.get(key) for key in REQUIRED + OPTIONAL if key != 'warnings'}
    for key in ('run_id', 'session_id'):
        if not isinstance(data[key], str) or re.fullmatch('[0-9a-f]{32}', data[key]) is None:
            raise ValueError('invalid run or session identifier')
    utc = data['created_utc']
    if not isinstance(utc, str) or re.fullmatch(r'\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z', utc) is None:
        raise ValueError('expected canonical UTC timestamp')
    datetime.strptime(utc, '%Y-%m-%dT%H:%M:%SZ')
    if not short_text(data['mod_build']):
        raise ValueError('invalid mod build')
    for key in NULLABLE_TEXT:
        if data[key] is not None and not short_text(data[key]):
            raise ValueError('invalid metadata text')
    fingerprint = data['game_build_fingerprint']
    if fingerprint is not None and re.fullmatch('[0-9a-f]{64}', fingerprint) is None:
        raise ValueError('invalid game fingerprint')
    for key, allowed in (('origin', ('local', 'imported', 'synthetic')),
                         ('recording_mode', ('automatic', 'manual', 'training', 'unknown')),
                         ('attempt_status', ('finished', 'restarted', 'abandoned', 'failed', 'unknown')),
                         ('capture_status', ('finalized', 'incomplete', 'failed', 'unavailable', 'unknown'))):
        if data[key] not in allowed:
            raise ValueError('invalid status')
    for key in ('igt_final_ms', 'rta_elapsed_ns'):
        value = data[key]
        if value is not None and (type(value) is not int or not 0 <= value < 2**63):
            raise ValueError('invalid elapsed value')
    if data['attempt_status'] != 'finished' and data['igt_final_ms'] is not None:
        raise ValueError('unfinished attempt has final IGT')
    if data['rta_elapsed_ns'] is not None and (not data['clock_source'] or not data['event_semantics_version']):
        raise ValueError('RTA requires explicit clock and event semantics')
    if data['capture_status'] == 'finalized' and not any(f['role'] == 'video' and f['size'] > 0 for f in files):
        raise ValueError('finalized capture needs nonempty video member')
    warnings = metadata.get('warnings', [])
    if not isinstance(warnings, list) or len(warnings) > 31 or not all(short_text(w, 256) for w in warnings):
        raise ValueError('invalid warnings')
    return {'schema': 2, 'schema_version': 2, 'scope': 'experimental_evidence_archive', **data, **FIXED,
            'warnings': [WARNING, *warnings], 'replay_available': any(f['role'] == 'replay' for f in files), 'files': files}


def validate(manifest):
    # File member schema and digests are validated separately by the archive reader.
    warnings = manifest.get('warnings')
    if not isinstance(warnings, list) or not warnings or warnings[0] != WARNING:
        raise ValueError('missing unverified warning')
    metadata = {k: manifest.get(k) for k in REQUIRED + OPTIONAL if k != 'warnings'}
    metadata['warnings'] = warnings[1:]
    expected = build(metadata, manifest['files'])
    if type(manifest.get('schema_version')) is not int or type(manifest.get('replay_available')) is not bool or expected != manifest:
        raise ValueError('invalid schema 2 metadata')
