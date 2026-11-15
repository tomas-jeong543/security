"""Check documentation, diffs and preserved files without launching binaries."""
import hashlib
import json
import re
import subprocess
from datetime import datetime
from pathlib import Path

BASE = Path(__file__).resolve().parent
ROOT = BASE.parents[1]
RUN = BASE/'run_20261114_115113_007585'
DOC = ROOT/'docs/hk01-crc-static-review-20261114.md'
files = [DOC,*[p for p in BASE.rglob('*') if p.is_file()]]
OUT = BASE/('checks_'+datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
OUT.mkdir()

def sha(p):
    with p.open('rb') as s: return hashlib.file_digest(s,'sha256').hexdigest().upper()

before = json.loads((RUN/'preserved-before.json').read_text(encoding='utf-8'))
changed = [p for p,h in before.items() if not (ROOT/p).exists() or sha(ROOT/p)!=h]
links = re.findall(r'\]\(([^)]+)\)',DOC.read_text(encoding='utf-8'))
missing = [p for p in links if not p.startswith(('https:','http:','#')) and not (DOC.parent/p.split('#')[0]).exists()]
result = {'approval_scope':'analysis, existing log review and normal test copies only; no new attack DLL or bypass replay',
    'preserved_file_count':len(before),'changed_or_missing':changed,'document_link_count':len(links),'missing_links':missing,'git':[]}
for label,args in [('status',['status','--short','--','.']),('diff',['diff','--','.']),('check',['diff','--check','--','.']),
    ('ignored',['status','--short','--ignored','--',str(BASE.relative_to(ROOT)),str(DOC.relative_to(ROOT))])]:
    p = subprocess.run(['git',*args],cwd=ROOT,capture_output=True)
    (OUT/(label+'.txt')).write_bytes(p.stdout+p.stderr)
    result['git'].append({'args':args,'exitCode':p.returncode})
    if label=='diff':
        result['tracked_diff_unchanged'] = (RUN/'before-diff.txt').read_bytes() == p.stdout+p.stderr
result['new_file_diffs'] = []
for i,file in enumerate(files):
    relative = str(file.relative_to(ROOT))
    p = subprocess.run(['git','diff','--no-index','--','/dev/null',relative],cwd=ROOT,capture_output=True)
    (OUT/f'new-{i:03}.diff').write_bytes(p.stdout+p.stderr)
    result['new_file_diffs'].append({'file':relative,'sha256':sha(file),'exitCode':p.returncode})
assert not changed and not missing and result['tracked_diff_unchanged']
assert all(r['exitCode']==0 for r in result['git'])
assert all(r['exitCode'] in (0,1) for r in result['new_file_diffs'])
(OUT/'final-checks.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'output':str(OUT.relative_to(ROOT)),'preserved_files':len(before),'changed':changed,'links_checked':len(links),
    'missing_links':missing,'tracked_diff_unchanged':result['tracked_diff_unchanged'],'new_files_reviewed':len(files)},ensure_ascii=False,indent=2))
