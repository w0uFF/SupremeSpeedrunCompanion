"""Run the standalone Windows probe in a private temporary directory."""
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from archive_publish import create_run
from evidence_archive import verify

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    stage = root / 'stage'
    stage.mkdir()
    output = stage / 'synthetic.mp4'
    subprocess.run([sys.argv[1], str(output), *sys.argv[2:]], check=True, timeout=90)
    if not output.is_file() or not 0 < output.stat().st_size <= 16 * 1024 * 1024:
        raise RuntimeError('missing or oversized synthetic media output')
    if sys.argv[2:] == ['--async-av']:
        final = root / 'synthetic.zip'
        metadata = dict(run_id='1'*32, session_id='2'*32, created_utc='2026-01-02T03:04:05Z',
                        mod_build='v0.0.9-p0-synthetic-probe', origin='synthetic',
                        recording_mode='unknown', attempt_status='unknown', capture_status='finalized',
                        warnings=['Synthetic encoder fixture; no race or gameplay capture.'])
        report = create_run(stage, {'synthetic.mp4': 'video'}, final, metadata)
        if (report != verify(final) or report['schema'] != 2 or report['files_verified'] != 1
                or report['bytes_verified'] != output.stat().st_size or report['complete_submission']
                or report['eligibility_verified'] or final.with_name(final.name+'.partial').exists()):
            raise RuntimeError('synthetic archive verification failed')
        with zipfile.ZipFile(final) as archive:
            if archive.read('synthetic.mp4') != output.read_bytes():
                raise RuntimeError('synthetic archive payload differs')
        print('Synthetic async MP4 archived, published without overwrite, and independently verified; eligibility unverified')
