"""Read-only projection of one trusted archive-verifier result, not a verdict."""
import re

# Display/projection bounds mirror the current verifier result, not a ZIP parser.
MAX_PAYLOAD = 1024 ** 3
ROLES = ('video', 'replay', 'telemetry', 'timing', 'diagnostic')
REVIEW_SUFFIXES = frozenset(('exe', 'dll', 'com', 'scr', 'bat', 'cmd', 'ps1', 'psm1',
                             'vbs', 'vbe', 'js', 'jse', 'mjs', 'wsf', 'wsh', 'hta',
                             'msi', 'msp', 'sys', 'lnk', 'url', 'jar', 'py', 'pyw',
                             'sh', 'reg', 'cpl', 'pif', 'ocx', 'so', 'dylib'))
LIMITATIONS = {
    'timing': 'unverified', 'track_difficulty': 'unverified',
    'capture_coverage': 'unverified', 'telemetry_coverage': 'unverified',
    'menu_selection_proof': 'unverified', 'video_playback': 'not_assessed',
    'replay_playback': 'not_assessed', 'reference_baseline': 'not_assessed',
    'gameplay_integrity': 'not_assessed', 'competition_policy': 'not_established',
    'local_provenance': 'not_established', 'payload_safety': 'not_established',
}


def base(verification, reason, assessment='not_assessed'):
    return {'report_version': 1, 'verification': verification, 'reason': reason,
            'assessment': assessment, 'eligibility_verified': False,
            'complete_submission': False, 'checks': dict(LIMITATIONS),
            'archive': None, 'claims': None, 'findings': []}


def require(condition):
    if not condition:
        raise ValueError('malformed verifier result')


def integer(value, low, high):
    return type(value) is int and low <= value <= high


def hex_id(value, length):
    return type(value) is str and len(value) == length and re.fullmatch('[0-9a-f]+', value) is not None


def elapsed(value):
    return value is None or integer(value, 0, 2**63 - 1)


def text_claim(value):
    return value is None or (type(value) is str and 0 < len(value) <= 128 and value.isprintable())


def project(raw):
    """Check only the consumed report boundary; existing verifier owns schema/hash checks."""
    require(type(raw) is dict and type(raw.get('manifest')) is dict)
    schema = raw.get('schema')
    require(type(schema) is int and schema in (1, 2))
    m = raw['manifest']
    require(type(m.get('schema')) is int and m['schema'] == schema and
            m.get('scope') == 'experimental_evidence_archive')
    require(raw.get('integrity_matches_manifest') is True and raw.get('eligibility_verified') is False
            and raw.get('complete_submission') is False)
    files = m.get('files')
    require(type(files) is list and 1 <= len(files) <= 64)
    require(integer(raw.get('files_verified'), 1, 64) and raw['files_verified'] == len(files))
    require(integer(raw.get('bytes_verified'), 0, MAX_PAYLOAD))
    members, names, suspicious = [], set(), []
    for item in files:
        require(type(item) is dict)
        name, role, size, digest = (item.get(k) for k in ('path', 'role', 'size', 'sha256'))
        # Safe bounded display fields, not independent path/member authorization.
        require(type(name) is str and 1 <= len(name) <= 127 and
                re.fullmatch('[A-Za-z0-9][A-Za-z0-9_.-]*', name) is not None)
        require(name.lower() not in names and type(role) is str and role in ROLES and
                integer(size, 0, MAX_PAYLOAD) and hex_id(digest, 64))
        names.add(name.lower())
        members.append({'name': name, 'role': role, 'bytes': size, 'sha256': digest})
        if any(part in REVIEW_SUFFIXES for part in name.lower().split('.')[1:]):
            suspicious.append(name)
    require(sum(x['bytes'] for x in members) == raw['bytes_verified'])
    settings = next((x for x in members if x['name'].lower() == 'attempt-settings.json'), None)
    has_settings_report = 'attempt_settings_sha256' in raw or 'attempt_settings_consistent' in raw
    if schema == 2 and settings is not None:
        require(settings['name'] == 'attempt-settings.json' and settings['role'] == 'diagnostic' and
                raw.get('attempt_settings_consistent') is True and
                raw.get('attempt_settings_sha256') == settings['sha256'])
    else:
        require(not has_settings_report)
    archive = {'schema': schema, 'files_verified': len(members), 'bytes_verified': raw['bytes_verified'],
               'members': members, 'manifest_sha256': None}
    report = base('consistent', 'member_hashes_match', 'insufficient_evidence')
    report['archive'] = archive
    findings = report['findings']
    if schema == 1:
        findings.append({'code': 'unsupported_run_schema'})
    else:
        require(hex_id(raw.get('run_id'), 32) and raw['run_id'] == m.get('run_id') and
                hex_id(m.get('session_id'), 32) and hex_id(raw.get('manifest_sha256'), 64))
        require(m.get('integrity_status') == 'not_assessed' and m.get('policy_eligibility') == 'unknown' and
                m.get('timing_provenance') == 'caller_supplied_unverified' and
                'video_coverage' in m and m['video_coverage'] is None and
                'telemetry_coverage' in m and m['telemetry_coverage'] is None)
        require(m.get('origin') in ('local', 'imported', 'synthetic') and
                m.get('attempt_status') in ('finished', 'restarted', 'abandoned', 'failed', 'unknown') and
                m.get('capture_status') in ('finalized', 'incomplete', 'failed', 'unavailable', 'unknown') and
                m.get('recording_mode') in ('automatic', 'manual', 'training', 'unknown'))
        require('igt_final_ms' in m and elapsed(m['igt_final_ms']) and
                'rta_elapsed_ns' in m and elapsed(m['rta_elapsed_ns']))
        require(m['attempt_status'] == 'finished' or m['igt_final_ms'] is None)
        require(all(k in m and text_claim(m[k]) for k in ('track_id', 'difficulty')))
        video = any(x['role'] == 'video' and x['bytes'] > 0 for x in members)
        replay = any(x['role'] == 'replay' for x in members)
        require(type(m.get('replay_available')) is bool and m['replay_available'] == replay)
        require(m['capture_status'] != 'finalized' or video)
        archive['manifest_sha256'] = raw['manifest_sha256']
        report['claims'] = {k: m[k] for k in ('run_id', 'session_id', 'origin', 'attempt_status',
                            'capture_status', 'recording_mode', 'igt_final_ms', 'rta_elapsed_ns',
                            'track_id', 'difficulty')}
        report['claims'].update(nonempty_video_member=video, replay_member_present=replay,
                                settings_consistency='matches_manifest' if settings else 'not_present')
        if not video:
            findings.append({'code': 'no_nonempty_video'})
        if m['attempt_status'] != 'finished' or m['capture_status'] != 'finalized':
            findings.append({'code': 'attempt_or_capture_not_complete_claim'})
        if m['igt_final_ms'] is None:
            findings.append({'code': 'final_igt_missing'})
        if m['track_id'] is None or m['difficulty'] is None:
            findings.append({'code': 'track_claim_missing'})
    for name in suspicious:
        findings.append({'code': 'executable_looking_name', 'member': name})
    if suspicious:
        report['assessment'] = 'review_required'
    return report


def inspect(verifier):
    """One trusted verifier invocation; adapter errors never become a positive report."""
    if verifier is None:
        return base('unavailable', 'verifier_unavailable')
    try:
        raw = verifier()
    except Exception:
        return base('unavailable', 'verifier_failed')
    try:
        return project(raw)
    except Exception:
        return base('malformed_report', 'verifier_contract_mismatch', 'review_required')
