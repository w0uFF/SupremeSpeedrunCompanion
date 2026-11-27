"""Experimental read-only moderator report. No extraction, admission or verdict."""
import argparse
import json
from pathlib import Path
import stat
import struct
import zipfile
from validator.report import base, inspect

# Existing verifier has no typed error taxonomy. Only explicitly known errors
# receive narrow classifications; ambiguous refusals stay invalid_or_unsupported.
INVALID = frozenset(('truncated archive', 'directory count mismatch', 'invalid member set',
                     'duplicate JSON key', 'invalid manifest entry', 'evidence digest mismatch',
                     'unlisted member', 'noncanonical schema 2 manifest', 'invalid schema 2 metadata',
                     'missing unverified warning', 'unsafe or reserved evidence filename',
                     'invalid member metadata'))


def validate(path, verifier=None):
    """One caller-selected file in a trusted quiescent directory. No path in report."""
    try:
        mode = Path(path).lstat().st_mode
    except (OSError, ValueError, TypeError):
        return base('unavailable', 'source_unavailable')
    if not stat.S_ISREG(mode):
        return base('unsupported_format', 'unsupported_source_type')
    if verifier is None:
        try:
            from evidence_archive import verify as verifier
        except Exception:
            return base('unavailable', 'verifier_unavailable')
    try:
        raw = verifier(Path(path), include_manifest=True)
    except (zipfile.BadZipFile, EOFError, UnicodeError, struct.error, json.JSONDecodeError):
        return base('invalid_package', 'archive_decode_failed')
    except ValueError as exc:
        if str(exc) == 'ZIP64 central directory unsupported':
            return base('unsupported_format', 'zip64_unsupported')
        if str(exc) in INVALID:
            return base('invalid_package', 'archive_validation_failed')
        return base('invalid_or_unsupported', 'verifier_rejected_package')
    except Exception:
        return base('unavailable', 'verifier_failed')
    return inspect(lambda: raw)


def human(report):
    lines = ['SSC offline package inspection (experimental)',
             'Archive verification: ' + report['verification'],
             'Reason: ' + report['reason'], 'Evidence assessment: ' + report['assessment']]
    a = report['archive']
    if a is not None:
        lines.append(f"Schema: {a['schema']}; checked members: {a['files_verified']}; payload bytes: {a['bytes_verified']}")
        if a['manifest_sha256'] is not None:
            lines.append('Manifest SHA-256: ' + a['manifest_sha256'])
    if report['claims'] is not None:
        lines.append('Manifest claims (not observed gameplay):')
        for k, v in report['claims'].items():
            lines.append('  ' + k + ': ' + json.dumps(v, ensure_ascii=True))
    lines.append('Checks outside this verifier scope:')
    for k, v in report['checks'].items():
        lines.append('  ' + k + ': ' + v)
    for f in report['findings']:
        lines.append('Finding: ' + f['code'] + (' [' + f['member'] + ']' if 'member' in f else ''))
    lines.extend(('Matching checksums do not establish authenticity, safe contents, or honest gameplay.',
                  'Missing optional replay is not a violation. No competition eligibility established.'))
    return '\n'.join(lines)


def exit_code(report):
    if report['verification'] == 'unavailable':
        return 4
    if report['verification'] in ('invalid_package', 'invalid_or_unsupported'):
        return 2
    if report['verification'] != 'consistent' or report['assessment'] == 'review_required' or report['archive']['schema'] == 1:
        return 3
    return 0  # Structural consistency ONLY; never eligibility.


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('archive', type=Path, help='One explicitly selected closed archive in a trusted directory')
    p.add_argument('--json', action='store_true', help='Machine-readable report, stdout only')
    a = p.parse_args(argv)
    report = validate(a.archive)
    print(json.dumps(report, ensure_ascii=True, sort_keys=True) if a.json else human(report))
    return exit_code(report)


if __name__ == '__main__':
    raise SystemExit(main())
