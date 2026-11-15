"""Final evidence assertions only; never starts a game or writes old artifacts."""
import hashlib
import json
import re
from pathlib import Path

BASE = Path(__file__).resolve().parent
ROOT = BASE.parents[1]
RUN = BASE/'run_20261114_115113_007585'
DEST = RUN/'verified-findings.json'
if DEST.exists():
    raise RuntimeError('Do not replace prior verification evidence')

def read(file):
    raw = file.read_bytes()
    return raw.decode('utf-16') if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else raw.decode('utf-8-sig',errors='replace')

def sha(file):
    with file.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest().upper()

report = json.loads(read(RUN/'review.json'))
report['corrections'] = ['Original replay debug-events.txt files are UTF-16, not UTF-8. The initial review.json replay booleans are superseded by this file.']
report['prior_replays_reviewed'] = []
replays = ROOT/'evidence/hk01_triple_20261114/validation_20261114_112244'
for folder in sorted(replays.glob('replay-*')):
    if not folder.is_dir(): continue
    text = read(folder/'debug-events.txt')
    details = {'name':folder.name,'encoding':'utf-16 BOM','hook_installed_log':'Hook installed' in text,
        'installation_rejected':'Target verification failed' in text or 'no hook installed' in text,
        'skipped_count':len(re.findall(r'\[GUARD-BYPASS\] SKIPPED',text)),
        'dll_detected':'New DLL detected' in text,'external_cleanup':'external_termination=1' in text,
        'before_after_entries':[line for line in text.splitlines() if line.startswith('ENTRY ')]}
    report['prior_replays_reviewed'].append(details)
    if 'defended' in folder.name:
        assert details['installation_rejected'] and not details['hook_installed_log'] and details['dll_detected']
    else:
        assert details['hook_installed_log']
        if folder.name.endswith('scanner'): assert details['skipped_count'] == 3 and not details['dll_detected']
        else: assert details['dll_detected']
    assert details['external_cleanup']

assert len(report['prior_replays_reviewed']) == 4
assert report['crc32_algorithm_unchanged']
assert not report['snapshot_manifest']['mismatches'] and not report['current_vs_snapshot_src']
assert not report['preservation']['changed_or_missing']
assert all(r['snapshot_exe_matches_recorded_sha256'] and not r['recorded_timeout'] for r in report['prior_tests_reviewed'])
assert len(report['prior_tests_reviewed']) == 18
for r in report['prior_tests_reviewed']:
    name = r['name']
    if name in ('Hk01SecurityTests-B','Hk01SecurityTests-B-preinit'):
        assert r['recorded_exit']==2 and r['patch_readback_log'] and r['B_detected']
    elif name in ('Hk01SecurityTests-C','Hk01SecurityTests-C-preinit'):
        assert r['recorded_exit']==3 and r['patch_readback_log'] and r['C_detected']
    elif name == 'Hk01SecurityTests-CRC':
        assert r['recorded_exit']==0 and r['patch_readback_log'] and r['crc_rejection_stdout']
    elif name == 'Hk01SecurityTests-A-C-blocked':
        assert r['recorded_exit']==3 and r['patch_readback_log'] and r['C_detected'] and r['A_detected'] and r['probe_identified']
for r in report['new_normal_runs']:
    assert r['exitCode']==0 and not r['timedOut'] and not r['integrity_fail']
    if r['name']=='GameTests': assert r['pass_lines']==49 and r['fail_lines']==0
    if r['name']=='GameClient': assert r['rendered_five'] and r['ultimate_spawn'] and r['app_exit']

restorations = json.loads(read(BASE/'run_20261114_114849_944849/capture-restoration.json'))
report['restoration_verified'] = []
for r in restorations:
    ok = sha(Path(r['target'])) == r['preRunSha256'] == r['restoredSha256']
    assert ok
    report['restoration_verified'].append({'target':r['target'],'sha256':r['preRunSha256'],'restored':ok})

before = json.loads(read(RUN/'preserved-before.json'))
report['final_preservation_recheck'] = {'file_count':len(before),'changed_or_missing':[]}
for relative,expected in before.items():
    file = ROOT/relative
    if not file.exists() or sha(file) != expected:
        report['final_preservation_recheck']['changed_or_missing'].append(relative)
assert not report['final_preservation_recheck']['changed_or_missing']
manifest = json.loads(read(replays/'replay-manifest.json'))
report['replay_manifest_structure'] = {'type':type(manifest).__name__,'count':len(manifest)}
report['replay_manifest'] = manifest
DEST.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'prior_replays':report['prior_replays_reviewed'],'preservation':report['final_preservation_recheck'],
    'manifest_structure':report['replay_manifest_structure'],'manifest_preview':manifest[:3]},ensure_ascii=False,indent=2))
