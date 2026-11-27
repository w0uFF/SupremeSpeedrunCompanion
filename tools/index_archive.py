"""Offline archive admission and retry, not an authenticity or eligibility verifier."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import stat
import subprocess
from archive_publish import publish
from baseline_snapshot import linked
from evidence_archive import verify, stream_digest, MAX_TOTAL, MAX_MANIFEST, CHUNK

# Reserved comparison identity: not an approved competition profile or its digest.
UNCLASSIFIED = hashlib.sha256(b'SSC:unclassified-index-v1').hexdigest()
BOUND = MAX_TOTAL + MAX_MANIFEST + CHUNK


def project(report, origin):
    if report['schema'] != 2 or origin not in ('imported', 'synthetic'):
        raise ValueError('schema-2 evidence and explicit nonlocal origin required')
    m = report['manifest']
    if origin == 'synthetic' and m['origin'] != 'synthetic':
        raise ValueError('synthetic fixture requires declared synthetic origin')
    if m['attempt_status'] not in ('finished', 'restarted', 'abandoned', 'failed'):
        raise ValueError('unknown attempt status cannot be indexed')
    semantics = m['event_semantics_version']
    if not isinstance(semantics, str) or re.fullmatch(r'[!-~]{1,128}', semantics) is None:
        semantics = 'unknown'
    # Deliberately not inferred from free-text profile_id or track name.
    fields = (m['run_id'], 'unclassified', UNCLASSIFIED, semantics,
              report['manifest_sha256'], origin, m['attempt_status'],
              m['igt_final_ms'], m['rta_elapsed_ns'])
    return ('\n'.join('-' if x is None else str(x) for x in fields) + '\n').encode('ascii')


def checked(path, origin):
    report = verify(path, include_manifest=True)
    project(report, origin)
    return report


def validate_guard(expected_projection, expected_container):
    """Both optional guards or neither. Equality to project() validates exact fields."""
    if expected_projection is None and expected_container is None:
        return False
    if (type(expected_projection) is not bytes or not 1 <= len(expected_projection) <= 9 * 129
            or re.fullmatch(rb'(?:[!-~]{1,128}\n){9}', expected_projection) is None
            or type(expected_container) is not tuple or len(expected_container) != 2
            or type(expected_container[0]) is not int or not 22 <= expected_container[0] <= BOUND
            or type(expected_container[1]) is not str
            or re.fullmatch('[0-9a-f]{64}', expected_container[1]) is None):
        raise ValueError('invalid or incomplete archive identity guard')
    return True


def container_identity(path):
    """Bounded whole-ZIP count/hash; no extraction and no new hash implementation."""
    path = Path(path)
    if linked(path):
        raise ValueError('unsupported archive link')
    before = path.stat()
    if not stat.S_ISREG(before.st_mode) or not 22 <= before.st_size <= BOUND:
        raise ValueError('unsupported archive file or size')

    def stamp(info):
        return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)

    with path.open('rb') as handle:
        if stamp(os.fstat(handle.fileno())) != stamp(before):
            raise ValueError('archive changed before read')
        result = stream_digest(handle, before.st_size)
        if stamp(os.fstat(handle.fileno())) != stamp(before):
            raise ValueError('archive changed during read')
    if linked(path) or stamp(path.stat()) != stamp(before) or result[0] != before.st_size:
        raise ValueError('archive changed during read')
    return result


def checked_identity(path, origin):
    """Projection and physical identity on a cooperating, quiescent file."""
    before = container_identity(path)
    report = checked(path, origin)
    projection = project(report, origin)
    if container_identity(path) != before:
        raise ValueError('archive changed during verification')
    return report, projection, before


def guarded_checked(path, origin, expected_projection, expected_container):
    if not validate_guard(expected_projection, expected_container):
        raise ValueError('explicit archive identity guard required')
    report, projection, container = checked_identity(path, origin)
    if projection != expected_projection or container != expected_container:
        raise ValueError('archive snapshot identity conflict')
    return report


def native_index(executable, database, report, origin):
    result = subprocess.run([str(executable), 'insert', str(database)], input=project(report, origin),
                            capture_output=True, timeout=30)
    if result.returncode != 0:
        raise RuntimeError('native index unavailable: ' + result.stderr.decode('utf-8', errors='replace')[:512])
    answer = result.stdout.strip()
    if answer not in (b'inserted', b'identical', b'conflict'):
        raise RuntimeError('invalid native index response')
    return answer.decode('ascii')


def copy_new(source, partial):
    if linked(source) or not source.is_file():
        raise ValueError('unsupported source archive')
    with source.open('rb') as inp, partial.open('xb') as out:
        try:
            count = 0
            while block := inp.read(CHUNK):
                count += len(block)
                if count > BOUND:
                    raise ValueError('archive copy exceeds bound')
                out.write(block)
            out.flush()
            os.fsync(out.fileno())
        except BaseException:
            out.close()
            partial.unlink()  # only own incomplete copy
            raise


def admit(source, directory, origin, index, *, expected_projection=None, expected_container=None):
    """Single owner, explicit trusted private directory; no extraction or overwrite.

    Pending = verified partial or incomplete copy left by interruption.
    Finalized = verified final ZIP; index can be retried idempotently.
    Index failure never deletes an archive. Conflicting bytes remain untouched.
    Opt-in guards require the full project() bytes and whole ZIP (size, SHA-256).
    Guarded retries retain an external selected source distinct from managed slots.
    Existing unguarded admission/CLI and manifest-only dedup remain unchanged.
    """
    guarded = validate_guard(expected_projection, expected_container)
    source, directory = Path(source), Path(directory)
    if linked(directory) or not directory.is_dir():
        raise ValueError('private archive directory required')
    def check(path):
        if guarded:
            return guarded_checked(path, origin, expected_projection, expected_container)
        return checked(path, origin)

    def verify_publication(path):
        if guarded:
            check(source)
        return check(path)

    report = check(source)
    run_id = report['run_id']
    final = directory / (origin + '-' + run_id + '.zip')
    partial = final.with_name(final.name + '.partial')
    digest = report['manifest_sha256']
    if guarded:
        for path in (final, partial):
            if os.path.lexists(path) and os.path.samefile(source, path):
                raise ValueError('guarded source aliases managed archive')
    if os.path.lexists(final):
        current = check(final)
        if current['manifest_sha256'] != digest:
            raise ValueError('archive identity conflict')
        # Recover only the known hardlink alias from interrupted publication.
        if os.path.lexists(partial):
            if linked(partial) or not os.path.samefile(partial, final):
                raise ValueError('separate pending archive requires review')
            publish(final, verify_publication)
    else:
        if not os.path.lexists(partial):
            if guarded:
                check(source)
            copy_new(source, partial)
        # Do not replace even a malformed interrupted partial.
        current = check(partial)
        if current['manifest_sha256'] != digest:
            raise ValueError('pending archive identity conflict')
        publish(final, verify_publication)
    if guarded:
        check(source)
    final_report = check(final)
    if final_report['manifest_sha256'] != digest:
        raise ValueError('final archive changed')
    status = index(final_report, origin)
    return {'run_id': run_id, 'manifest_sha256': digest, 'origin': origin,
            'index': status, 'eligibility_verified': False}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path, help='Schema-2 archive, or managed pending/final archive for retry')
    p.add_argument('directory', type=Path, help='Existing private managed archive folder')
    p.add_argument('database', type=Path, help='Existing private native runs.sqlite')
    p.add_argument('native', type=Path, help='Built standalone ssc_index executable')
    p.add_argument('--synthetic-fixture', action='store_true', help='Explicit developer fixture, never local PB')
    a = p.parse_args()
    result = admit(a.source, a.directory, 'synthetic' if a.synthetic_fixture else 'imported',
                   lambda r, o: native_index(a.native.resolve(), a.database.resolve(), r, o))
    print(result)
    return 3 if result['index'] == 'conflict' else 0


if __name__ == '__main__':
    raise SystemExit(main())
