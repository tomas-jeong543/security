from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from copy import deepcopy
from lxml import etree as E
import hashlib,json
W=Path.cwd();O=W/'.report_work';ref=W/'EASILE_HK01_Bot_Aim_Hooking_빈_보고서_양식.docx';out=W/'EASILE_HK01_Bot_Aim_Hooking_완성_보고서_간결본.docx'
ns={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
def q(x):return '{'+ns['w']+'}'+x
z=ZipFile(ref);root=E.fromstring(z.read('word/document.xml'));body=root.find('w:body',ns);ts=body.findall('w:tbl',ns);ps=body.findall('w:p',ns)
def text(el,s):
 p=el.find('w:p',ns) if el.tag==q('tc') else el
 pp=deepcopy(p.find('w:pPr',ns)) if p.find('w:pPr',ns) is not None else None
 rp=p.find('.//w:rPr',ns);rp=deepcopy(rp) if rp is not None else None
 if el.tag==q('tc'):
  for c in list(el):
   if c.tag!=q('tcPr'):el.remove(c)
  p=E.SubElement(el,q('p'))
  if pp is not None:p.append(pp)
 else:
  for c in list(p):
   if c.tag!=q('pPr'):p.remove(c)
 r=E.SubElement(p,q('r'))
 if rp is not None:r.append(rp)
 for i,line in enumerate(s.split('\n')):
  if i:E.SubElement(r,q('br'))
  t=E.SubElement(r,q('t'));t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=line
def c(t,r,k,s):text(ts[t].findall('w:tr',ns)[r].findall('w:tc',ns)[k],s)
def fill(t,data):
 for r,vals in enumerate(data):
  for k,s in enumerate(vals):c(t,r,k,s)
def hashof(p):return hashlib.sha256((W/p).read_bytes()).hexdigest().upper()
def h(p):s=hashof(p);return s[:32]+'\n'+s[32:]
text(ps[2],'1차 공격 결과와 방어 후보 검토  |  2026년 11월 7일')
fill(0,[['실험 번호','HK-01 / Bot 조준 방향 변조'],['작성자','정명훈'],['수행 기간','2026-11-01 ~ 2026-11-07'],['대상 실행 파일','GameClient.exe / Windows x64'],['개발 / 실행 환경','Visual Studio 2022 / MSVC v143 / MinHook'],['Baseline SHA-256',h('GameClient.exe')],['Attack SHA-256','hook_payload.dll\n'+h('hook_payload.dll')],['Defense SHA-256','']])
c(1,0,0,'DLL을 주입해 Bot의 투사체 방향을 바꾸고, 1차 공격 결과를 확인한다. 이후 기존 DLLGuard의 한계를 분석하고 적용할 방어 후보를 비교한다.')
c(2,0,0,'자체 제작 EASILE의 SpawnProjectile()과 DLLGuard를 분석한다. 근거는 hook_payload.c·injector.c, 현재 EXE/DLL, 실행 로그와 기존 검증 문서다.')
c(2,1,0,'작성 범위는 1차 공격과 방어 후보 검토까지다. 방어 구현·재공격·후속 측정은 작성하지 않는다.')
fill(3,[['공격 단계','DLL 주입 → SpawnProjectile 후킹 → 실제 플레이로 방향 변화 확인'],['방어 단계','기존 DLLGuard의 동작·한계를 분석하고 보완 후보 비교'],['재공격 단계',''],['최종 검증','']])
fill(4,[['확인한 함수','Update() → UpdateBot() → SpawnProjectile()'],['호출 흐름','UpdateBot()이 목표와 조준 방향을 계산하고 SpawnProjectile()이 투사체를 생성한다. 플레이어도 같은 함수를 쓰므로 발사자를 구분해야 한다.\nHeroId: 플레이어 0 / 기존 적 Bot 1 / 아군 Bot 2 / 추가 적 Bot 3. [GameWorld.cpp]']])
fill(5,[['후보 함수','선정 근거','주의 사항'],['UpdateBot()','Bot 공격 로직에 직접 접근','이동·회복·상태 전이에도 영향'],['Normalize()','방향 벡터 변경 가능','이동 등 다른 계산과 공유'],['SpawnProjectile()','생성 직전 방향·발사자 확인','발사자·공격 종류 필터 필요']])
for r,s in enumerate(['SpawnProjectile()은 direction만 바꿔 원본에 전달할 수 있어 최종 대상으로 선택했다.','대상 RVA 0x137E0. 실행 주소는 ASLR이 적용된 GameClient base + RVA다.','진입 14바이트: 48 8B C4 48 89 58 18 55 56 57 41 54 41 55. 현재 EXE와 payload의 검사값이 일치한다.','world는 this, Vec2는 float 2개(8바이트)다. 인자 순서와 크기를 원본 함수에 맞춘다.']):c(6,r,0,s)
fill(7,[['공격 방식','원격 DLL Injection + MinHook Inline Hooking'],['사용한 도구 / DLL','VS2022 x64 / injector.exe / hook_payload.dll'],['Hook 대상 함수','GameWorld::SpawnProjectile(), RVA 0x137E0'],['변조 대상 판별 조건','현재 식: sourceId == 3 || (sourceId == 2 && kind == 0)\nID 3의 모든 공격과 ID 2의 기본 공격이 해당한다. “적 Bot 기본 공격만”이라는 주석보다 범위가 넓다.'],['조준 방향 변조 방식','x′ = 0.8660254x − 0.5y / y′ = 0.5x + 0.8660254y\n코드상 +30° 회전이며 벡터 길이를 거의 보존한다.'],['설치 / 해제 방법','worker에서 대상 검증 → MH_CreateHook → MH_EnableHook. detour가 방향을 바꾸고 trampoline으로 원본을 실행한다.\n종료 요청 시 DisableHook → 실행 중 호출 대기 → RemoveHook. detach에서는 MinHook 정리를 하지 않는다.'],['공격 버전 / 해시','현재 hook_payload.dll은 루트와 out/x64/Release가 동일하다. 해시는 01절에 기재했다. 과거 검증 DLL과는 해시가 다르다.']])
fill(8,[['단계','수행 내용','실행 결과 / 증거'],['01','대상 함수와 인자 분석','SpawnProjectile, RVA 0x137E0 선정'],['02','인젝터로 원격 프로세스 접근','OpenProcess → VirtualAllocEx → WriteProcessMemory로 절대 DLL 경로 전달'],['03','원격 로더 실행','원격 모듈 base + LoadLibraryW RVA → CreateRemoteThread → 완료 대기·모듈 재열거'],['04','Inline Hook 설치','DllMain의 worker가 MinHook 설치. 로드 성공과 비동기 설치 완료는 별도로 확인'],['05','실제 플레이 검증','실험자 정명훈이 직접 플레이하여 공격 방향 변조와 플레이 동작을 확인했다. 정확한 각도·명중률·FPS는 별도 수치 없음.']])
fill(9,[['기존 설치·탐지 기록\nPID 11368: DLL 로드, [HK-01] Hook installed.\nDLLGuard가 payload를 탐지하고 경고 확인 후 종료.\n[HK01_validation_result.md]','직접 플레이 확인\n실험자 정명훈이 실제 플레이 검증을 완료했다고 확인했다. 방향 변조 효과는 플레이로 확인한 결과로 기록한다.\n+30°는 코드상 설정값이며 호출별 각도 실측값과 구분한다.']])
c(10,0,0,'판정: DLL 로드·후킹 설치는 기존 실행 기록으로, 공격 방향 변화와 플레이 동작은 실험자의 직접 플레이로 확인했다. DLLGuard는 별도 기존 실행에서 신규 DLL 탐지·경고 후 종료를 수행했다.')
c(10,1,0,'문제·수정 사항: 양식의 45°를 실제 코드의 +30°로 바로잡았다. 현재 조건은 ID 3 궁극기·ID 2 기본 공격도 포함한다. 인젝터는 절대경로·원격 로더 주소 계산·모듈 재열거를 사용한다. 소스는 수정하지 않았다.')
c(11,0,0,'기존 DLLGuard는 첫 렌더 후 DLL 목록을 baseline으로 저장하고 약 1초마다 새 경로를 비교한다. 허용되지 않은 DLL이면 경고창을 표시하고, 창이 닫히면 ExitProcess(1)로 종료한다. 로드 자체를 선제 차단하는 방식은 아니다.')
c(11,1,0,'핵심 한계는 검사 함수도 같은 프로세스에서 패치할 수 있다는 점이다. Dll2의 실제 소스는 Dll2/Dll2/dllmain.cpp이며 DetectNewModules(RVA 0xEF90)의 원본 호출을 생략한다. 기존 CRC 범위는 이 함수와 SpawnProjectile 진입점을 포함하지 않는다. [IntegrityGuard.cpp·dllmain.cpp]')
text(ps[35],'방어 목표 및 후보 검토')
text(ps[38],'14  방어 후보 기법')
fill(12,[['방어 기법','우선 추천: DLLGuard 무결성 검사 + 핵심 함수 진입부 검사'],['탐지 / 검증 위치','DetectNewModules(0xEF90), SpawnProjectile(0x137E0), 검사 호출 경로'],['검사 조건 / 임계값','빌드별 신뢰 바이트·해시와 비교. 방향 오차 임계값은 정상 공격 표본으로 별도 설정'],['탐지 시 처리','후보: 게임 진행 제한·구조화 로그·독립 재검사. 실제 적용하지 않음'],['방어 적용 파일 / Commit','후보 검토 단계. 새 구현·commit 없음'],['Defense SHA-256','']])
matrix=[['후보 / 순위','동작 원리·기대 효과','구현 난이도','성능 영향','우회 가능성·한계'],['1 DLLGuard 무결성','검사 함수·호출 경로의 해시 비교로 Dll2 패치 탐지','중간','짧은 영역이면 낮음','검사 코드·기준값도 변조 가능'],['1 함수 진입부 검사','핵심 진입 바이트·분기 목적지 대조로 Inline Hook 탐지','낮음~중간','낮음','중간 지점 패치·검사 순간 복원 가능'],['2 독립 다중 탐지','별도 watchdog·외부 관찰자로 검사 누락 교차 확인','중간~높음','낮음~중간','같은 검사 함수를 공유하면 독립성 부족'],['3 Bot 방향·로직 검증','목표 기반 기대 방향과 최종 투사체 방향 비교','중간~높음','발사당 연산 낮음','정상 산개·목표 이동 예외 필요. 검증 데이터 변조 가능'],['4 모듈 신뢰 관리','정확한 경로·서명·해시 관리와 검사 실패 기록','중간','해시·서명 확인 시 증가','정상 DLL 메모리 변조·baseline 오염에 한계']]
ct=deepcopy(ts[16]);rr=ct.findall('w:tr',ns);sample=deepcopy(rr[1])
for r in rr[1:]:ct.remove(r)
for k,s in enumerate(matrix[0]):text(rr[0].findall('w:tc',ns)[k],s)
for vals in matrix[1:]:
 r=deepcopy(sample);ct.append(r)
 for cell,s in zip(r.findall('w:tc',ns),vals):text(cell,s)
widths=[1600,2900,1200,1250,2940]
for g,v in zip(ct.find('w:tblGrid',ns),widths):g.set(q('w'),str(v))
for r in ct.findall('w:tr',ns):
 for cell,v in zip(r.findall('w:tc',ns),widths):cell.find('w:tcPr/w:tcW',ns).set(q('w'),str(v))
ts[12].addnext(ct)
note=deepcopy(ts[11].find('w:tr/w:tc/w:p',ns));text(note,'추천 순서: 두 핵심 함수의 무결성·진입부 검사 → 독립 감시 → 방향·게임 로직 검증. 정상 Windows DLL 오탐을 줄이는 신뢰 정책도 병행한다. 모든 후보는 추천 단계이며 실제 구현하지 않았다.');ct.addnext(note)

# Leave all sections after the defense candidates blank, retaining headings and field labels.
for ti in range(13,24):
 t=ts[ti];rr=t.findall('w:tr',ns)
 for ri,row in enumerate(rr):
  cells=row.findall('w:tc',ns)
  for ki,cell in enumerate(cells):
   retain=(ti in [13,16,17,22] and ri==0) or (ti in [13,16,17,18,19,22] and ki==0)
   if not retain:text(cell,'')

for ti,t in enumerate(body.findall('w:tbl',ns)):
 first=t.findall('w:tr',ns)[0];grid=t.find('w:tblGrid',ns);cells=first.findall('w:tc',ns)
 if grid is not None and len(grid)==len(cells):
  for g,cell in zip(grid,cells):g.set(q('w'),cell.find('w:tcPr/w:tcW',ns).get(q('w')))
 if len(cells)>=3:
  pr=first.find('w:trPr',ns)
  if pr is None:pr=E.SubElement(first,q('trPr'))
  E.SubElement(pr,q('tblHeader'))
 for row in t.findall('w:tr',ns):
  for ht in row.findall('w:trPr/w:trHeight',ns):ht.set(q('hRule'),'atLeast')
  for cell in row.findall('w:tc',ns):
   pr=cell.find('w:tcPr',ns);mar=pr.find('w:tcMar',ns)
   if mar is None:mar=E.SubElement(pr,q('tcMar'))
   for side in ['left','right']:
    a=mar.find('w:'+side,ns)
    if a is None:a=E.SubElement(mar,q(side))
    a.set(q('w'),'70');a.set(q('type'),'dxa')
# Keep headings with their immediate following content.
for p in ps:
 st=p.find('w:pPr/w:pStyle',ns)
 if st is not None and st.get(q('val')) in ['Kicker','PageTitle','Subhead','TinyGap']:
  pr=p.find('w:pPr',ns)
  if pr.find('w:keepNext',ns) is None:E.SubElement(pr,q('keepNext'))
# Empty break paragraphs are replaced by pageBreakBefore to avoid blank pages.
for p in list(body.findall('w:p',ns)):
 if p.find('.//w:br[@w:type="page"]',ns) is not None:
  nxt=p.getnext()
  if nxt is not None and nxt.tag==q('p'):
   pr=nxt.find('w:pPr',ns)
   if pr is None:pr=E.Element(q('pPr'));nxt.insert(0,pr)
   E.SubElement(pr,q('pageBreakBefore'))
  prev=p.getprevious();body.remove(p)
  while prev is not None and prev.tag==q('p') and not ''.join(prev.itertext()).strip():old=prev;prev=prev.getprevious();body.remove(old)
footer=E.fromstring(z.read('word/footer1.xml'))
for t in footer.findall('.//w:t',ns):
 if t.text==' / 6':t.text='' # Keep the original PAGE field without a stale fixed denominator.
changed={'word/document.xml':E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True),'word/footer1.xml':E.tostring(footer,xml_declaration=True,encoding='UTF-8',standalone=True)}
with ZipFile(out,'w',ZIP_DEFLATED) as dest:
 for info in z.infolist():dest.writestr(deepcopy(info),changed.get(info.filename,z.read(info.filename)))
with ZipFile(out) as new:
 assert all(new.read(n)==z.read(n) for n in z.namelist() if n not in changed)
for ti in range(13,24):
 for ri,row in enumerate(ts[ti].findall('w:tr',ns)):
  for ki,cell in enumerate(row.findall('w:tc',ns)):
   retain=(ti in [13,16,17,22] and ri==0) or (ti in [13,16,17,18,19,22] and ki==0)
   if not retain:assert not ''.join(cell.itertext()).strip()
print(out.name,out.stat().st_size,'bytes; all response slots after section14 blank')
