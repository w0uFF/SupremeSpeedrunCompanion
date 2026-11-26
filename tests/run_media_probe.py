"""Run the standalone Windows probe in a private temporary directory."""
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile
import json
import re

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from archive_publish import create_run
from evidence_archive import verify

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    stage = root / 'stage'
    stage.mkdir()
    output = stage / 'synthetic.mp4'
    result = subprocess.run([sys.argv[1], str(output), *sys.argv[2:]], timeout=90, capture_output=True, text=True)
    print(result.stdout, end='')
    if result.stderr:
        print(result.stderr, file=sys.stderr, end='')
    result.check_returncode()
    if not output.is_file() or not 0 < output.stat().st_size <= 16 * 1024 * 1024:
        raise RuntimeError('missing or oversized synthetic media output')
    if sys.argv[2:] == ['--hd720']:
        matches=re.findall(r'^HDPerformance ([0-9 ]+)$',result.stdout,re.M)
        assert len(matches)==1
        values=list(map(int,matches[0].split()))
        assert len(values)==14
        count,wall,cpu,median,p95,p99,maximum,drain,peak,working,private,size,high,bound=values
        assert count==250 and wall>=9900000 and cpu>0 and 0<=drain<=wall
        assert 0<=median<=p95<=p99<=maximum and peak>=working>0 and private>0
        assert size==output.stat().st_size and 1<=high<=8 and bound==8*1280*720*3//2
        decoded=[tuple(map(int,x)) for x in re.findall(r'^TimedPTS (\d+) (\d+) (\d+)$',result.stdout,re.M)]
        assert len(decoded)==count and all(i==j and e==j*400000 and abs(a-e)<=20000 for j,(i,e,a) in enumerate(decoded))
        assert 'Decoded 250 H264 frames, 1280x720' in result.stdout
        print('720p synthetic encoder resource fixture verified; not a game/capture performance measurement')
    if sys.argv[2:] in (['--async-av'], ['--sustained-av'], ['--extended-av']):
        final = root / 'synthetic.zip'
        metadata = dict(run_id='1'*32, session_id='2'*32, created_utc='2026-01-02T03:04:05Z',
                        mod_build='v0.0.11-p0-synthetic-probe', origin='synthetic',
                        recording_mode='unknown', attempt_status='unknown', capture_status='finalized',
                        warnings=['Synthetic encoder fixture; no race or gameplay capture.'])
        names = ('accepted', 'written', 'first_100ns', 'end_100ns', 'written_duration_100ns', 'internal_gap_100ns',
                 'dropped_full', 'dropped_duration_100ns', 'rejected_invalid', 'discarded')
        streams = {}
        marks = []
        for line in result.stdout.splitlines():
            match = re.fullmatch(r'StreamQuality (video|audio) ([0-9 ]+)', line)
            if match:
                stream = match[1]
                values = [int(value) for value in match[2].split()]
                if stream in streams or len(values) != len(names):
                    raise RuntimeError('invalid stream quality output')
                streams[stream] = dict(zip(names, values))
            mark = re.fullmatch(r'QueueHighWater ([0-9]+)', line)
            if mark:
                marks.append(int(mark[1]))
        if set(streams) != {'video', 'audio'} or len(marks) != 1 or not 1 <= marks[0] <= 32:
            raise RuntimeError('missing stream quality output')
        expected = {'--async-av': 12, '--sustained-av': 250, '--extended-av': 750}[sys.argv[2]]
        for stream in streams.values():
            if (stream['accepted'] != expected or stream['written'] != expected or stream['first_100ns'] != 0
                    or stream['end_100ns'] != expected * 400000 or stream['written_duration_100ns'] != stream['end_100ns']
                    or any(stream[name] for name in names[5:])):
                raise RuntimeError('unexpected synthetic stream statistics')
        quality = stage / 'recording-quality.json'
        quality.write_text(json.dumps({'schema': 1, 'scope': 'synthetic_sink_handoff',
                                      'streams': streams, 'queue_high_water': marks[0],
                                      'gameplay_coverage_verified': False}, sort_keys=True), encoding='utf-8')
        report = create_run(stage, {'synthetic.mp4': 'video', 'recording-quality.json': 'diagnostic'}, final, metadata)
        if (report != verify(final) or report['schema'] != 2 or report['files_verified'] != 2
                or report['bytes_verified'] != output.stat().st_size + quality.stat().st_size or report['complete_submission']
                or report['eligibility_verified'] or final.with_name(final.name+'.partial').exists()):
            raise RuntimeError('synthetic archive verification failed')
        with zipfile.ZipFile(final) as archive:
            if archive.read('synthetic.mp4') != output.read_bytes() or archive.read('recording-quality.json') != quality.read_bytes():
                raise RuntimeError('synthetic archive payload differs')
        print('Synthetic async MP4 and quality sidecar archived and independently verified; eligibility unverified')
