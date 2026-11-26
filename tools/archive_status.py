"""Read-only status for one explicit nonlocal run; no publication, cleanup or indexing."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import zipfile
from baseline_snapshot import linked
from index_archive import checked, project


def native_status(executable, database, run_id, report, origin):
    action = 'compare' if report is not None else 'presence'
    data = project(report, origin) if report is not None else (run_id+'\n').encode('ascii')
    result = subprocess.run([str(executable), action, str(database)], input=data,
                            capture_output=True, timeout=30)
    if result.returncode:
        raise RuntimeError('native read-only index unavailable')
    answer = result.stdout.strip().decode('ascii')
    allowed = ('missing', 'identical', 'conflict') if report is not None else ('present', 'missing')
    if answer not in allowed:
        raise RuntimeError('unexpected index status')
    return answer


def inspect(directory, run_id, origin, lookup):
    if origin not in ('imported', 'synthetic') or re.fullmatch('[0-9a-f]{32}', run_id) is None:
        raise ValueError('explicit nonlocal run identity required')
    directory = Path(directory)
    if linked(directory) or not directory.is_dir():
        raise ValueError('private archive directory required')
    final = directory / (origin+'-'+run_id+'.zip')
    pending = final.with_name(final.name+'.partial')
    def inspect_file(path):
        if not os.path.lexists(path):
            return 'absent', None
        try:
            if linked(path) or not path.is_file():
                return 'invalid', None
            report = checked(path, origin)
            if report['run_id'] != run_id:
                return 'identity_mismatch', None
            return 'verified', report
        except (OSError, ValueError, zipfile.BadZipFile, EOFError, RecursionError, TypeError):
            return 'invalid', None
    final_state, final_report = inspect_file(final)
    pending_state, pending_report = inspect_file(pending)
    alias = final_report is not None and pending_report is not None and os.path.samefile(final, pending)
    if final_state == 'absent' and pending_state == 'absent':
        archive = 'absent'
    elif final_state == 'verified' and pending_state == 'absent':
        archive = 'finalized'
    elif final_state == 'absent' and pending_state == 'verified':
        archive = 'pending'
    elif alias:
        archive = 'finalized_with_pending_alias'
    elif final_state == 'verified' and pending_state == 'verified':
        archive = 'separate_pending_conflict'
    else:
        archive = 'review_required'
    report = final_report if final_report is not None else pending_report
    try:
        index = lookup(run_id, report, origin)
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError):
        index = 'unavailable'
    if archive == 'pending' and index == 'missing':
        action = 'retry_publication'
    elif archive in ('finalized', 'finalized_with_pending_alias') and index in ('missing','identical'):
        action = 'retry_index' if archive == 'finalized' and index == 'missing' else (
            'retry_alias_cleanup_and_index' if alias else 'none')
    elif archive == 'absent' and index == 'missing':
        action = 'none'
    else:
        action = 'review'
    return {'run_id':run_id,'origin':origin,'archive':archive,'final':final_state,
            'pending':pending_state,'index':index,
            'manifest_sha256':None if report is None else report['manifest_sha256'],
            'action':action,'eligibility_verified':False}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory',type=Path)
    p.add_argument('run_id')
    p.add_argument('database',type=Path)
    p.add_argument('native',type=Path)
    p.add_argument('--synthetic-fixture',action='store_true')
    a=p.parse_args()
    result=inspect(a.directory,a.run_id,'synthetic' if a.synthetic_fixture else 'imported',
                   lambda i,r,o:native_status(a.native.resolve(),a.database.resolve(),i,r,o))
    print(json.dumps(result,sort_keys=True))
    return 0 if result['action']=='none' else 3


if __name__=='__main__':
    raise SystemExit(main())
