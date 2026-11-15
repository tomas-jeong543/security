"""Assert installed-hook evidence and preservation; no target processes are started."""
import hashlib
import json
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8',errors='backslashreplace')

BASE=Path(__file__).resolve().parent
ROOT=BASE.parents[2]
OUT=BASE/('verification_'+datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
OUT.mkdir()

def sha(p):
    with p.open('rb') as s: return hashlib.file_digest(s,'sha256').hexdigest().upper()

def function(text, name):
    start=text.index(name)
    begin=text.index('{',start)
    level=0
    for end in range(begin,len(text)):
        if text[end]=='{': level+=1
        if text[end]=='}':
            level-=1
            if level==0: return re.sub(r'\s+','',text[start:end+1])
    raise ValueError(name)

before=json.loads((BASE/'preserved-before.json').read_text(encoding='utf-8'))
changed=[p for p,h in before.items() if not (ROOT/p).exists() or sha(ROOT/p)!=h]
assert not changed,changed
report={'preservation':{'count':len(before),'changed_or_missing':changed},'provenance':[],'runs':[],'builds':[]}
for p in sorted((BASE/'adapted').glob('*/provenance.json')):
    item=json.loads(p.read_text(encoding='utf-8'))
    assert sha(ROOT/item['source'])==item['source_sha256']
    assert sha(ROOT/item['adapted_source'])==item['adapted_sha256']
    assert sha(ROOT/item['target_exe'])==item['target_sha256']
    assert sha(ROOT/item['artifact'])==item['artifact_sha256']
    name='detour_spawn' if 'spawn' in p.parent.name else 'observed_detect_new_modules'
    item['attack_body_unchanged']=function((ROOT/item['source']).read_text(encoding='utf-8-sig'),name)==function((ROOT/item['adapted_source']).read_text(encoding='utf-8-sig'),name)
    assert item['attack_body_unchanged']
    report['provenance'].append(item)
    diff=subprocess.run(['git','diff','--no-index','--',item['source'],item['adapted_source']],cwd=ROOT,capture_output=True)
    (OUT/(p.parent.name+'-adaptation.diff')).write_bytes(diff.stdout+diff.stderr)
    assert diff.returncode==1
for p in sorted((BASE/'build').glob('*/command.json')):
    row=json.loads(p.read_text(encoding='utf-8'))
    assert row['exitCode']==0
    log=(p.parent/'build.log').read_bytes().decode('utf-8',errors='replace')
    report['builds'].append({'command':row,'log':str((p.parent/'build.log').relative_to(ROOT)),
        'warnings':sorted(set(re.findall(r'warning ([A-Z]+[0-9]+)',log)))})

for result in sorted((BASE/'runs').glob('*/results.json')):
    for row in json.loads(result.read_text(encoding='utf-8')):
        folder=ROOT/row['cwd']
        text=(folder/'debug-events.txt').read_bytes().decode('utf-8',errors='replace')
        events=(folder/'GameClient.log').read_text(encoding='utf-8') if (folder/'GameClient.log').exists() else ''
        summary=re.search(r'RETEST_RESULT fixture=(\d+) spawn_E9_observed=(\d+) scanner_E9_observed=(\d+) game_exit=(\d+) self_exit=(\d+)',text)
        assert summary
        record={'folder':row['cwd'],'mode':row['mode'],'runner_exit':row['runner_exit'],'fixture':bool(int(summary[1])),
            'spawn_E9':bool(int(summary[2])),'scanner_E9':bool(int(summary[3])),'game_exit':int(summary[4]),'self_exit':bool(int(summary[5])),
            'hook_installed_log':'Hook installed' in text,'installation_rejected':'Target verification failed' in text,
            'A_detected':'[DETECTED][HK01_A]' in events,'B_detected':'[DETECTED][HK01_B]' in events,'C_detected':'[DETECTED][HK01_C]' in events,
            'projectile_spawned':'[PROJECTILE_SPAWN]' in events,'match_started':'[MATCH][START]' in events,
            'minhook_boundary_verified':'[MINHOOK_ENTRY_VERIFIED]' in events,'bypass_detour_count':len(re.findall(r'\[GUARD-BYPASS\] SKIPPED',text)),
            'external_cleanup':'CLEANUP external_termination=1' in text,'copies':row['copies']}
        for copied in row['copies']:
            assert sha(ROOT/copied['source'])==copied['sha256']==sha(ROOT/copied['copy'])
        if row['mode']=='main_spawn':
            assert record['spawn_E9'] and record['hook_installed_log'] and record['A_detected']
            assert not record['B_detected'] and not record['projectile_spawned'] and not record['match_started']
            assert record['game_exit']==77 and record['external_cleanup']
            record['judgment']='Product A detection confirmed; product B not exercised (menu)'
        elif row['mode']=='main_scanner':
            assert record['scanner_E9'] and record['hook_installed_log'] and record['C_detected'] and record['game_exit']==3
            assert record['self_exit'] and not record['external_cleanup']
            installed=re.search(r'DEBUG ms=(\d+) \[GUARD-OBS\] Hook installed',text)
            detection=re.search(r'DEBUG ms=(\d+) HK01_C',text)
            assert installed and detection
            record['observed_install_to_C_ms']=int(detection[1])-int(installed[1])
            record['judgment']='Product C detection confirmed after real scanner MinHook installation'
        elif record['installation_rejected']:
            assert not record['spawn_E9'] and record['game_exit']==96 and row['runner_exit']==2
            record['judgment']='Failed fixture compatibility attempt retained; not a defense success'
        else:
            assert record['fixture'] and record['spawn_E9'] and record['hook_installed_log'] and record['minhook_boundary_verified']
            assert record['B_detected'] and record['game_exit']==2 and record['self_exit'] and not record['projectile_spawned']
            record['judgment']='Independent fixture B blocks real MinHook at firing boundary; not a product gameplay observation'
        report['runs'].append(record)
assert any(r['mode']=='main_scanner' and r['C_detected'] for r in report['runs'])
assert any(r['fixture'] and r['B_detected'] for r in report['runs'])
for label,args in [('status',['status','--short','--','.']),('diff',['diff','--','.']),('check',['diff','--check','--','.']),
    ('ignored',['status','--short','--ignored','--',str(BASE.relative_to(ROOT))]),('ignore-rule',['check-ignore','-v',str((BASE/'lab.py').relative_to(ROOT))])]:
    p=subprocess.run(['git',*args],cwd=ROOT,capture_output=True)
    (OUT/(label+'.txt')).write_bytes(p.stdout+p.stderr)
    if label=='diff': report['tracked_diff_unchanged']=(BASE/'before-diff.txt').read_bytes()==p.stdout+p.stderr
    if label=='check': assert p.returncode==0
assert report['tracked_diff_unchanged']
sources=[p for p in BASE.rglob('*') if p.is_file() and p.suffix in ('.cpp','.c','.h','.py','.vcxproj') and not any(x in p.parts for x in ('obj','build','verification'))]
for i,file in enumerate(sources):
    p=subprocess.run(['git','diff','--no-index','--','/dev/null',str(file.relative_to(ROOT))],cwd=ROOT,capture_output=True)
    (OUT/f'new-source-{i:02}.diff').write_bytes(p.stdout+p.stderr)
    assert p.returncode in (0,1)
report['new_source_files_reviewed']=[str(p.relative_to(ROOT)) for p in sources]
(OUT/'verified.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'output':str(OUT.relative_to(ROOT)),'preservation':report['preservation'],'runs':[{k:v for k,v in r.items() if k!='copies'} for r in report['runs']],
    'build_warning_codes':[{ 'log':b['log'],'warnings':b['warnings']} for b in report['builds']]},ensure_ascii=False,indent=2))
