"""Offline archive admission and retry, not an authenticity or eligibility verifier."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess
from archive_publish import publish
from baseline_snapshot import linked
from evidence_archive import verify, MAX_TOTAL, MAX_MANIFEST, CHUNK

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


def admit(source, directory, origin, index):
    """Single owner, explicit trusted private directory; no extraction or overwrite.

    Pending = verified partial or incomplete copy left by interruption.
    Finalized = verified final ZIP; index can be retried idempotently.
    Index failure never deletes an archive. Conflicting bytes remain untouched.
    """
    source, directory = Path(source), Path(directory)
    if linked(directory) or not directory.is_dir():
        raise ValueError('private archive directory required')
    report = checked(source, origin)
    run_id = report['run_id']
    final = directory / (origin + '-' + run_id + '.zip')
    partial = final.with_name(final.name + '.partial')
    digest = report['manifest_sha256']
    if os.path.lexists(final):
        current = checked(final, origin)
        if current['manifest_sha256'] != digest:
            raise ValueError('archive identity conflict')
        # Recover only the known hardlink alias from interrupted publication.
        if os.path.lexists(partial):
            if linked(partial) or not os.path.samefile(partial, final):
                raise ValueError('separate pending archive requires review')
            publish(final, lambda p: checked(p, origin))
    else:
        if not os.path.lexists(partial):
            copy_new(source, partial)
        # Do not replace even a malformed interrupted partial.
        current = checked(partial, origin)
        if current['manifest_sha256'] != digest:
            raise ValueError('pending archive identity conflict')
        publish(final, lambda p: checked(p, origin))
    final_report = checked(final, origin)
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
