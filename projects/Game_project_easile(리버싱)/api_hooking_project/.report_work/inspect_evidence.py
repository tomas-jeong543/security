from pathlib import Path
import hashlib,json,struct,re,zlib,collections
from zipfile import ZipFile
from docx import Document
W=Path.cwd(); O=W/'.report_work'; O.mkdir(exist_ok=True)
files=[W/'GameClient.exe',W/'hook_payload.dll',W/'injector.exe',W/'Dll2.dll',W/'Dll2/x64/Release/Dll2.dll',W/'EASILE_HK01_starter_fixed/easile_hk01/out/x64/Release/hook_payload.dll',W/'EASILE_HK01_starter_fixed/easile_hk01/out/x64/Release/injector.exe',W/'EASILE_HK01_starter_fixed/easile_hk01/hook_payload.c',W/'EASILE_HK01_starter_fixed/easile_hk01/injector.c',W/'Dll2/Dll2/dllmain.cpp',W/'GameClient.log',W/'guard_observer.log',W/'HK01_validation_result.md',W/'EASILE_HK01_Bot_Aim_Hooking_빈_보고서_양식.docx']
def pe(p):
 b=p.read_bytes(); nt=struct.unpack_from('<I',b,60)[0]; machine,nsec=struct.unpack_from('<HH',b,nt+4); sz=struct.unpack_from('<H',b,nt+20)[0]; opt=nt+24; base=struct.unpack_from('<Q',b,opt+24)[0]; secs=[]
 for i in range(nsec):
  off=opt+sz+i*40; name=b[off:off+8].split(b'\0')[0].decode(); vs,va,rawsz,raw=struct.unpack_from('<IIII',b,off+8); secs.append((name,va,vs,raw,rawsz))
 def data(rva,n):
  for _,va,vs,raw,rs in secs:
   if va<=rva<va+max(vs,rs): return b[raw+rva-va:raw+rva-va+n]
  return b[rva:rva+n]
 def string(rva):return data(rva,512).split(b'\0')[0].decode(errors='replace')
 imports=[]; irva=struct.unpack_from('<I',b,opt+120)[0]
 if irva:
  for i in range(300):
   row=struct.unpack('<IIIII',data(irva+i*20,20))
   if not any(row):break
   thunk=row[0] or row[4]; names=[]
   for j in range(2000):
    v=struct.unpack('<Q',data(thunk+j*8,8))[0]
    if not v:break
    names.append('#'+str(v&0xffff) if v>>63 else string(v+2))
   imports.append([string(row[3]),names])
 result={'machine':hex(machine),'image_base':hex(base),'sections':secs,'imports':imports}
 if p.name=='GameClient.exe':
  result['targets']={hex(r):{'bytes':data(r,n).hex(' ').upper(),'crc32':f'{zlib.crc32(data(r,n)):08X}'} for r,n in [(0x137e0,14),(0xef90,15),(0x14820,0x1444),(0x110d0,0x685)]}
 result['strings']=[s.decode(errors='replace') for s in re.findall(rb'[\x20-\x7e]{9,}',b) if any(q in s for q in [b'GUARD-',b'Hook installed',b'Both DLLs',b'payload.dll',b'Target verification'])]
 return result
out=[]
for p in files:
 b=p.read_bytes(); item={'path':str(p.relative_to(W)),'size':len(b),'sha256':hashlib.sha256(b).hexdigest().upper()}
 if p.suffix.lower() in ['.exe','.dll']: item['pe']=pe(p)
 out.append(item)
g=(W/'guard_observer.log').read_text(encoding='utf8'); events=re.findall(r'SKIPPED #(\d+) pid=(\d+) tid=(\d+)',g); counts=collections.defaultdict(list)
for n,p,t in events: counts[p].append(int(n))
summary={'install_messages':g.count('Hook installed'),'events':len(events),'pids':{p:{'count':len(v),'min':min(v),'max':max(v),'sequence_complete':v==list(range(1,max(v)+1))} for p,v in counts.items()}}
log=(W/'GameClient.log').read_text(encoding='utf8',errors='replace'); lines=log.splitlines()
summary['game']={'lines':len(lines),'projectile_spawns':log.count('[PROJECTILE_SPAWN]'),'match_starts':[{'line':i+1,'text':s} for i,s in enumerate(lines) if '[MATCH][START]' in s],'app_events':[{'line':i+1,'text':s} for i,s in enumerate(lines) if '[APP]' in s],'integrity_fails':log.count('INTEGRITY_FAIL'),'hk01_messages':log.count('[HK-01]')}
(O/'evidence.json').write_text(json.dumps({'files':out,'logs':summary},ensure_ascii=False,indent=2),encoding='utf8')
d=Document(files[-1]); inv={'paragraphs':[(i,p.text,p.style.name) for i,p in enumerate(d.paragraphs)],'tables':[[[c.text for c in r.cells] for r in t.rows] for t in d.tables]}
(O/'template_inventory.json').write_text(json.dumps(inv,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({'files':[{k:v for k,v in x.items() if k!='pe'} for x in out],'logs':summary},ensure_ascii=False,indent=2))
