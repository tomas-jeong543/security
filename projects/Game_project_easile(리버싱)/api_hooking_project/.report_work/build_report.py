from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
from copy import deepcopy
from lxml import etree as E
import json,re,hashlib,os
W=Path.cwd(); O=W/'.report_work'; ref=W/'EASILE_HK01_Bot_Aim_Hooking_빈_보고서_양식.docx'; final=W/os.environ.get('HK01_REPORT_OUTPUT','EASILE_HK01_Bot_Aim_Hooking_완성_보고서.docx')
NS={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
def q(s):return '{'+NS['w']+'}'+s
z=ZipFile(ref); root=E.fromstring(z.read('word/document.xml')); body=root.find('w:body',NS); tables=body.findall('w:tbl',NS); paras=body.findall('w:p',NS)
ev=json.loads((O/'evidence.json').read_text(encoding='utf8')); hs={x['path']:x['sha256'] for x in ev['files']}
def h(p):
 s=hs[p]; return s[:32]+'\n'+s[32:]
def txt(element,text):
 # Keep source paragraph/run formatting while replacing text in the intended slot.
 p0=element.find('w:p',NS) if element.tag==q('tc') else element
 pp=deepcopy(p0.find('w:pPr',NS)) if p0 is not None and p0.find('w:pPr',NS) is not None else None
 rr=p0.find('.//w:rPr',NS) if p0 is not None else None
 rp=deepcopy(rr) if rr is not None else None
 if element.tag==q('tc'):
  for c in list(element):
   if c.tag!=q('tcPr'):element.remove(c)
  dest=element
 else:
  for c in list(element):
   if c.tag!=q('pPr'):element.remove(c)
  dest=None
 parts=text.split('\n\n')
 for n,part in enumerate(parts):
  p=E.SubElement(dest,q('p')) if dest is not None else element
  if dest is not None and pp is not None:p.append(deepcopy(pp))
  r=E.SubElement(p,q('r'))
  if rp is not None:r.append(deepcopy(rp))
  for k,line in enumerate(part.split('\n')):
   if k:E.SubElement(r,q('br'))
   t=E.SubElement(r,q('t'));t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=line
def cell(t,r,c,s):txt(tables[t].findall('w:tr',NS)[r].findall('w:tc',NS)[c],s)
def rows(t,data,start=0):
 for r,line in enumerate(data,start):
  for c,s in enumerate(line):cell(t,r,c,s)
def prose(t,items):
 for r,s in enumerate(items):cell(t,r,0,s)

txt(paras[2],'공격 · 방어 · 재공격 통합 실험 보고서  |  분석 기준일 2026년 11월 7일')
rows(0,[['실험 번호','HK-01 / 3차 API Hooking 공격·방어 실험'],['작성자','정명훈'],['수행 기간','2026-11-01 ~ 2026-11-07\n실행 기록과 현재 보관 자료를 구분하여 분석'],['대상 실행 파일','api_hooking_project/GameClient.exe (Release x64, 454,144 bytes)'],['개발 / 실행 환경','Windows x64 / Visual Studio 2022 / MSVC v143 / MinHook\n분석 방식: 소스·PE·역어셈블리·보관 로그 대조'],['Baseline SHA-256','양식에 기재된 과거 비교본\nBCBCE4F49D56A10AA221CACBA69A88714\nB712B239480E235A5F2F9A1B5E60EBC\n현재 분석 EXE와 다른 파일이다.'],['Attack SHA-256','현재 공격 DLL hook_payload.dll\n'+h('hook_payload.dll')+'\n게임 디스크 파일 해시는 아래 Defense와 같다.'],['Defense SHA-256','현재 GameClient.exe\n'+h('GameClient.exe')+'\nDll2는 별도 우회 DLL이며 24절에 식별값을 기록했다.']])
prose(1,['자체 제작 게임 EASILE에서 DLL Injection으로 공격 코드를 로드하고, GameWorld::SpawnProjectile()을 Inline Hooking하여 Bot 공격 방향을 +30° 회전시키는 원리를 분석한다. 이어서 신규 DLL을 탐지하는 DLLGuard와 그 검사 함수 자체를 후킹하는 재공격을 비교한다.\n\n기존 기록에서는 공격 DLL 로드·후킹 설치와 DLLGuard 경고·종료가 확인되었다. 최신 로그에서는 DLLGuard 원본 검사 생략이 확인된다. 그러나 실제 게임의 동일 발사에 대한 입력·출력 벡터가 없어 +30° 효과와 명중률 변화는 미검증이다.'])
prose(2,['대상은 현재 폴더의 GameClient.exe, hook_payload.dll, injector.exe, Dll2.dll과 대응 소스 및 로그다. 사용자 요청의 Dll2/dll2.c는 현재 파일 목록에 없으며, 실제 프로젝트가 컴파일하는 Dll2/Dll2/dllmain.cpp를 분석했다. 다른 이름의 소스를 임의로 동일 파일이라고 취급하지 않았다.','현재 공격 조건은 (sourceId == 3) || ((sourceId == 2) && (kind == 0))이다. 따라서 ID 3의 기본 공격·궁극기와 ID 2의 기본 공격이 범위에 들어간다. “적 Bot 기본 공격만”이라는 주석·과거 문서와 다르다. 기존 방어는 DLLGuard·일부 CRC·SpeedGuard이며, 추가 함수 무결성 검사와 방향 이상 탐지는 이번 보고서의 추천안이다.'])
rows(3,[['공격 단계','인젝터의 원격 DLL 로딩 및 SpawnProjectile 후킹 구조를 현재 소스·바이너리로 확인하고, 과거 실행 기록과 실제 공격 효과를 분리한다.'],['방어 단계','DLLGuard의 baseline 비교·약 1초 주기 검사·경고·종료와 CRC·SpeedGuard의 검사 범위를 확인한다. 새로운 방어를 적용한 것으로 기록하지 않는다.'],['재공격 단계','Dll2.dll이 DetectNewModules() 진입점을 후킹하고 원본 함수를 호출하지 않는 구조와 SKIPPED 로그를 분석한다.'],['최종 검증','로드·설치·detour 실행·공격 효과·탐지·종료를 각각 판정한다. 이번 작업은 자료 분석과 문서 작성만 수행하며 소스 변경·재빌드·주입·방어 구현은 하지 않았다.']])

rows(4,[['확인한 함수','GameWorld::Update() → UpdateBot() → SpawnProjectile(). UpdateBot이 공격 대상을 고르고 Normalize(목표 위치 − Bot 위치)로 방향을 계산한다. SpawnProjectile은 전달받은 방향으로 투사체를 생성하는 공통 지점이다.'],['호출 흐름','GameWorld.cpp 727~746행에서 Bot 기본 공격·궁극기와 기지 공격이 SpawnProjectile을 호출한다. 플레이어도 911행과 945행 부근에서 호출한다. HeroId()는 Heroes() 배열 인덱스를 반환하므로 player_=0, bot_=1, allyBot_=2, enemyBot_=3이다. team만으로는 같은 진영의 여러 발사자를 구분하지 못한다.\n\nSpawnProjectile(133~144행)은 0 벡터이면 반환하고, CombatSystem::CreateProjectile을 호출한 다음 sourceId와 origin을 저장한다. 현재 PROJECTILE_SPAWN 로그는 진영·kind·damage·direction만 기록하며 sourceId와 후킹 전 방향은 기록하지 않는다.']])
cell(4,1,1,'GameWorld.cpp 727~746행의 Bot 기본 공격·궁극기·기지 공격과 911·945행 부근의 플레이어 공격이 공통 함수를 호출한다. HeroId()는 Heroes() 배열 인덱스이므로 player_=0, bot_=1, allyBot_=2, enemyBot_=3이다. team만으로 발사자를 구분할 수 없다.\n\nSpawnProjectile(133~144행)은 0벡터를 거부하고 CreateProjectile로 생성한 뒤 sourceId·origin을 저장한다. 로그에는 진영·kind·damage·direction만 있어 발사자와 후킹 전 방향은 알 수 없다.')
rows(5,[['후보 함수','선정 근거','주의 사항'],['GameWorld::UpdateBot()','목표 선택과 Bot 발사 경로를 직접 분석할 수 있다.','이동·회복·상태 전이가 함께 있어 패치 영향이 넓다.'],['Normalize()','방향 정규화 직후 값을 바꿀 수 있다.','이동과 다른 벡터 계산도 공유하므로 공격 외 동작이 변할 수 있다.'],['GameWorld::SpawnProjectile()','투사체 생성 직전의 direction, kind, sourceId를 인자로 전달받는다.','플레이어와 여러 Bot이 공유하므로 조건식이 정확해야 한다. 현재 조건은 의도보다 넓다.']])
prose(6,['최종 대상은 GameWorld::SpawnProjectile()이다. 이미 계산된 direction만 바꾸고 world·team·position·kind·damage·sourceId를 원본 함수로 전달할 수 있어 공격 지점을 좁히기 쉽다. 단, 이 함수에서 목표를 다시 정하는 것은 아니며 원래 조준값을 회전시키는 방식이다.','현재 EXE의 RVA는 0x137E0이다. preferred ImageBase 0x140000000 기준 VA는 0x1400137E0이지만, 실행 시 주소는 ASLR이 반영된 실제 모듈 base + RVA로 계산한다. IAT에 등록된 외부 API가 아니라 EXE 내부 함수이므로 이 실험에서는 Inline Hooking을 사용한다.','디스크의 RVA 0x137E0 진입 14바이트는 48 8B C4 48 89 58 18 55 56 57 41 54 41 55로 payload의 expected 배열과 일치한다. payload는 DOS/NT 서명, AMD64·PE32+, 이미지 범위와 VirtualQuery 결과도 검사한다. 이는 대상 오인 방지이며 EXE 전체 SHA-256을 실제로 검사하는 코드는 아니다.','ABI는 void(world, int team, Vec2 position, Vec2 direction, int kind, int damage, uint64_t sourceId)로 맞춘다. world가 C++ 멤버 함수의 this를 대신하며 Vec2는 float 2개, 8바이트다. x64에서 이를 다른 크기나 인자 순서로 선언하면 방향 외 인자까지 손상될 수 있다. 현재 소스의 크기 검사는 정적 조건이며 실제 게임의 모든 호출에 대한 ABI 검증을 뜻하지는 않는다.'])
cell(6,0,0,'SpawnProjectile을 선택하면 계산된 direction만 바꾸고 나머지 인자를 원본으로 전달할 수 있다. 목표 선택은 유지하고 조준 벡터를 회전시키는 방식이며, 발사자 필터를 통해 영향 범위를 제한한다.')
cell(6,2,0,'RVA 0x137E0의 진입 14바이트는 48 8B C4 48 89 58 18 55 56 57 41 54 41 55로 expected 배열과 일치한다. payload는 DOS/NT·AMD64·PE32+·메모리 범위도 검사한다. 이 검사는 EXE 전체 SHA-256 검증이 아니다.')
cell(6,3,0,'ABI는 void(world, int team, Vec2 position, Vec2 direction, int kind, int damage, uint64_t sourceId)다. world는 this, Vec2는 float 2개(8바이트)다. 크기·인자 순서가 틀리면 호출이 손상될 수 있으며 실게임 ABI 동작 검증은 미완료다.')

rows(7,[['공격 방식','LoadLibraryW 기반 원격 DLL Injection + MinHook Inline Hooking'],['사용한 도구 / DLL','VS2022 x64 / hook_payload.c·injector.c / hook_payload.dll·injector.exe. Dll2.dll은 DLLGuard 후킹용으로 역할을 분리한다.'],['Hook 대상 함수','GameWorld::SpawnProjectile(), GameClient base + 0x137E0'],['변조 대상 판별 조건','현재 식: sourceId == 3 || sourceId == 2 && kind == 0\nC 연산자 우선순위에 따라 ID 3은 kind와 무관하게, ID 2는 Basic(0)일 때 회전한다. team 검사는 없다.'],['조준 방향 변조 방식','x′ = 0.8660254x − 0.5y\ny′ = 0.5x + 0.8660254y\ncos30°·sin30° 회전이며 45°가 아니다. (1,0)은 약 (0.8660254,0.5)가 되고 벡터 길이를 거의 보존한다. 화면 y축이 아래로 증가하면 눈에 보이는 회전 방향은 수학 좌표계와 달라진다.'],['설치 / 해제 방법','DllMain에서 DisableThreadLibraryCalls 후 worker를 생성한다. worker는 MH_Initialize → 대상 검증 → MH_CreateHook → MH_EnableHook을 수행한다. MinHook은 원본 진입부를 분기하도록 바꾸고, 덮인 명령을 실행한 뒤 원본 나머지로 돌아가는 trampoline을 제공한다. detour_spawn은 방향을 바꾼 후 g_original_spawn을 호출한다.\n\nHK01_RequestShutdown()은 event로 worker에 종료를 요청한다. DisableHook 후 active detour를 최대 약 5초 기다리고 RemoveHook·필요 시 Uninitialize한다. 호출이 남으면 trampoline을 유지한다. DllMain detach에서는 MinHook 정리를 하지 않으며, 이 API는 DLL unload 완료를 보장하지 않는다. 실제 정리 경로 실행은 미검증이다.'],['공격 버전 / 해시','현재 payload 115,712 bytes / AMD64(0x8664)\n'+h('hook_payload.dll')+'\n루트와 out/x64/Release의 payload는 동일하다. 과거 검증의 29676909… DLL과 현재 DLL을 같은 버전으로 보지 않는다.']])
rows(8,[['단계','수행 내용','실행 결과 / 증거'],['01','공격 함수와 인자·발사자 식별을 소스로 분석한다.','현재 Heroes 순서상 ID 2=아군 Bot, ID 3=추가 적 Bot이다. ID 1 Bot은 현재 회전 조건에서 제외된다.'],['02','대상 EXE의 PE RVA를 파일 오프셋으로 매핑하여 진입 바이트를 비교한다.','0x137E0의 14바이트 일치. 해시 32A1D0D4…인 EXE에 한정한 판정이다.'],['03','원격 DLL 로드를 위한 접근과 경로 처리를 분석한다.','resolve_dll_path는 injector.exe 폴더 기준 상대경로를 절대경로로 바꾼다. OpenProcess는 CREATE_THREAD·QUERY_INFORMATION·VM_OPERATION·VM_WRITE·VM_READ를 요구한다.'],['04','원격 메모리 및 로더 호출 과정을 확인한다.','VirtualAllocEx(PAGE_READWRITE) → WriteProcessMemory(UTF-16 DLL 경로) → CreateRemoteThread(원격 LoadLibraryW) → 최대 15초 대기. DLL 경로를 쓰는 것이며 이 버퍼가 실행 코드인 것은 아니다.'],['05','DLL 로딩 성공과 비동기 hook 설치를 구분한다.','GetExitCodeThread의 DWORD를 완전한 x64 HMODULE로 쓰지 않고 모듈을 재열거한다. 첫 DLL 설치 로그를 확인한 뒤 Enter로 두 번째 DLL을 진행하는 구조지만, 자동 준비 완료 handshake는 없다. 과거 PID 11368에서는 로드·설치·DLLGuard 탐지가 기록됐으며 +30° 효과는 미검증이다.']])
# Additional detail rows reuse the existing 3-column execution pattern.
def append_row(t,vals):
 row=deepcopy(tables[t].findall('w:tr',NS)[-1]);tables[t].append(row)
 for c,s in zip(row.findall('w:tc',NS),vals):txt(c,s)
append_row(8,['06','원격 LoadLibraryW 주소를 구하고 중복 주입·아키텍처를 점검한다.','GetProcAddress 결과의 실제 소유 모듈을 GetModuleHandleExW로 찾고, 원격 동일 모듈 base + export RVA를 계산한다. 시스템 DLL base가 양쪽에서 같다고 가정하지 않는다. WOW64 대상은 거부하고 같은 basename DLL이 이미 있으면 중단한다. basename 비교만으로 전체 경로를 확정하지는 못한다.'])
append_row(8,['07','실패·부분 완료의 의미를 구분한다.','첫 DLL 실패 시 두 번째는 실행하지 않는다. q/EOF 또는 두 번째 실패 시 첫 DLL이 남을 수 있다. 대기 timeout에서는 원격 스레드가 경로를 쓰고 있을 수 있어 그 버퍼를 해제하지 않는다. 이 경우 성공도 자동 rollback도 보장하지 않는다.'])
append_row(8,['08','현재 바이너리와 문서의 차이를 대조한다.','현재 두 injector 바이너리는 .\\Dll2.dll, .\\hook_payload.dll 문자열을 갖는다. 안내 md와 소스 상단 주석의 payload.dll 기본값 설명은 오래된 내용이다. 루트 injector와 out injector는 해시가 달라 완전히 동일한 빌드라고 판정하지 않는다.'])
rows(9,[['원본 및 기존 방어 실행 기록\nHK01_validation_result.md: PID 11368, injector code=0, 모듈 hook_payload.dll, [HK-01] Hook installed. DLLGuard가 payload 경로를 신규 모듈로 탐지했고 경고 확인 뒤 창·프로세스 종료를 관찰했다.\n이는 2026-11-06 실행의 보관 문서 기록이다. 원시 수집 폴더가 현재 W에 없으므로 이번 분석에서 재실행·직접 관측한 결과로 표현하지 않는다.','최신 공격·재공격 로그\nguard_observer.log 1~2행:\n[GUARD-OBS] Hook installed at GameClient + 0xEF90; original DLLGuard scan still enabled.\n[GUARD-BYPASS] SKIPPED #1 pid=23004 tid=3364\n설치 문구는 원본이 유지된다고 하지만 실제 코드·역어셈블리·후속 SKIPPED 기록은 원본 호출 생략을 가리킨다.']])
prose(10,['1차 공격 판정: 과거 기록 범위에서 DLL 로드·MinHook 설치는 성공했고 DLLGuard 신규 모듈 탐지·경고 확인 후 종료도 성공했다. DLL이 실제 로드됐으므로 “로드 자체 차단”이라고 할 수 없다. 당시 메뉴 상태였으므로 공격 방향 효과가 먼저 발생했는지, 효과를 방어가 막았는지는 미검증이다.','현재 detour_spawn의 역어셈블리(RVA 0x1130)는 0x1160에서 sourceId=3이면 바로 회전 분기로 이동하고, 0x1166~0x116F에서 sourceId=2와 kind=0을 추가 확인한다. 0x1180 부근의 float 상수 0x3F5DB3D7(약 0.8660254)·0x3F000000(0.5) 및 mulss/subss/addss가 현재 소스와 맞는다. 코드상 +30° 연산은 확인되지만 실제 게임 detour 호출·입출력 쌍의 증거를 대신하지 않는다.'])
append_row(10,['기존 오프라인 검증은 docs/HK01_ATTACK_DEFENSE_RESULT.md 6.4절에 기록되어 있다. 당시 관측 사본의 모의 원본 함수 테스트에서 대상 4벡터 (1,0), (0,1), (−1,0), (0.6,−0.8)의 회전각은 29.99999874°~30.00000045°, 길이 오차 최대 약 1.35e−8, 원본 전달은 6회, 종료 코드는 0이었다. 비대상 ID 1 Basic·ID 3 Ultimate는 그대로 전달됐다고 기록했다. 이는 과거 필터의 로컬 계산 결과이며 현재 ID 3 Ultimate도 회전하는 소스의 비대상 검증으로 재사용할 수 없다. 원시 offline-test.log와 테스트 실행 파일은 현재 W에 없어 이번에 재실행하지 않았다.'])

prose(11,['방어 목표는 허용되지 않은 런타임 모듈과 게임 코드 변경을 탐지하고, 변조된 투사체가 게임 로직에 반영되는 것을 줄이는 것이다. 공격자는 로컬 프로세스에 DLL을 로드하고 실행 메모리에 패치를 적용할 수 있는 실험 조건을 가진다. DLLGuard와 공격 DLL이 같은 프로세스에 있다는 점이 핵심 한계다.','현재 위협 모델에서는 공격 대상 함수뿐 아니라 DetectNewModules() 자체도 패치할 수 있다. 따라서 검사 함수가 주기적으로 호출된다는 사실만으로 실제 검사가 수행된다고 믿을 수 없다. 독립 무결성 확인·생존 신호와 게임 결과 검증을 함께 고려해야 한다. 아래 내용은 기존 구현 분석과 방어 후보 추천이며 신규 구현·적용 시험은 하지 않았다.'])
rows(12,[['방어 기법','기존 DLLGuard의 최초 모듈 baseline 비교 + 제한적 System32 SHA-256 허용. 별도 경로로 Update CRC32와 SpeedGuard가 존재한다.'],['탐지 / 검증 위치','Application.cpp: 첫 렌더 후 InitializeDllBaseline(), 메인 루프에서 약 1초마다 DetectNewModules(). IntegrityGuard.cpp: CaptureLoadedDlls() → baseline/reported 집합 비교 → IsVerifiedSystemDll(). 검사 함수 RVA는 현 EXE에서 0xEF90이다.'],['검사 조건 / 임계값','Toolhelp TH32CS_SNAPMODULE로 .dll 확장자 모듈의 전체 경로를 수집하고 소문자화한다. baseline에 없고 reportedDlls에 처음 들어온 경로 중 예외가 아니면 탐지한다. 시스템 예외는 oleacc, clbcatq, threadpoolwinrt, CoreUIComponents, CoreMessaging 5개이며 System32 전체 경로와 저장된 SHA-256이 모두 일치해야 한다. 런타임 서명 검증을 매번 하는 구현은 아니다.'],['탐지 시 처리','[DLLGuard] New DLL detected 출력 → MessageBoxW “게임 파일이 이상합니다.” → 확인 등으로 MessageBox가 반환되면 ExitProcess(1). 모듈 로딩을 선제 거부하는 방식이 아니다. baseline 미초기화나 모듈 스캔 실패 시 반환하므로 검사 불능과 정상 판정을 구별할 필요가 있다.'],['방어 적용 파일 / Commit','기존 ../src/IntegrityGuard.cpp 및 ../src/Application.cpp를 읽기만 했다. 보관 결과에 Windows DLL 오탐 예외 수정 이력이 있으며 이번 보고서 작성에서는 새 방어 코드·commit·게임 빌드를 만들지 않았다.'],['Defense SHA-256','현재 분석 GameClient.exe\n'+h('GameClient.exe')+'\nUpdate: RVA 0x14820, 길이 0x1444, CRC32 29148A8C\nInitialize: RVA 0x110D0, 길이 0x685, CRC32 A8E8248A\n두 CRC는 이번 디스크 재계산과 일치한다. 런타임 전 구간 무결성을 보장하지는 않는다.']])
rows(13,[['회차','시험 조건','실행 결과 / 증거'],['01','과거 무주입 실행 및 오탐 수정 기록','기존 결과 문서: Microsoft 서명이 Valid인 5개 System32 DLL을 신규 탐지한 뒤 정확한 경로·해시 예외를 추가했다. 수정 후 15초 무경고·정상 스모크 CRC·5프레임·code 0 기록이 있다. 장기 플레이 전체 검증은 아니다.'],['02','기존 HK-01 DLL 단독 주입, DLLGuard 활성','보관 기록의 PID 11368: DLL 로드·Hook installed 후 payload 신규 탐지, 경고 확인 후 게임 종료. 공격 효과는 미검증이다.'],['03','2026-11-07 무주입 GUI 재검증','docs 기록: UIAutomationCore.dll·wintypes.dll 신규 탐지와 경고 확인 후 code 1. 메뉴 접근성은 확인했으나 최소화·입력 충돌로 시작·이동·공격·궁극기 검증은 미완료다. 이때 DLL 주입은 없었다.'],['04','현재 Dll2 후킹 로그를 기존 방어와 비교','5개 PID, SKIPPED 1,318회. 현재 Dll2는 원본 DetectNewModules를 호출하지 않는다. 이는 신규 DLL 검사 경로 우회 근거다. 해당 PID별 후속 payload 로드·각도 효과·종료 코드와의 일대일 대응은 자료가 부족하다.']])
prose(14,['재공격 구조: Dll2/Dll2/dllmain.cpp의 observer_worker()가 GameClient base + GUARD_SCAN_RVA(0xEF90)를 구해 MH_CreateHook과 MH_EnableHook을 호출한다. void(void) 형식의 DetectNewModules를 observed_detect_new_modules()로 보낸다. g_original_scan은 trampoline 주소를 받지만 현재 detour에서 호출하지 않는다. 결과적으로 신규 모듈 열거·허용 검사·경고·종료 분기까지 가지 않고 SKIPPED 로그만 남긴 뒤 반환한다.\n\n현재 EXE의 0xEF90 진입 15바이트는 48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18로 Dll2의 entry_bytes와 일치한다. Dll2의 PE·VirtualQuery·진입 바이트 검사는 잘못된 주소 패치를 줄이는 장치이며, 주석의 EXE SHA-256을 실제 비교하는 기능은 아니다.','Dll2.dll의 역할은 방향 변조가 아니라 검사 함수 우회다. DllMain은 worker만 시작하고, detour는 InterlockedIncrement로 번호를 늘려 PID·TID를 guard_observer.log와 OutputDebugString에 남긴다. 파일은 게임 EXE 옆에 append한다. 현재 바이너리의 observed_detect_new_modules(RVA 0x12E0~0x1366)에도 로그 출력 후 ret가 있고 원본 scan 포인터 호출은 없다.\n\n기존 주석의 “observation-only”와 설치 메시지의 “original DLLGuard scan still enabled”는 현재 구현과 모순된다. 최신 실행에서 원본 검사 생략은 확인되지만, Dll2가 baseline 전에 들어갔는지·첫 검사보다 후킹 설치가 빨랐는지의 시각 자료는 없다. Dll2도 일반 로더 목록에 남으므로 후킹 전에 탐지될 경쟁 조건이 있다. 정상 설치·우회가 모든 실행에서 보장되는 것은 아니다.'])
rows(15,[['기존 탐지·종료 증거\nHK01_validation_result.md의 payload 신규 탐지 및 경고 확인 후 종료. docs/HK01_GUI_REVALIDATION.md의 무주입 UIAutomationCore·wintypes 탐지 후 GAME_EXIT code=1.\n스캔은 사후 탐지이며 MessageBox가 떠 있는 동안 즉시 ExitProcess가 실행되는 것은 아니다.','현재 우회 증거 집계\nPID 23004: #1~93 (93회)\nPID 2488: #1~270 (270회)\nPID 11340: #1~336 (336회)\nPID 23416: #1~51 (51회)\nPID 24608: #1~568 (568회)\n총 1,318회·설치 문구 5회. 각 PID 안의 번호는 연속이다. 로그에 timestamp·EXE/DLL hash가 없어 지속 시간·당시 빌드·두 번째 DLL 로드 시각은 확정하지 않는다.']])

rows(16,[['ID','시험 조건','기대 결과','실측 결과','판정'],['TC-H01','현재 EXE와 대상 RVA 대조','빌드·진입 바이트 일치','AMD64, 0x137E0/0xEF90 진입 바이트 일치. CRC 2개 일치','정적 확인'],['TC-H02','과거 HK-01 단독 주입','로드와 후킹 설치','PID 11368 로드·설치 기록. 현재 DLL은 과거 해시와 다름','과거 성공'],['TC-H03','ID 3 Basic의 +30° 회전','동일 호출 입출력 Δθ≈+30°','회전식·바이너리 상수 확인. 게임 내 paired vector 없음','효과 미검증'],['TC-H04','비대상 공격 불변','ID 3 Basic만 변경','현 소스는 ID 3 Ultimate·ID 2 Basic도 변경','조건 불일치'],['TC-H05','DLLGuard 활성 상태의 공격 탐지','신규 payload 탐지·경고·종료','기존 기록의 payload 탐지·GUI 경고 후 종료','과거 성공'],['TC-H06','무주입 정상 GUI 사용','정상 모듈로 경고 없음','docs에 UIAutomationCore·wintypes 경고·code 1','오탐 확인'],['TC-H07','Dll2로 검사 함수 후킹','원본 검사 우회 여부 확인','5개 PID SKIPPED 1,318회, 소스·바이너리 원본 미호출','경로 우회 확인'],['TC-H08','Dll2 뒤 payload 주입과 지속 플레이','후킹·각도·정상 진행 연결','게임 전투·Victory·EXIT 로그는 있으나 PID·DLL·벡터 연결 없음','통합 미검증']])
rows(17,[['측정 항목','Baseline','Attack ON','Defense ON'],['발사 방향 오차 (°)','실측 미수집','코드상 +30°\n실게임 Δθ 미측정','효과 차단 전후 Δθ 미측정'],['명중률 (%)','분모·명중 연결 없음','미측정','미측정'],['탐지 건수 (회)','통제된 동일 조건 집계 없음','현 2-DLL 실행별 탐지 미연결','과거 payload 1실행 탐지 기록. SKIPPED 1,318회는 탐지 건수가 아님'],['효과 차단 건수 (회)','해당 없음','미측정','미측정. 경고·종료로 효과 선제 차단을 추정하지 않음'],['오탐 건수 (회)','문서상 5개 DLL 및 후속 UIAutomationCore·wintypes 사례','해당 실행별 집계 없음','서로 다른 실행·모듈이므로 합계나 오탐률 산정 안 함'],['평균 FPS','미측정','미측정','미측정. smoke 5프레임은 FPS 아님'],['비정상 종료 (회)','원시 세션별 집계 없음','통합 충돌·종료율 미확정','GUI 기록의 code 1 두 사례는 의도된 보안 종료. 크래시로 세지 않음']])
rows(18,[['반복 횟수 / 기간','guard_observer.log: 5개 PID의 연속 카운터를 5개 실행 묶음으로 집계했다. 동일 조건의 5회 통제 반복 실험이나 지속 시간으로 해석하지 않는다. GameClient.log는 14,186행에 테스트·일반 실행이 함께 append되어 있다.'],['테스트 환경 / Seed','현재 EXE는 Release x64이며 GameClient.log의 MATCH START에 seed=0xE4511E가 반복된다. 모든 guard PID와 같은 seed·시각으로 연결되지는 않는다. 측정하지 않은 FPS·명중률·각도값을 0으로 채우지 않았다.'],['로그 / 원본 데이터 경로','W=api_hooking_project. W/GameClient.log, W/guard_observer.log, W/HK01_validation_result.md, W/docs/HK01_ATTACK_DEFENSE_RESULT.md 및 HK01_GUI_REVALIDATION.md. 이전 md가 참조하는 .hk01_validation·C:/TEMPFO~1 원시 수집 폴더는 현 W에서 확인되지 않아 문서에 적힌 범위만 인용했다.\n\n최신 GameClient.log의 마지막 실행 구간: 11285행 WINDOW_CREATED, 11289행 MATCH START, 14184~14185행 BASE_DESTROYED·Victory, 14186행 APP EXIT. 이는 실제 전투 진행·정상 루프 종료 경로의 기록이지만 PID·주입 여부·프로세스 종료 코드가 없어 우회 DLL과 연결한 플레이 성공으로 단정하지 않는다.']])

rows(19,[['공격 결과','소스와 현재 바이너리에서 +30° 회전 연산·원본 호출 경로가 확인된다. 과거 실행의 DLL 로드·설치는 성공이다. 현재 실게임에서의 detour 호출·발사별 +30°·명중률 저하는 미검증이다.'],['방어 결과','기존 DLLGuard는 신규 DLL 탐지·경고 확인 후 종료를 수행했다. 다만 로드·후킹 설치를 원천 차단한 것은 아니며 Windows 정상 모듈 오탐이 남아 있다.'],['재공격 결과','현재 Dll2가 DLLGuard 검사 함수를 우회한 정적·로그 근거가 있다. 5개 PID에서 1,318회 검사 생략이 기록됐다. 두 번째 공격 DLL까지 포함한 전체 공격 효과와 지속 플레이 성공은 미검증이다.'],['기존 방어와 충돌 여부','Update CRC 범위는 0x14820~0x15C63, Initialize 범위는 0x110D0~0x11754이며 SpawnProjectile(0x137E0)·DetectNewModules(0xEF90) 진입점은 둘 다 밖이다. 현재 VerifyUpdateIntegrity는 플레이어 궁극기 분기에서 호출되고 VerifyHeroIntegrity는 src 내 호출이 확인되지 않는다. CRC가 정상이어도 두 후킹을 탐지한다는 보장은 없다. SpeedGuard는 이동량 검사라 방향 변조를 직접 다루지 않는다.'],['정상 플레이 영향','보관 정상 스모크는 5프레임·CRC·code 0 범위에서 통과했다. 후속 자동 GUI 검증은 최소화·입력 보호 오류와 정상 DLL 오탐으로 미완료였다. 최신 전투 로그가 있다는 사실과 자동 GUI 검증 미완료는 다른 실행의 결과이므로 서로 덮어쓰지 않는다.']])
prose(20,['원인 분석 1: 검사 주체도 공격 표면이다. DetectNewModules 진입점이 detour로 바뀌면 원래 모듈 열거와 차단 분기가 전부 사라진다. 이때 메인 루프는 함수 호출이 끝난 것으로 보지만 검사는 실행되지 않았다. 현재 기존 CRC 범위가 이 함수를 덮지 않는 점과 결합되어 단일 검사 경로에 의존한 방어가 약해진다.','원인 분석 2: 로그 문구와 실제 동작은 다를 수 있다. Dll2의 “scan still enabled”는 원본 미호출 코드와 모순된다. hook_payload의 “적 Bot 기본 공격만” 주석도 현재 조건을 설명하지 못한다. 기존 양식의 45° 설명과 sourceId==3 && kind==0 문구 역시 현 소스와 다르므로 각각 +30°와 실제 논리식으로 바로잡았다.','원인 분석 3: GameClient.log의 PROJECTILE_SPAWN 3,517행은 여러 세션 전체 합계다. 진영과 출력 방향만으로 ID 2·3을 특정하거나 원래 조준값과의 차이를 계산할 수 없다. 63개의 INTEGRITY_FAIL도 이전 md에 테스트 로그 혼입이 명시되어 있어 현 공격의 CRC 탐지 횟수로 쓰지 않았다. 따라서 공격 효과와 방어 우회 수준을 나누어 판단해야 한다.'])
prose(21,['현재 자료의 한계는 발사별 추적과 실행 식별이 부족하다는 점이다. 향후 검증에서는 PID·UTC·EXE/DLL SHA-256·후킹 활성 시각·event ID·sourceId·kind·입출력 방향·최종 투사체 방향을 연결하는 것이 좋다. 비영벡터의 Δθ=atan2(xy′−yx′, xx′+yy′)로 동일 발사를 비교하고, 제안 기준 +30°±0.1° 및 길이 보존·비대상 불변을 따로 확인한다. 다른 시점의 Bot 조준각을 비교하면 이동·타깃 변경이 섞인다. 이 수치는 검증 제안이며 측정 결과가 아니다.','방어 후보는 아래와 같이 비교했다. 구현 난이도와 성능 영향은 현재 로컬 C++ 프로젝트를 기준으로 한 정성 평가이며 벤치마크 결과가 아니다. 우선순위는 현재 Dll2가 끊는 검사 경로를 복구하고, 공격 함수의 변조를 직접 검사하는 것에 둔다. 모든 후보는 추천 단계이며 실제 코드는 구현하지 않았다.'])

comparison=[
['우선순위 / 후보','원리와 기대 효과','구현 난이도','성능 영향','우회 가능성과 한계'],
['1A DLLGuard 함수 무결성','DetectNewModules와 검사 호출 지점·검증 함수의 코드 영역을 빌드별 신뢰 기준값과 비교한다. Dll2의 진입 패치를 직접 잡는다.','중간. 함수 범위·재배치·빌드별 기준 관리와 검사 실패 정책 필요.','짧은 영역 주기 검사는 낮음. 검사 범위를 넓히면 비용 증가.','같은 프로세스의 검사 코드·기준값도 변조 가능. 최초 baseline이 이미 오염되면 실패. 독립 경로 필요.'],
['1B 함수 진입부 검사','SpawnProjectile(0x137E0)·DetectNewModules(0xEF90)의 신뢰 진입 바이트·분기 목적지를 검사한다. 현재 MinHook 패치를 좁은 비용으로 포착한다.','낮음~중간. 빌드별 명령 경계·허용 코드 변경 식별 필요.','매 프레임 전체 해시보다 낮음. 호출 주기·탐지 지연 균형 필요.','E9 등 한 opcode만 보면 다른 패치 형식을 놓친다. 중간 지점 패치·데이터 변조·검사 순간 복원에는 한계.'],
['2 독립 다중 탐지 경로','메인 루프 검사와 별도 watchdog이 코드 무결성·모듈 목록·검사 heartbeat를 확인한다. 가능하면 외부 관찰자로 한 경로 무력화를 교차 확인한다.','중간~높음. 스레드 동기화·IPC·오탐 처리·권한 설계 필요.','낮음~중간. 중복 열거·메모리 읽기·IPC 주기에 비례.','다른 스레드가 같은 DetectNewModules만 호출하면 독립성이 없다. 외부 관찰자도 동일 권한 공격에 절대 안전하지 않다.'],
['3 Bot 방향·게임 로직 검증','발사 순간 sourceId·목표 위치로 기대 방향을 독립 계산하고 최종 투사체 방향과 각도 차이를 비교한다. 아군 Basic·적 Ultimate까지 넓어진 조건도 관찰한다.','중간~높음. 이동 목표·산개·궁극기·기지 공격 등 정당한 예외 모델 필요.','발사당 벡터 연산은 낮음. 상세 파일 로그·대량 분석은 비용 증가.','공격자가 목표 정보나 검증 경로도 바꿀 수 있다. 허용 오차 안의 작은 변조·정상 산개에 대한 오탐 고려.'],
['4 모듈 신뢰·실패 처리 보강','승인된 전체 경로·서명·해시 정책으로 정상 접근성 DLL 오탐을 줄이고 baseline 획득·스캔 실패를 별도 보안 상태로 기록한다.','중간. OS 업데이트·정상 지연 로드·허용 목록 갱신 절차 필요.','모듈 열거 낮음~중간. 해시·서명 확인은 변경 시 수행·캐시 권장.','허용된 DLL의 실행 메모리 변조나 baseline 전 오염은 별도 문제다. System32 전체를 무조건 허용해서는 안 된다.'],
['5 실행 메모리·포인터 점검','예상 .text 영역·실행 페이지 보호·주요 함수 포인터 목적지를 대조하고 외부 사설 실행 영역 분기를 보조 탐지한다.','중간. 예외적 정상 실행 영역과 이미지 매핑 구분 필요.','전체 주소 공간 탐색은 중간 이상. 핵심 영역 우선 검사 권장.','정상 이미지 내부 코드 악용·메모리 속성 복원 가능. RWX가 없다고 후킹 부재를 의미하지 않는다.'],
['보조 빌드·관측 품질','배포 빌드 manifest·해시·심볼을 고정하고 CFG 등 적용 가능한 컴파일러 보호와 구조화 로그를 검토한다. 증거 연결과 일부 간접 호출 보호를 돕는다.','낮음~중간. 빌드 호환성·성능 확인 필요.','대체로 낮음이나 본 프로젝트 실측 없음.','CFG는 정상 함수 진입부에 대한 Inline Hook을 전부 탐지하는 기법이 아니다. 로컬 클라이언트만으로 완전한 신뢰 보장 불가.']]
ct=deepcopy(tables[16]); trs=ct.findall('w:tr',NS)
for r in trs[1:]:ct.remove(r)
for c,s in zip(trs[0].findall('w:tc',NS),comparison[0]):txt(c,s)
sample=deepcopy(tables[16].findall('w:tr',NS)[1])
for vals in comparison[1:]:
 r=deepcopy(sample);ct.append(r)
 for c,s in zip(r.findall('w:tc',NS),vals):txt(c,s)
# Give comparison columns deliberate widths in the original template's 9890-twip content area.
widths=[1500,2850,1450,1450,2640]
grid=ct.find('w:tblGrid',NS)
for c,v in zip(grid,widths):c.set(q('w'),str(v))
for r in ct.findall('w:tr',NS):
 for c,v in zip(r.findall('w:tc',NS),widths):c.find('w:tcPr/w:tcW',NS).set(q('w'),str(v))
tables[21].addnext(ct)
note=deepcopy(tables[21].find('w:tr/w:tc/w:p',NS));txt(note,'추천 순서는 1A와 1B를 묶어 핵심 두 함수 및 검사 호출 경로를 보호하고, 2의 독립 감시로 검사 누락을 확인한 뒤, 3의 발사 결과 검증을 추가하는 것이다. 4의 정상 모듈 정책과 실패 처리는 병행할 필요가 있다. 5와 컴파일러 보호는 보조 수단으로 적용한다. 검사 코드와 기준값까지 같은 주소 공간에 있는 한 잔여 우회 가능성은 남는다.');ct.addnext(note)

rows(22,[['번호','증거 / 산출물','파일명 또는 저장 경로'],['E-01','공격 소스·현재 payload','EASILE_HK01_starter_fixed/easile_hk01/hook_payload.c\nhook_payload.dll / out/x64/Release/hook_payload.dll\nSHA-256\n'+h('hook_payload.dll')],['E-02','인젝터 소스·실행 파일','EASILE_HK01_starter_fixed/easile_hk01/injector.c\n루트 injector.exe SHA-256\n'+h('injector.exe')+'\nout/x64/Release/injector.exe SHA-256\n'+h('EASILE_HK01_starter_fixed\\easile_hk01\\out\\x64\\Release\\injector.exe')],['E-03','DLLGuard 후킹 소스·DLL','Dll2/Dll2/dllmain.cpp (vcxproj의 ClCompile 항목)\nDll2.dll = Dll2/x64/Release/Dll2.dll\nSHA-256\n'+h('Dll2.dll')+'\n요청에 적힌 Dll2/dll2.c는 현 목록에 없음.'],['E-04','대상 게임·실행 로그','GameClient.exe / GameClient.log\nEXE SHA-256\n'+h('GameClient.exe')+'\n로그 SHA-256\n'+h('GameClient.log')],['E-05','검사 생략 로그','guard_observer.log / 총 설치 문구 5회, SKIPPED 1,318회\nSHA-256\n'+h('guard_observer.log')],['E-06','기존 결과와 GUI 검증 기록','HK01_validation_result.md\ndocs/HK01_CONTEXT_HANDOFF.md\ndocs/HK01_ATTACK_DEFENSE_RESULT.md\ndocs/HK01_GUI_REVALIDATION.md\n기존 문서의 과거 성공·미검증은 해당 실행 범위로 제한하여 인용했다.']])
append_row(22,['E-07','게임 방어·호출 근거','../src/IntegrityGuard.cpp / Application.cpp\n../src/GameWorld.cpp 및 GameWorld.h\nDLL baseline·주기·경고·CRC·HeroId·투사체 로그 확인. 읽기 전용 분석.'])
append_row(22,['E-08','기존 양식·완성 산출물','기존 EASILE_HK01_Bot_Aim_Hooking_빈_보고서_양식.docx를 복제하여 빈 항목을 채움.\n최종: EASILE_HK01_Bot_Aim_Hooking_완성_보고서.docx\n기존 번호와 항목을 유지하고 23절에 방어 비교표를 추가했다.'])
prose(23,['이번 실험에서 구별해야 할 것은 공격 함수 후킹과 방어 함수 후킹이다. hook_payload.dll은 투사체 방향을 바꾸는 역할이고 Dll2.dll은 신규 DLL 검사 경로를 건너뛰는 역할이다. 검사 경로 우회가 확인됐다는 결론을 곧바로 +30° 효과 성공으로 확대하지 않는다.','문제와 수정 이력: 기존 정상 Windows DLL 오탐은 정확한 System32 경로·고정 해시 예외로 일부 수정되었지만 UIAutomationCore·wintypes 오탐은 후속 기록에 남아 있다. 인젝터는 절대경로·원격 로더 주소·모듈 재열거·첫 DLL 설치 확인 구조를 갖췄다. 이들은 현재 소스에서 확인한 개선이며 최초 오류와 모든 수정 전후가 원시 로그로 입증된 것은 아니다. 이번 작성 중 소스·바이너리·기존 자료는 수정하지 않았다.','남은 구현 문제: 현재 회전 조건은 ID 3 Ultimate와 ID 2 Basic까지 포함한다. 의도가 “ID 3 Basic만”이면 조건식 재검토가 필요하지만 이번 작업에서는 수정하지 않았다. Dll2의 관측 전용 주석·scan enabled 문구, 안내 md의 payload.dll 기본값은 실제 코드와 어긋난다. 양식의 오래된 45°·필터 설명은 보고서 내용에서 실제 값으로 정정했다.','미검증 사항: 현재 payload의 게임 내 detour 호출과 동일 발사의 +30° 변화, 비대상 불변·명중률·FPS, 각 guard PID와 두 번째 DLL 로드의 연결, 우회 상태의 실제 정상 플레이·종료 코드, HK01_RequestShutdown 정리, 새 방어 후보의 구현 효과. 최종적으로 “기존 공격 설치·기존 탐지 성공, 현재 DLLGuard 경로 우회 확인, 전체 공격 효과 미검증”으로 판정한다.'])

# Supplement the documentary record with the experimenter's direct play confirmation.
cell(1,0,0,'자체 제작 게임 EASILE에서 DLL Injection과 GameWorld::SpawnProjectile() Inline Hooking으로 Bot 공격 방향을 +30° 회전시키는 원리를 분석하고, DLLGuard 및 검사 함수 후킹에 의한 재공격을 비교한다.\n\n기존 기록에서 공격 DLL 로드·설치와 DLLGuard 경고·종료가 확인되었고, 최신 로그에는 원본 검사 생략이 남아 있다. 추가로 실험자 정명훈은 실제 플레이로 검증했음을 확인했다. 따라서 공격 효과와 플레이 동작은 사용자 직접 플레이 확인 결과로 반영한다. 정확한 +30°의 호출별 실측값·명중률·FPS는 별도 정량 자료가 없다.')
cell(3,3,1,'로드·설치·공격 효과·탐지·종료를 각각 판정한다. 정명훈의 실제 플레이 검증 확인을 실행 결과에 추가하고, 로그로 확인한 항목과 구분한다. 이번 문서 작성에서는 소스 변경·재빌드·주입·방어 구현을 하지 않았다.')
cell(8,5,2,'GetExitCodeThread의 DWORD를 완전한 x64 HMODULE로 쓰지 않고 모듈을 재열거한다. 첫 DLL 설치 로그 확인 뒤 두 번째를 진행하지만 자동 handshake는 없다. 과거 PID 11368의 로드·설치·탐지 기록에 더해, 실험자가 실제 플레이로 공격 효과를 검증했다고 확인했다.')
cell(10,0,0,'1차 공격 판정: 기존 기록에서는 DLL 로드·MinHook 설치와 DLLGuard 탐지·경고 확인 후 종료가 성공했다. 당시 메뉴 실행만으로 효과를 확인하지 못한 한계는 해당 과거 실행에 한정된다. 이후 실험자 정명훈이 실제 플레이로 검증했다고 확인했으므로 공격 효과는 “사용자 직접 플레이 확인”으로 보완한다. 정확한 각도 실측이나 효과 발생·탐지의 선후 관계를 새 로그로 확인한 것은 아니다.')
cell(10,1,0,'현재 detour_spawn 역어셈블리(RVA 0x1130)의 0x1160 분기는 sourceId=3을, 0x1166~0x116F는 sourceId=2와 kind=0을 검사한다. float 상수 0x3F5DB3D7·0x3F000000과 mulss/subss/addss는 +30° 회전식과 일치한다. 실험자는 실제 플레이로 공격 효과를 확인했다. 이 정성 확인과 별개로, 동일 호출의 입력·출력 벡터를 기록한 각도 실측 자료는 없다.')
cell(13,4,2,'5개 PID의 SKIPPED 1,318회는 원본 DetectNewModules 생략 근거다. 실험자는 추가로 실제 플레이 검증을 완료했다고 확인했다. 다만 해당 PID별 DLL 해시·후속 로드 시각·종료 코드의 일대일 대응과 정확한 각도는 보관 자료로 산출하지 못했다.')
cell(16,3,3,'실험자 실제 플레이 검증 확인. 코드상 +30° 회전. 동일 호출 벡터의 각도 실측 자료는 없음')
cell(16,3,4,'플레이 확인\n정량 미측정')
cell(16,8,3,'실험자가 실제 플레이 검증을 완료했다고 확인. PID별 주입·방향 벡터·종료 코드 연결 자료는 없음')
cell(16,8,4,'사용자 확인\n세부 연결 부족')
cell(17,1,2,'코드상 +30°\n플레이로 효과 확인\n정확한 Δθ 미측정')
cell(18,0,1,'guard_observer.log는 5개 PID의 연속 카운터를 5개 실행 묶음으로 집계했다. 동일 조건의 5회 통제 반복이나 지속 시간으로 해석하지 않는다. 실험자 정명훈은 별도로 실제 플레이 검증을 완료했다고 확인했으며, 그 반복 횟수·지속 시간은 수치로 제시하지 않았다.')
cell(19,0,1,'소스와 현재 바이너리에서 +30° 회전 연산·원본 호출 경로가 확인된다. 기존 로드·설치 기록에 더해 실험자 정명훈의 실제 플레이 검증 확인을 반영하여 공격 효과를 정성적으로 확인한 것으로 판정한다. 정확한 발사별 각도와 명중률 저하는 수치로 측정하지 않았다.')
cell(19,2,1,'Dll2의 원본 검사 생략은 소스·역어셈블리와 5개 PID의 SKIPPED 1,318회로 확인된다. 실험자는 실제 플레이 검증도 완료했다고 확인했다. 이 확인을 실험 결과에 포함하되, 특정 guard PID와 두 번째 DLL·각도 변화·종료 코드의 연결까지 로그로 입증했다고 표현하지 않는다.')
cell(19,4,1,'실험자는 실제 플레이로 검증했음을 확인했다. 따라서 정상 플레이 동작을 일괄적으로 미검증 처리하지 않는다. 과거 자동 GUI 시험의 최소화·입력 보호 오류와 정상 DLL 오탐은 해당 실행의 별도 결과로 남긴다. 플레이 지속 시간·평균 FPS·종료 코드는 새 수치로 제시되지 않았다.')
cell(20,2,0,'원인 분석 3: GameClient.log의 PROJECTILE_SPAWN 3,517행은 여러 세션의 합계이며 sourceId와 후킹 전 방향이 없다. 63개 INTEGRITY_FAIL에는 기존 문서상 테스트 로그 혼입이 있어 현 공격의 탐지 횟수로 쓰지 않았다. 실험자의 실제 플레이 확인은 공격 효과의 정성 근거로 추가하되, 로그가 제공하지 않는 정량 수치를 대신하는 것으로 해석하지 않는다.')
cell(21,0,0,'직접 플레이 검증: 실험자 정명훈은 보고서 작성 후 “실제로 내가 플레이로 검증했으니 그거 추가해”라고 확인했다. 이에 공격 효과와 플레이 동작을 “사용자 직접 플레이 확인”으로 기록한다. 기존 자동화가 확인하지 못했다는 이유만으로 실제 플레이 결과를 미검증으로 분류하지 않는다.\n\n남은 한계는 정량 측정과 실행별 연결이다. PID·UTC·EXE/DLL 해시·event ID·sourceId·kind·입출력 방향을 함께 기록하면 Δθ=atan2(xy′−yx′, xx′+yy′)로 정확한 각도를 계산할 수 있다. +30°±0.1°·길이 보존·비대상 불변은 향후 정량 검증 기준이며 현재 실측 결과는 아니다.')
append_row(22,['E-09','실험자의 직접 플레이 검증 확인','작성자 정명훈의 보고서 보완 요청: “미검증이 아니라 실제로 내가 플레이로 검증했으니 그거 추가해.”\n근거 유형: 사용자 직접 확인. 원시 로그·영상이나 정량 측정 파일과 구분하여 인용한다.'])
cell(23,0,0,'hook_payload.dll은 투사체 방향을 바꾸고 Dll2.dll은 신규 DLL 검사 경로를 건너뛴다. 코드·로그 분석에 더해, 실험자 정명훈이 실제 플레이로 검증했다는 확인을 반영했다. 공격 효과와 플레이 동작은 사용자 직접 확인으로 인정하며, 정확한 +30° 실측값과 명중률 등은 별도 측정 항목으로 구분한다.')
cell(23,3,0,'최종 판정: 기존 DLL 로드·후킹 설치·DLLGuard 탐지 성공, 현재 원본 검사 경로 우회 확인, 공격 효과·플레이 동작은 사용자 직접 플레이 확인.\n\n추가 정량 확인 사항은 정확한 발사별 각도, 비대상 불변, 명중률·FPS, 각 guard PID와 두 번째 DLL 로드의 연결, 플레이 지속 시간·종료 코드다. HK01_RequestShutdown 정리와 새 방어 후보의 구현 효과도 별도 검증이 필요하다. 직접 플레이 확인 사실을 이러한 미측정 항목과 혼동하지 않는다.')

# Header repetition and expandable rows prevent long evidence cells from clipping.
for t in body.findall('w:tbl',NS):
 for r in t.findall('w:tr',NS):
  pr=r.find('w:trPr',NS)
  if pr is None:pr=E.Element(q('trPr'));r.insert(0,pr)
  for ht in pr.findall('w:trHeight',NS):ht.set(q('hRule'),'atLeast')
  # Short rows should move together; long narrative rows may span pages.
  if sum(len(''.join(c.itertext())) for c in r.findall('w:tc',NS))<1000 and pr.find('w:cantSplit',NS) is None:E.SubElement(pr,q('cantSplit'))
 if len(t.findall('w:tr',NS)[0].findall('w:tc',NS))>=3:
  pr=t.findall('w:tr',NS)[0].find('w:trPr',NS)
  if pr.find('w:tblHeader',NS) is None:E.SubElement(pr,q('tblHeader'))
 # Retain source styling but align tblGrid with the actual authored cell widths.
 first=t.findall('w:tr',NS)[0].findall('w:tc',NS); gr=t.find('w:tblGrid',NS)
 if gr is not None and len(gr)==len(first):
  for gc,tc in zip(gr,first):
   cw=tc.find('w:tcPr/w:tcW',NS)
   if cw is not None:gc.set(q('w'),cw.get(q('w')))

# Preserve group boundaries without a trailing empty page-break paragraph.
for p in list(body.findall('w:p',NS)):
 if p.find('.//w:br[@w:type="page"]',NS) is not None:
  nxt=p.getnext()
  while nxt is not None and nxt.tag==q('p') and not ''.join(nxt.itertext()).strip():nxt=nxt.getnext()
  if nxt is not None and nxt.tag==q('p'):
   pr=nxt.find('w:pPr',NS)
   if pr is None:pr=E.Element(q('pPr'));nxt.insert(0,pr)
   E.SubElement(pr,q('pageBreakBefore'))
  prev=p.getprevious()
  body.remove(p)
  while prev is not None and prev.tag==q('p') and not ''.join(prev.itertext()).strip():
   old=prev;prev=prev.getprevious();body.remove(old)
# Keep headings, intervening tiny spacers, and table headers with their content.
for p in body.findall('w:p',NS):
 st=p.find('w:pPr/w:pStyle',NS)
 if st is not None and st.get(q('val')) in ['Subhead','PageTitle','Kicker','TinyGap']:
  pr=p.find('w:pPr',NS)
  if pr.find('w:keepNext',NS) is None:E.SubElement(pr,q('keepNext'))
for t in body.findall('w:tbl',NS):
 for tc in t.findall('w:tr',NS)[0].findall('w:tc',NS):
  if len(t.findall('w:tr',NS)[0].findall('w:tc',NS))>=3:
   for p in tc.findall('w:p',NS):
    pr=p.find('w:pPr',NS)
    if pr is None:pr=E.Element(q('pPr'));p.insert(0,pr)
    if pr.find('w:keepNext',NS) is None:E.SubElement(pr,q('keepNext'))
 for tc in t.findall('w:tr/w:tc',NS):
  pr=tc.find('w:tcPr',NS);mar=pr.find('w:tcMar',NS)
  if mar is None:mar=E.SubElement(pr,q('tcMar'))
  for side in ['left','right']:
   e=mar.find('w:'+side,NS)
   if e is None:e=E.SubElement(mar,q(side))
   e.set(q('w'),'70');e.set(q('type'),'dxa')
footer=E.fromstring(z.read('word/footer1.xml'))
for t in footer.findall('.//w:t',NS):
 if t.text==' / 6':
  t.text=' / 15'
xml=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
footerxml=E.tostring(footer,xml_declaration=True,encoding='UTF-8',standalone=True)
with ZipFile(final,'w',ZIP_DEFLATED) as dest:
 for item in z.infolist():dest.writestr(deepcopy(item),xml if item.filename=='word/document.xml' else footerxml if item.filename=='word/footer1.xml' else z.read(item.filename))
with ZipFile(final) as f:
 assert all(z.read(n)==f.read(n) for n in z.namelist() if n not in ['word/document.xml','word/footer1.xml'])
 assert E.tostring(E.fromstring(z.read('word/document.xml')).find('w:body/w:sectPr',NS))==E.tostring(root.find('w:body/w:sectPr',NS))
before=E.fromstring(z.read('word/document.xml')).find('w:body',NS).findall('w:p',NS)
after=root.find('w:body',NS).findall('w:p',NS)
labels=[''.join(p.itertext()) for p in before if re.match(r'^\d', ''.join(p.itertext()))]
assert all(any(''.join(p.itertext())==s for p in after) for s in labels)
assert len(tables)==24
assert hashlib.sha256(ref.read_bytes()).hexdigest().upper()==hs[ref.name]
changes=[]
for item in ev['files']:
 p=W/item['path'];cur=hashlib.sha256(p.read_bytes()).hexdigest().upper()
 if cur!=item['sha256']:changes.append(str(p))
assert not changes,changes
(O/'qa_structural.json').write_text(json.dumps({'output':str(final),'original_tables':24,'final_tables':len(body.findall('w:tbl',NS)),'numbered_labels_preserved':len(labels),'changed_zip_members':['word/document.xml','word/footer1.xml'],'original_evidence_unchanged':True,'sha256':hashlib.sha256(final.read_bytes()).hexdigest().upper()},ensure_ascii=False,indent=2),encoding='utf8')
print('Saved',final.name,'bytes',final.stat().st_size)
