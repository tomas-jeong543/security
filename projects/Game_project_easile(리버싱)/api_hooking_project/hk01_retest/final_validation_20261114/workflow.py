import ast
import hashlib
import json
import re
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8',errors='backslashreplace')
BASE=Path(__file__).resolve().parent
ROOT=BASE.parents[2]
FINAL=ROOT/'evidence/hk01_triple_20261114/final_20261114_112516'
DOC=ROOT/'api_hooking_project/EASILE_HK01_Bot_Aim_Hooking_완성_보고서_간결본.docx'
MSBUILD=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/amd64/MSBuild.exe')

def sha(p):
    with p.open('rb') as s: return hashlib.file_digest(s,'sha256').hexdigest().upper()
def write(p,data):
    if p.exists(): raise RuntimeError('Will not replace '+str(p))
    p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(json.dumps(data,ensure_ascii=False,indent=2) if not isinstance(data,str) else data,encoding='utf-8')
def prepare():
    if (BASE/'before-files.json').exists(): raise RuntimeError('Already prepared')
    for label,args in [('before-status',['status','--short','--','.']),('before-diff',['diff','--','.']),('before-check',['diff','--check','--','.'])]:
        p=subprocess.run(['git',*args],cwd=ROOT,capture_output=True)
        (BASE/(label+'.txt')).write_bytes(p.stdout+p.stderr)
    paths=set()
    for folder in ('src','tests','GameClient','api_hooking_project','docs','evidence','build','tools'):
        paths.update(p for p in (ROOT/folder).rglob('*') if p.is_file() and not p.is_relative_to(BASE))
    write(BASE/'before-files.json',{str(p.relative_to(ROOT)):sha(p) for p in sorted(paths)})
    backup=BASE/'backup'/DOC.name
    backup.parent.mkdir()
    shutil.copy2(DOC,backup)
    assert sha(DOC)==sha(backup)
    write(BASE/'backup/manifest.json',{'original':str(DOC),'backup':str(backup),'sha256':sha(backup)})
    # Extract pure raw PE helpers without executing the prior auditor's top level.
    source=ROOT/'tools/audit_hk01.py'
    nodes=[n for n in ast.parse(source.read_text(encoding='utf-8-sig')).body if isinstance(n,(ast.Import,ast.ImportFrom)) or isinstance(n,ast.ClassDef) and n.name=='PE']
    ns={}; exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),ns)
    exe=FINAL/'bin/Release/GameClient.exe'; pe=ns['PE'](exe)
    mapping=exe.with_suffix('.map').read_text(errors='replace')
    def address(name):
        lines=[l for l in mapping.splitlines() if len(l.split())>3 and l.split()[1].startswith(name) and re.search(r'\s[0-9A-Fa-f]{16}\s+f\s',l)]
        assert len(lines)==1,lines
        return int(re.search(r'\s([0-9A-Fa-f]{16})\s+f\s',lines[0])[1],16)-pe.base
    checked=address('?CheckedSpawnProjectile@GameWorld@@')
    bot=address('?UpdateBot@GameWorld@@AEAAXMAEAVBotHero@@AEAUBotRuntime@@@Z')
    end=next(b for a,b,_ in pe.functions if a==bot)
    write(BASE/'target_rvas.h',f'#pragma once\nconstexpr unsigned long long CHECKED_RVA=0x{checked:X};\nconstexpr unsigned long long SPAWN_RVA=0xF670;\nconstexpr unsigned long long BOT_BEGIN=0x{bot:X};\nconstexpr unsigned long long BOT_END=0x{end:X};\n')
    write(BASE/'target.json',{'exe_sha256':sha(exe),'checked_rva':checked,'bot_begin':bot,'bot_end':end,'spawn_rva':0xF670})
    print(f'PREPARED {len(paths)} original files; DOCX backup={backup}',flush=True)
def b_test():
    project=BASE/'BotBoundaryDebug.vcxproj'
    if not project.exists():
        original=ROOT/'GameClient/Hk01Replay.vcxproj'
        code=original.read_text(encoding='utf-8-sig')
        code=code.replace('..\\tests\\Hk01Replay.cpp',str(BASE/'BotBoundaryDebug.cpp'))
        write(project,code)
    out=BASE/'debug_build'/datetime.now().strftime('%Y%m%d_%H%M%S')
    out.mkdir(parents=True)
    cmd=[str(MSBUILD),str(project),'/p:Configuration=Release','/p:Platform=x64',f'/p:OutDir={out}\\',f'/p:IntDir={out / "obj"}\\','/v:minimal']
    p=subprocess.run(cmd,cwd=BASE,capture_output=True)
    (out/'build.log').write_bytes(p.stdout+p.stderr)
    write(out/'command.json',{'command':cmd,'exitCode':p.returncode})
    if p.returncode: raise RuntimeError('Build failed: '+str(out/'build.log'))
    run=BASE/'bot_run'/datetime.now().strftime('%Y%m%d_%H%M%S')
    run.mkdir(parents=True)
    sources=[(FINAL/'bin/Release/GameClient.exe','GameClient.exe'),(ROOT/'api_hooking_project/hk01_retest/followup_20261114/build/main_spawn_dll_120914_413001/main_spawn.dll','attack.dll'),(ROOT/'api_hooking_project/injector.exe','injector.exe')]
    copies=[]
    for source,name in sources:
        dest=run/name; shutil.copy2(source,dest); assert sha(dest)==sha(source)
        copies.append({'source':str(source.relative_to(ROOT)),'copy':str(dest.relative_to(ROOT)),'sha256':sha(dest)})
    target=json.loads((BASE/'target.json').read_text(encoding='utf-8'))
    assert sha(run/'GameClient.exe')==target['exe_sha256']
    cmd=[str(out/'BotBoundaryDebug.exe'),str(run)]
    p=subprocess.run(cmd,cwd=run,capture_output=True,timeout=35)
    (run/'debug-events.txt').write_bytes(p.stdout+p.stderr)
    write(run/'result.json',{'command':cmd,'exitCode':p.returncode,'copies':copies,'target':target})
    print((p.stdout+p.stderr).decode('utf-8',errors='replace'),flush=True)
    print('RESULT '+str(run/'result.json'),flush=True)
def regression():
    run=BASE/'regression'/datetime.now().strftime('%Y%m%d_%H%M%S'); run.mkdir(parents=True)
    props=run/'Isolated.props'
    write(props,f'''<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><PropertyGroup><OutDir>{run}\\bin\\$(Configuration)\\</OutDir><IntDir>{run}\\obj\\$(ProjectName)\\$(Configuration)\\</IntDir><LinkIncremental>false</LinkIncremental></PropertyGroup><ItemDefinitionGroup><Link><GenerateMapFile>true</GenerateMapFile><GenerateDebugInformation>true</GenerateDebugInformation></Link></ItemDefinitionGroup></Project>''')
    rows=[]
    for config in ('Debug','Release'):
        cmd=[str(MSBUILD),'EASILE.sln',f'/p:Configuration={config}','/p:Platform=x64',f'/p:ForceImportBeforeCppTargets={props}','/m','/v:minimal']
        p=subprocess.run(cmd,cwd=ROOT,capture_output=True)
        (run/f'build-{config}.log').write_bytes(p.stdout+p.stderr)
        rows.append({'kind':'build','configuration':config,'command':cmd,'exitCode':p.returncode})
        print(f'{config} build exit={p.returncode}',flush=True)
        if p.returncode: continue
        cwd=run/(config+'-GameTests'); cwd.mkdir()
        exe=run/'bin'/config/'GameTests.exe'
        p=subprocess.run([str(exe)],cwd=cwd,capture_output=True,timeout=60)
        (cwd/'stdout.txt').write_bytes(p.stdout); (cwd/'stderr.txt').write_bytes(p.stderr)
        lines=p.stdout.decode('utf-8',errors='replace').splitlines()
        rows.append({'kind':'test','configuration':config,'exe':str(exe.relative_to(ROOT)),'sha256':sha(exe),'cwd':str(cwd.relative_to(ROOT)),
            'exitCode':p.returncode,'pass_count':sum(l.endswith(' PASS') for l in lines),'fail_count':sum(' FAIL:' in l for l in lines)})
        print(f'{config} GameTests exit={p.returncode} passes={rows[-1]["pass_count"]}',flush=True)
    write(run/'results.json',rows)
    print('REGRESSION '+str(run/'results.json'),flush=True)
if __name__=='__main__':
    {'prepare':prepare,'b-test':b_test,'regression':regression}[sys.argv[1]]()
