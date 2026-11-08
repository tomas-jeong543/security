# EASILE HK-01 공격·방어 검증 결과

작성: 2026-11-07, Asia/Seoul. **현재 판정: 방어 활성 상태의 실제 +30° 효과 및 지속 플레이는 미검증.** 기존 공격 DLL을 숨기거나 방어를 끄는 변경은 하지 않았다. 새 무주입 기준선에서 Windows DLL 오탐이 발생했고 GUI 복원도 실패하여, 이번 게임 실행에서는 DLL 주입을 진행하지 않았다. 별도 오프라인 테스트에서는 기존 회전·필터·원본 함수 전달 로직이 통과했다.

## 1. 목표

사용자 소유의 로컬 Windows C++17 / DirectX 11 게임에서 DLLGuard·CRC·SpeedGuard를 유지하고, DLL 로드 → 후킹 설치 → detour 호출 → ID 3 Bot 기본 공격의 +30° 변화 → 게임 진행 → 탐지·경고·종료를 각각 판정한다. 설치 메시지나 프로세스 생존만으로 공격 효과 또는 정상 플레이를 인정하지 않는다.

기준 문서는 [HK01_CONTEXT_HANDOFF.md](HK01_CONTEXT_HANDOFF.md), 기존 검증은 [HK01_validation_result.md](../HK01_validation_result.md)다. 이번 결과는 이전 실행과 분리했다. 기존 이동속도 220→440 패치 실험은 이번 방향 후킹의 실행 증거가 아니다.

## 2. 환경·빌드 식별

경로 표기:

```text
P = D:\GitHub\security\projects\Game_project_easile(리버싱)
W = P\api_hooking_project
R = W\.hk01_validation\resume_20261107
S = W\EASILE_HK01_starter_fixed\easile_hk01
```

이번 대상은 **W\GameClient.exe**이며 P\build\Release\GameClient.exe와 SHA-256이 같다. 게임은 재빌드하지 않았다. 관측용 DLL과 오프라인 테스트만 별도 출력 경로에서 VS2022 v143 Release/x64로 빌드했다. 복사한 vcxproj는 /MT와 기존 MinHook 정적 라이브러리를 사용한다.

| 파일 | 이번 확인한 SHA-256 |
|---|---|
| W\GameClient.exe 및 P\build\Release\GameClient.exe | `32A1D0D4D8FBF15E2A1E45467475A6057F26EEE54B1AF5746945DE24C768A926` |
| W\injector.exe — 기존 인젝터, 이번에는 실행 안 함 | `EA2F820B2D62F445C9DCDA2B3E39F023ABC563D32D9F6D9C28C2B02A080931BD` |
| W\hook_payload.dll — 원본 보존 | `296769092E14E4EC1003A8B955D26E03D8820AEDA30BA07EA303648D73697591` |
| R\out\x64\Release\hook_payload.dll — 관측 사본, 게임에 미로드 | `5EB30670ACEFCD10FD686821165670F6E06AC07B47A5AE0D307186807032ADB4` |
| R\out\x64\Release\observe_test.exe — 오프라인 테스트 | `F302D0B4E2BDB17E7C4683238498FF2785E5E677D29D49329F14A5F5306BAD0B` |
| P\build\Release\GameClient.pdb | `E6DCC6BD083756ECD07586C3CBB58DD4CC65728435FC5D3F3049988249EB09A8` |

**새 정적 대조 결과**: PE Machine=0x8664. EXE CodeView와 PDB 정보 스트림의 GUID 원시 바이트가 모두 `6442e8f5f48adb40ae700f615dc0fe2e`, Age=7로 일치한다. GUID 문자열은 표준 표시 순서가 아닌 원시 바이트 순서다. 이번에는 심볼 전체를 다시 해석하지 않고 기존 심볼 분석 결과와 현재 식별자·함수 진입 바이트를 대조했다.

| 항목 | 해당 빌드의 RVA·길이 | 이번 파일 대조 |
|---|---|---|
| GameWorld::Update | 0x14820, 0x1444 | CRC32 `29148A8C`, 현재 기대값과 일치 |
| GameWorld::Initialize | 0x110D0, 0x685 | CRC32 `A8E8248A`, 현재 기대값과 일치 |
| GameWorld::SpawnProjectile | 0x137E0 | 진입 14바이트 `48 8B C4 48 89 58 18 55 56 57 41 54 41 55`, 원본 payload 검사와 일치 |

이는 **디스크 파일 정적 검사**이며 이번 게임 실행의 동적 CRC 호출 성공을 뜻하지 않는다. ABI는 GameWorld.h의 멤버 함수 인자와 payload의 `world, team, Vec2 position, Vec2 direction, kind, damage, uint64_t sourceId`를 대조했다. Vec2 크기는 payload의 컴파일 타임 검사로 8바이트다. 오프라인 x64 함수 전달 테스트는 통과했지만 실제 게임 detour의 ABI 정확성은 이번 실행에서 입증하지 못했다. 다른 EXE·새 빌드에는 이 RVA·CRC·ABI 판정을 재사용하지 않는다.

근거: [build-verification.json](../.hk01_validation/resume_20261107/build-verification.json), [실행 manifest](../.hk01_validation/resume_20261107/run_20261107_011556/manifest.json).

## 3. 기존 문제와 현재 방어·호출 관계

### 3.1 탐지되는 정확한 이유 — 소스로 확인

`P/src/Application.cpp` 124~134행은 첫 렌더 이후 DLL baseline을 만든다. 같은 파일 60~65행은 메인 루프에서 약 1초 간격으로 `DetectNewModules()`를 호출한다.

`P/src/IntegrityGuard.cpp`:

- `CaptureLoadedDlls()` 167행: Toolhelp로 현재 프로세스 DLL 전체 경로 수집.
- `InitializeDllBaseline()` 209행: 최초 모듈 집합 저장.
- `DetectNewModules()` 235행: baseline에 없고 아직 보고하지 않은 경로 확인.
- `IsVerifiedSystemDll()` 129행 및 257행: 정확한 System32 경로와 고정 SHA-256이 일치하는 5개 DLL만 예외 처리.
- 265~277행: 신규 DLL 디버그 출력 → `MessageBoxW` → 반환 후 `ExitProcess(1)`.

기존 HK-01은 `LoadLibraryW`로 DLL을 정상 로더 목록에 올리므로, baseline 이후 로드된 payload가 열거되고 예외 조건을 만족하지 않아 탐지된다. 이는 MinHook 동작이나 +30° 변화 자체를 식별한 것이 아니라 **신규 모듈의 경로**를 탐지한 것이다. 신규 모듈 탐지는 로딩 후 검사이므로 DLL 로드·후킹 설치가 먼저 완료될 수 있다. 이번 실행에서는 주입이 없었으며, 이 설명의 기존 공격 적용 사례는 이전 증거에 근거한다.

### 3.2 CRC와 SpeedGuard의 구분

`IntegrityGuard.cpp` 83행의 `VerifyUpdateIntegrity()`는 `GameWorld.cpp` 920행의 플레이어 궁극기 명령 처리에서 호출된다. 실패하면 `INTEGRITY_FAIL`을 기록하고 해당 Update에서 반환한다. 그 분기 자체에는 경고창·프로세스 종료가 없다. `VerifyHeroIntegrity()`는 정의돼 있으나 현재 src에서 호출 지점이 확인되지 않는다. SpawnProjectile 진입점은 두 CRC 범위 밖이다.

`GameWorld.cpp` 860~890행의 SpeedGuard는 플레이어 이동 거리를 고정 속도 한도와 비교하고 초과 시 로그·경고·종료한다. 방향 회전의 직접 검사는 아니다. 이번 세션에서는 CRC 호출 경로와 SpeedGuard를 변경하거나 동적 시험하지 않았다.

### 3.3 기존 관측과 새 기준선의 차이

이전 기록에는 무주입 15초 무경고와 정상 스모크 CRC·종료 코드 0, 다른 실행의 DLL 로드·Hook installed·DLLGuard 탐지·경고 확인 후 종료가 있다. 이는 인수인계에서 재사용한 **과거 결과**다. 이번 무주입 실행에서 새 Windows DLL이 탐지됐으므로, 과거 15초 결과를 현재의 일반적인 오탐 해소나 장시간 플레이 보장으로 확대할 수 없다.

## 4. 시도한 방법과 선택 이유

| 후보 | 장점 | 단점·위험·수정 대상 | 이번 처리 |
|---|---|---|---|
| 기존 인젝터·DLL 그대로 재실행 | 구현 차이 최소 | 호출별 벡터 증거 없음. 설치만 확인될 수 있음 | 원본 보존, 실행 안 함 |
| DLL 사본에 호출 전후 관측 추가 | 조건·회전·원본 호출을 유지하며 이벤트 식별 가능 | OutputDebugString·문자열 출력이 실행 시간에 영향. S/hook_payload.c의 사본만 변경 | 선택, 별도 DLL 빌드 완료 |
| 게임 SpawnProjectile에 sourceId·이벤트 ID·실제 생성 방향 로그 추가 | 최종 생성 투사체와 detour를 명확히 연결 가능 | 게임 재빌드 및 RVA·CRC 재측정 필요. P/src/GameWorld.cpp와 빌드 사본 필요 | 원본 보존을 위해 이번 미실시 |
| 게임 없이 detour를 모의 원본 함수에 연결 | GUI·주입 없이 필터·회전·인자 전달 검증 가능 | 실제 게임 효과·MinHook 설치·방어 대응은 증명 못 함 | 선택, 테스트 통과 |
| 로딩 방식을 바꿔 모듈 탐지를 피하거나 DLLGuard 예외에 payload 추가 | 현재 탐지의 전제를 바꿈 | 방어 활성 상태에서 기존 경로를 평가하는 실험과 달라지고 비교를 오염시킴 | 구현·실행 안 함 |

이번 작업의 실제 제한은 Windows SDK 접근 권한과 GUI 복원 실패였다. 권한 제한은 정식 실행 권한 요청으로 처리했다. 은닉·탐지 회피 구현이나 방어 비활성화로 결과를 만들지 않았다. 기존 코드의 비회피형 로컬 검증 및 방어 범위 분석은 수행했다.

## 5. 실제 변경·생성 파일

모든 새 파일은 W 아래 R 또는 docs에만 작성했다. **P/src의 방어·게임 코드, W 루트 EXE·DLL·인젝터, S의 원본 소스는 수정하지 않았다.**

| 파일 | 전후 차이·목적 |
|---|---|
| R/hook_payload.c | S 원본 복사 후 stdio.h·호출 카운터 추가. detour 진입/원본 반환, GetTickCount64, 이벤트 번호, sourceId/kind/team/damage, 입력·출력 벡터 출력. 필터·회전식·후킹 대상·설치/종료 로직 유지 |
| R/hook_payload.vcxproj | 원본 프로젝트 복사. 위치가 달라져 출력·중간 파일이 R 아래에 생성됨 |
| R/Collect-Run.ps1 | 독립 CWD·manifest·UTC JSONL·PID별 DBWIN 출력 수집. 별도 요청 파일이 있을 때만 기존 injector 실행. GUI 성공과 무관하게 자동 주입하지 않음 |
| R/observe_test.c, R/observe_test.vcxproj | 모의 원본 함수로 detour를 직접 호출하는 콘솔 테스트. DllMain/worker/MinHook 설치를 실행하지 않음 |
| R/out, R/obj | 위 두 프로젝트의 격리된 빌드 산출물 |
| R/*.json, *.log, *.diff, git-status.txt 및 run_*/ | 빌드 식별·원본 보존·테스트·실행 증거 |
| docs/HK01_ATTACK_DEFENSE_RESULT.md | 본 보고서 |

관측 사본의 `detour_spawn()`은 46행, 입력·출력 출력은 63행, 원본 반환 출력은 70행이다. [observation-only.diff](../.hk01_validation/resume_20261107/observation-only.diff)에서 최소 변경을 확인할 수 있다. 동시 호출 시 전역 이벤트 번호는 고유성을 제공하지만 출력 순서 전체를 직렬화하지는 않는다.

git status에서 기존 IntegrityGuard 수정과 빌드 tlog 변경, 다른 프로젝트의 삭제·미추적 파일도 확인했다. 이들은 이번 수정으로 만들거나 되돌리지 않았다. W 전체가 상위 저장소에서 미추적 상태이므로 git diff만으로 새 파일을 전부 검증할 수 없다. 별도의 [source-preservation.json](../.hk01_validation/resume_20261107/source-preservation.json)에서 검사한 원본 5개 소스의 전후 해시가 모두 같음을 확인했다. 기존 방어 diff는 [preexisting-defense.diff](../.hk01_validation/resume_20261107/preexisting-defense.diff)에 보존했다.

## 6. 단계별 실제 실행·증거

### 6.1 명령과 결과

작업 디렉터리 W에서 다음을 수행했다.

```powershell
# 읽기 전용 현황 및 기존 수정 확인
git status --short
git diff -- ../src/IntegrityGuard.cpp

# 아래 MSBuild 전체 경로를 사용
# C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe
& $msbuild '.hk01_validation/resume_20261107/hook_payload.vcxproj' /p:Configuration=Release /p:Platform=x64 /m /v:minimal
& '.\.hk01_validation\resume_20261107\Collect-Run.ps1' -Seconds 300
& $msbuild '.hk01_validation/resume_20261107/observe_test.vcxproj' /p:Configuration=Release /p:Platform=x64 /m /v:minimal
& '.\.hk01_validation\resume_20261107\out\x64\Release\observe_test.exe'
```

`$msbuild`는 문서상 경로 축약이며 실제 실행은 위 주석의 전체 경로를 직접 지정했다. 첫 DLL 빌드는 SDK 경로 `AppData\Local\Microsoft SDKs` 접근 거부 MSB4184로 실패했다. 정식 권한 요청 후 DLL 빌드 종료 코드 0, 오프라인 테스트 빌드도 코드 0. 게임 실행·수집기는 권한 요청 후 시작했다. 별도의 게임 재빌드나 전체 GameTests는 하지 않았다.

### 6.2 새 무주입 게임 실행

실행 증거 폴더: [run_20261107_011556](../.hk01_validation/resume_20261107/run_20261107_011556/). 대상 PID **23212**, CWD는 이 폴더, EXE는 W\GameClient.exe다. JSONL은 UTC이며 폴더명은 한국 시각이다.

| 시점·단계 | 실제 관측 | 판정 |
|---|---|---|
| 시작 2026-11-07 01:15:56경 KST | WINDOW_CREATED, DX11_READY, HUD_READY, smoke=0 | 초기화 로그 확인 |
| 16:15:57.231Z / 01:15:57 KST | Baseline initialized (52 DLLs), 첫 렌더 후 baseline | DLLGuard 활성 확인 |
| GUI 관찰 | 접근성 트리에 State: Menu. 시작을 위해 Return 입력 시도 | 메뉴 확인, 시작 성공 미확인 |
| 16:16:17.212~221Z | CoreUIComponents·CoreMessaging의 pinned 시스템 DLL 허용 로그 | 기존 제한적 예외 동작 확인 |
| 16:16:17.233Z | New DLL detected: c:\windows\system32\wintypes.dll | **무주입 신규 DLL 탐지 확인** |
| GUI 입력·복원 | `window is minimized; call activate_window...`; 새 창 객체로 복원 재시도 후 `user input was detected in this window...` | 게임 시작·경고 GUI·정상 플레이 확인 불가 |
| 16:20:57.090Z / 01:20:57 KST | 관측 300초 종료 시 프로세스 생존 | 플레이 지속 또는 방어 무력화 증거 아님 |
| 종료 정리 | 이 실험에서 시작한 PID와 전체 경로를 대조한 뒤 외부 Stop-Process로 정리 | 방어 자체 종료로 분류하지 않음 |

주입 요청 파일을 만들지 않았고 injector 실행·payload 모듈 로드는 없었다. 정리 전 모듈 목록에서도 hook_payload.dll은 없었다. GameClient.log에는 초기화 3행만 있으며 전투·MATCH·PROJECTILE_SPAWN이 없다.

프로세스가 300초 뒤에도 살아 있던 이유는 코드상 MessageBox가 반환돼야 ExitProcess에 도달하는 구조와 부합한다. 그러나 이번에는 경고창을 직접 관찰·확인하지 못했으므로 **실제 대기 위치는 추정**이다. 자동 관측 중 GUI 입력 시도와 신규 DLL 로드가 시간적으로 가까웠지만, GUI 도구가 wintypes.dll 로드의 원인이라고 확정하지 않는다.

정리 단계에서도 일반 권한 Stop-Process가 Access denied로 실패했다. 대기 명령을 취소하고 정식 권한 요청 후 해당 PID만 종료했다. 수집기의 종료 코드 0은 수집기 완료를 뜻하며 게임 정상 종료 코드가 아니다. 16:22:45.521Z의 외부 정리 종료 코드는 **-1**이며, 방어의 ExitProcess(1) 관측값이 아니다. 실제 정리 결과는 events.jsonl의 `EXTERNAL_TEST_CLEANUP`에 기록했으며 `defenseExitEvidence=false`다.

### 6.3 새 Windows DLL 검증

`C:\Windows\System32\wintypes.dll`:

- Authenticode 상태: **Valid**.
- 서명자: `CN=Microsoft Windows, O=Microsoft Corporation, L=Redmond, S=Washington, C=US`.
- SHA-256: `D606EF89E2A7A6DC90FA333C8DDAA4B650D165704F5B5202E9DF9B68273A9A6B`.

이는 새 기준선 실행에서 정상 서명 시스템 DLL을 신규 모듈로 탐지한 **오탐 근거**다. 현재 5개 예외에 wintypes.dll이 없다는 소스와 일치한다. 이번에는 방어 코드 보존 조건에 따라 예외를 추가하지 않았다. 유효한 디스크 서명만으로 실행 중 메모리 전체의 무결성을 보장하는 것은 아니다.

근거: [wintypes-verification.json](../.hk01_validation/resume_20261107/run_20261107_011556/wintypes-verification.json), [events.jsonl](../.hk01_validation/resume_20261107/run_20261107_011556/events.jsonl), 같은 폴더의 GameClient.log.

### 6.4 비회피형 오프라인 검증

실제 관측 사본의 `detour_spawn()`을 그대로 포함하고 원본 함수 포인터만 모의 기록 함수로 연결했다. 대상 4개 벡터 `(1,0), (0,1), (-1,0), (0.6,-0.8)`와 비대상 2개(ID 1 Basic, ID 3 Ultimate)를 검사했다.

| 검사 | 결과 |
|---|---|
| 대상 4개 회전 | `29.99999874°`~`30.00000045°` |
| 벡터 길이 오차 | 최대 약 `1.35e-8` |
| 비대상 2개 | 입력 방향 그대로 전달 |
| 원본 함수 전달 | 총 6회, 대상 sourceId·kind 전달 확인 |
| 활성 detour 카운터·설치 플래그 | 호출 후 0, hook 생성·활성 플래그 모두 false |
| 프로세스 종료 코드 | **0** |

근거: [offline-test.log](../.hk01_validation/resume_20261107/offline-test.log), [observe_test.c](../.hk01_validation/resume_20261107/observe_test.c). 이는 로컬 계산·필터·콜백 전달 검증이다. **게임 DLL 로드, MinHook 설치, 실제 투사체 생성 또는 플레이 지속의 성공으로 환산하지 않는다.**

## 7. 공격·방어 판정

| 이벤트 | 과거 기록 | 이번 새 실험 |
|---|---|---|
| 게임 초기화·baseline | 확인됨 | 확인됨 |
| 런타임 Update CRC 통과 | 과거 스모크에서 확인 | 호출 미검증, 디스크 CRC만 일치 |
| DLL 로드 | 과거 PID 11368에서 확인 | 미실시 |
| MinHook 설치 | 과거 Hook installed 확인 | 미실시 |
| 게임 내 detour 호출 | 미검증 | 미검증 |
| 실제 생성 투사체 +30° | 미검증 | 미검증; 오프라인 회전만 통과 |
| 후킹된 정상 플레이 지속 | 미검증 | 미검증 |
| 주입 DLL 탐지 | 과거 확인 | 주입 없음; wintypes.dll 오탐 확인 |
| 경고 GUI·확인 후 종료 | 과거 확인, 당시 종료 숫자 미수집 | GUI 미검증, 방어 자체 종료 미검증 |
| DLL 주입 자체 차단 | 입증 안 됨 | 평가 안 함 |

최종 연구 질문인 **“방어 활성 상태에서 효과가 발생하고 일정 시간 지속되는가?”에는 아직 답할 실행 증거가 없다.** 오프라인 테스트 통과와 300초 프로세스 생존은 그 답이 아니다.

## 8. 실패 원인과 다음 실행 계획

확인된 장애는 ① 게임 시작 전에 발생한 정상 서명 시스템 DLL 신규 탐지, ② 최소화·포커스 복원 실패다. 두 장애를 후킹 실패나 공격 차단으로 분류하지 않는다. 현재 소스상 탐지 이후 모달 경고가 게임 메인 루프를 막고, 확인 뒤에 종료하는 구조이므로 정상 플레이 지속과 단순 프로세스 생존을 구분해야 한다.

다음 검증은 다음 순서로 진행한다.

1. **무주입 기준선부터 해결**: 정확한 wintypes 경로·서명·해시 근거를 검토해 필요한 경우 별도 방어 변경으로 제한적 예외를 추가한다. 원본을 보관하고 새 게임 빌드의 EXE/PDB·RVA·CRC를 다시 대조한다. 이번에는 변경하지 않았다.
2. **GUI 기준선 확보**: 복원 후 수동 또는 정상 UI 입력으로 시작·이동·공격을 확인하고 게임 로그의 MATCH·발사 기록을 확보한다. 복원 불가하면 그 항목을 미검증으로 종료한다.
3. **기존 로딩 경로의 관측 실험**: DLLGuard 활성·정상 전투 상태를 확인한 뒤 기존 인젝터로 관측 사본을 한 번 로드한다. 검사 주기에 맞추거나 기준선에 payload를 포함시키는 조작은 하지 않는다. 현재 하네스의 `inject.request`는 이 준비 확인 뒤에만 생성한다.
4. **같은 호출의 증거 연결**: detour 이벤트의 ID 3·Basic 입력/출력과 원본 반환을 수집한다. `atan2(cross,dot)`로 +30° 및 길이 보존을 판정한다. 게임의 최종 생성 방향까지 입증하려면 별도 게임 사본의 SpawnProjectile 로그에 sourceId·공유 가능한 이벤트 식별·실제 방향을 추가해야 한다. 현재 게임 로그는 sourceId가 없어 엄밀한 동일 발사 연결에 한계가 있다.
5. **지속 시간·대응 분리**: 최초 효과부터 마지막 실제 진행 이벤트까지의 시간을 측정하고, 탐지 출력·경고 관찰·확인·프로세스 종료를 별도로 수집한다. 30초를 사전 지속 관측 목표로 두되 탐지·종료 전에 관측 시간이 짧으면 그 실제 시간만 기록한다. 경고 대기 중 생존을 지속 플레이로 계산하지 않는다.

하네스 UTC는 **디버그 메시지 수신 시각**이다. payload 내부 tick은 별도 단조 시각이다. 모든 이벤트의 원천 발생 시각·순서를 수신 시각만으로 단정하지 않는다. 전역 DBWIN 수집은 다른 수집기와 경합할 수 있고, 관측 출력 자체의 지연도 있으므로 다음 실행 전 단일 수집기 사용을 확인한다.

## 9. 다음 방어 개선안 — 이번 미적용

- **오탐 관리**: 시스템 DLL 허용은 검증한 전체 경로·해시를 명시하고 OS 업데이트 시 재검증한다. 모든 System32 DLL 또는 모든 서명 DLL을 일괄 허용하지 않는다.
- **응답 구조**: 경고 확인 여부와 보안 대응을 분리한다. 탐지 즉시 게임 진행을 명시적으로 중단하고 구조화 로그를 남기는 정책을 검토한다. 모달창 확인까지 프로세스가 살아 있는 상태를 문서화한다.
- **검사 실패 가시성**: baseline·모듈 열거 실패와 정상 무변화를 구분하고 검사 시각·성공 여부를 영속 로그로 기록한다. 현재는 디버그 출력 위주라 사후 분석 근거가 제한된다.
- **무결성 검사 커버리지**: 실제 보호가 필요한 게임 로직 범위를 재정의하고, 정의만 된 Hero CRC의 호출 여부를 결정한다. 특정 입력에서만 실행되는 CRC의 검사 시점도 검토한다. 커버리지 변경은 별도 방어 버전으로 시험한다.
- **재현성**: 빌드별 심볼·해시·예상 바이트·CRC를 묶고, 공격 효과·탐지·차단·게임 종료를 별도 필드로 기록한다. 사용자 다른 작업 파일과 기존 실험 증거는 보존한다.

이번 산출물은 **관측 DLL 사본과 하네스 준비, 오프라인 테스트 통과, 새 기준선 오탐 확인, 방어 커버리지 재확인**이다. 실제 공격 효과와 지속 플레이 검증을 완료한 것으로 기록하지 않는다.
