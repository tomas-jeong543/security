# EASILE HK-01 작업 맥락 및 인수인계

작성일: 2026-11-07 (Asia/Seoul)

이 문서는 현재 세션의 대화, 이미 수행한 파일·소스·PE/PDB 분석, 실행 로그 및 GUI 관측을 요약한다. 문서 작성 과정에서는 새로운 빌드, 게임 실행, DLL 주입, 후킹 실행 또는 소스·바이너리 변경을 수행하지 않았다. 아래 실행·수정 이력은 **이전 작업의 기록**이다.

## 1. 프로젝트 개요 및 목표

EASILE은 사용자가 직접 개발하고 소유한 Windows C++17 / DirectX 11 기반 2D 게임이다. 개인 PC의 격리된 로컬 테스트 환경에서 DLL Injection, API Hooking, 코드 무결성 검사 및 안티치트 방어의 원리와 실제 효과를 검증한다. 대상은 사용자 소유의 GameClient.exe와 로컬 테스트 DLL에 한정한다. 제3자 프로그램, 외부 서버 또는 타인의 시스템은 범위에 포함하지 않는다.

실험의 장기 목표는 공격 구현의 효과를 측정하고, 방어의 탐지 범위와 대응 한계를 확인한 뒤 개선하는 것이다. 이번 단계에서는 기존 DLL Injection 프로젝트를 분석하고 HK-01 인라인 후킹 실험과 구분하여, 재현 가능한 비교 실험과 증거 수집 방법을 정리했다.

현재 진행 단계는 다음과 같다.

1. 기존 DLL Injection 및 HK-01 소스 분석: 완료.
2. 정상 실행의 Windows DLL 오탐 원인 조사 및 제한적 수정: 이전 작업에서 수행.
3. 수정 빌드의 CRC·스모크 실행 확인: 완료한 범위가 있음.
4. DLL 로드·HK-01 설치·DLLGuard 탐지 및 경고 확인 후 종료: 이전 실행에서 확인.
5. detour 호출 및 실제 공격 방향 약 30도 변경: **미검증**.
6. A/B/C 비교 실험: 계획만 수립. B 전용 테스트 빌드 생성과 비교 실험은 아직 수행하지 않음.

### 작업 권한 및 현재 제약

사용자는 이후 요청에서 분석·계획만 수행하도록 명시했고, 이번 요청은 문서화만 승인했다. **다음 세션에서 이 문서의 향후 계획을 실행 승인으로 해석하지 말 것.** 새로운 소스 수정, 빌드, 게임·인젝터 실행, DLL 주입, 후킹 실행 및 보안 기능 비활성화에는 별도 승인이 필요하다. 이전 세션에서 승인받아 수행한 수정·실행 이력과 현재 허용 범위를 구분해야 한다.

## 2. 경로 및 기존 프로젝트 분석 결과

### 2.1 기준 경로

프로젝트 상위 루트 `P`:

```text
D:\GitHub\security\projects\Game_project_easile(리버싱)
```

현재 작업 폴더 `W`:

```text
P\api_hooking_project
```

이하 `P/...`와 `W/...`는 위 경로를 기준으로 한다. 이 문서의 저장 위치는 `W/docs/HK01_CONTEXT_HANDOFF.md`이다. 게임 소스와 주 솔루션은 작업 폴더의 상위에 있으므로, 현재 workspace-write 허용 범위 밖에 있는 파일을 수정하려면 도구의 권한 절차가 필요할 수 있다. 과거 실제 소스 수정은 해당 절차를 거쳐 수행했다.

### 2.2 실제 DLL Injection 프로젝트 구조

```text
P/dll_injection_project/
├─ dll_injection_project.zip
└─ dll_injection_project/
   ├─ EASILE_PATCH02.pdf
   ├─ GameClient.log
   ├─ payload.c
   ├─ payload.dll
   ├─ before_protect/
   │  ├─ GameClient_dll_injection.exe
   │  ├─ injector2.exe
   │  ├─ payload_before_protection.c
   │  ├─ payload.dll
   │  └─ GameClient.log
   └─ after_protect/
      ├─ GameClient.exe
      ├─ injector2.exe
      ├─ payload._after_protect.c
      └─ payload.dll
```

폴더와 ZIP의 목록을 모두 확인했다. 이 프로젝트 내부에는 인젝터 소스, README, `.sln`, `.vcxproj`, 빌드 스크립트가 없었다. 관련 인젝터 소스는 `P/build/Release/injector.c`에서 발견했다. 이 소스와 기존 injector2.exe의 정확한 빌드 대응 관계는 확정하지 못했다.

`EASILE_PATCH02.pdf`는 6쪽 문서다. 압축된 텍스트와 폰트 문자 매핑을 읽어 문서의 이동속도 2배 공격, 기존 CRC 범위의 한계, 방어 후보 및 과거 결과 기록을 확인했다. PDF 내 스크린샷은 이번 분석에서 시각적으로 재검증하지 않았다. 문서에 적힌 과거 성공 결과를 현재 빌드의 실행 성공으로 승계하지 않는다.

### 2.3 구 injector.c

근거 파일: `P/build/Release/injector.c`.

| 위치 | 확인한 구현 |
|---|---|
| `find_remote_module()`, 9행 | Toolhelp 모듈 열거 함수. 주입 함수에서는 사용하지 않음 |
| `inject()`, 25행 | 원격 메모리에 DLL 경로를 쓰고 LoadLibraryA 원격 스레드 생성 |
| 36행 | `OpenProcess(PROCESS_ALL_ACCESS)` |
| 40~49행 | `VirtualAllocEx` → `WriteProcessMemory` → 로컬 LoadLibraryA 주소 → `CreateRemoteThread` |
| 51~53행 | 실제 완료 대기·결과 검사 없이 성공 메시지와 0 반환 |
| `main()`, 56행 | PID 수동 입력 후 `inject(pid, "payload.dll")` 호출 |

프로세스 이름·전체 경로 기반 자동 검색은 없다. 구 코드의 성공 메시지는 성공의 증거로 사용할 수 없다. API 실패 처리, 종료 NUL을 명시적으로 포함한 경로 전달, 원격 함수 주소 검증, 비트수 검사, 완료 대기, 로드 모듈 확인, 핸들 및 원격 메모리 정리가 부족하다. 상대 경로는 대상의 DLL 검색 환경에 영향을 받는다.

구 injector2.exe의 import에서도 OpenProcess, VirtualAllocEx, WriteProcessMemory, GetProcAddress, CreateRemoteThread를 확인했다. 소스의 방식과 부합하지만, import만으로 정확한 소스·빌드 이력을 증명하지는 않는다.

### 2.4 기존 payload.c: 이동속도 패치

세 페이로드 모두 `DllMain(DLL_PROCESS_ATTACH)`에서 `PatchHeroSpeed()`를 직접 호출한다. 게임 베이스 주소에 고정 RVA를 더하고, 기대 바이트와 일치할 때만 `VirtualProtect`로 쓰기 가능하게 변경한 뒤 속도 초기화 명령의 상수를 220.0f에서 440.0f로 바꾼다. 이후 FlushInstructionCache를 호출하고 메모리 보호 속성을 복원한다.

| 소스 | 함수 위치 | 고정 RVA |
|---|---|---|
| `P/dll_injection_project/dll_injection_project/payload.c` | `PatchHeroSpeed()` 9행, DllMain 38행 | `0x110A8` |
| 같은 프로젝트의 `before_protect/payload_before_protection.c` | PatchHeroSpeed 10행, DllMain 39행 | `0x10DE8` |
| 같은 프로젝트의 `after_protect/payload._after_protect.c` | PatchHeroSpeed 11행, DllMain 40행 | `0x11158` |

기대 명령은 `C7 41 3C 00 00 5C 43`, 패치 명령은 `C7 41 3C 00 00 DC 43`이다. 이는 **코드 즉시값 패치**이며 IAT·vtable·DX11 후킹이나 trampoline 기반 인라인 후킹이 아니다. 원래 코드 바이트를 되돌리는 기능은 없다. 보호 속성 복원 API의 성공 여부도 확인하지 않는다.

이 패치는 기존 플레이어 객체의 speed_를 직접 바꾸지 않는다. 패치 후 초기화 명령이 실행돼야 새 객체의 속도에 영향을 준다. 현재 게임에서는 시작·재시작에 따른 Initialize 호출 시점이 중요하다. 단순 DLL 로드·패치 적용만으로 이동속도 증가가 입증되지는 않는다.

### 2.5 HK-01 프로젝트: 별도 인라인 후킹 구현

위치: `W/EASILE_HK01_starter_fixed/easile_hk01/`.

주요 파일은 `injector.c`, `hook_payload.c`, 두 `.vcxproj`, `EASILE_HK01.sln`, `README.md`이며 기존 산출물은 `out/x64/Release/`에 있다. 루트 W에도 injector.exe와 hook_payload.dll이 있다.

HK-01 인젝터는 구 인젝터보다 오류 처리와 관측성이 개선되어 있다.

- `injector.c::find_remote_module()`, 21행: 대상 모듈 열거.
- `remote_load_library()`, 43행: 로컬 LoadLibraryW 소유 모듈과 export RVA를 이용해 원격 주소 계산.
- `inject()`, 62행: Unicode 절대 경로, 필요한 프로세스 권한, 대상 비트수, 중복 모듈, 쓰기 결과, 원격 스레드 완료 대기 및 모듈 확인.
- `wmain()`, 161행: PID·DLL 경로 인자 또는 PID 입력 처리.
- 원격 스레드 DWORD 종료값을 완전한 x64 HMODULE로 간주하지 않고 모듈 목록을 확인한다. 다만 현재 모듈 검사는 기본 파일명을 기준으로 하므로 전체 경로 대조를 보강할 여지가 있다.

`hook_payload.c`의 주요 구조:

| 함수·행 | 역할 |
|---|---|
| `detour_spawn()`, 44행 | ID 3 Bot의 Basic만 방향 +30도 회전 후 원본 호출 |
| `verify_target_build()`, 61행 | x64 PE·RVA 범위·메모리 상태·진입 바이트 검사 |
| `install_hook()`, 115행 | MH_CreateHook → MH_EnableHook |
| `shutdown_hook()`, 143행 | 비활성화, 활성 detour 대기, 제거·MinHook 정리 |
| `HK01_RequestShutdown()`, 186행 | 프로세스 내부에서 호출하는 종료 요청 API |
| `hook_worker()`, 194행 | MinHook 초기화·설치·종료 이벤트 대기 |
| `DllMain()`, 231행 | 워커 생성. detach에서 직접 MinHook 정리하지 않음 |

회전식은 `x'=0.8660254*x-0.5*y`, `y'=0.5*x+0.8660254*y`이며 조건은 `sourceId == 3 && kind == 0`이다. 수학적으로 +30도 회전이다. 화면상 시계·반시계 방향은 Y축 방향을 고려해야 한다.

README에는 Hook 미구현 TODO 설명이 남아 있지만 현재 소스에는 설치·회전·해제 코드가 있다. 이 부분은 실제 소스를 우선한다. HK01_RequestShutdown은 인젝터의 원격 종료·언로드 기능을 뜻하지 않는다.

구 인젝터와 속도 DLL은 확인한 PE에서 x64이며 VCRUNTIME140·UCRT 계열 의존성이 있다. 구 프로젝트 빌드 설정은 없으므로 정확한 컴파일 플래그는 미확정이다. HK-01 프로젝트는 VS2022/v143·x64·/MT 및 MinHook 정적 라이브러리를 지정한다. MinHookRoot 설정은 `D:\Downloads\minhook-1.3.4\minhook-1.3.4`다. W의 현재 HK-01 EXE/DLL은 직접 import하는 DLL이 KERNEL32.dll뿐이었다.

## 3. EASILE 동작 및 기존 방어 구조

### 3.1 공격과 관련된 게임 흐름

| 파일·위치 | 확인한 사실 |
|---|---|
| `P/src/Application.cpp::Run()`, 22행 | 창·DX11·HUD 초기화, 입력·시뮬레이션·렌더링 루프 |
| `P/src/Renderer.cpp::Initialize()`, 12행 | D3D11CreateDeviceAndSwapChain 사용 |
| 같은 파일 `Render()`, 69행 | Draw 기반 렌더링 및 139행 Present. 검토한 페이로드에는 DX11 후킹 없음 |
| `P/src/InputManager.cpp::Read()`, 26행 | 전경 창일 때만 입력. Enter 시작, WASD 이동, K/LMB Basic, J Ultimate, Tab 조준 모드 |
| `P/src/GameWorld.cpp`, 49·84행 | Initialize와 Restart에서 게임 객체 재초기화 |
| `P/src/Hero.h`, 36·57행 | 이동은 방향×speed_×시간, 기본 speed_=220.0f |
| `P/src/PlayerController.h::AimDirection()`, 13행 | 마우스 위치와 플레이어 위치 차이 정규화 |
| `P/src/GameWorld.cpp`, 722행 부근 | Bot 목표 방향 계산 후 SpawnProjectile 호출 |
| 같은 파일 `SpawnProjectile()`, 133행 | 투사체 생성·발사자 저장·방향 로그 |
| `P/src/CombatSystem.h::CreateProjectile()`, 9행 | 생성 방향 정규화 |

Heroes() 순서는 player_, bot_, allyBot_, enemyBot_이며 HeroId는 그 인덱스를 반환한다(GameWorld.cpp 34~40행). 따라서 현재 HK-01은 **적 Bot 전체가 아니라 ID 3인 enemyBot_의 Basic만** 변경한다. ID 1 Bot을 관찰하면 변화가 보이지 않을 수 있다. 기존 PROJECTILE_SPAWN 로그에는 발사자 ID가 없어 두 적 Bot을 명확히 구분하기 어렵다.

### 3.2 CRC 검사

`P/src/IntegrityGuard.cpp`:

- 23~30행: Update 및 Initialize의 RVA·길이·기대 CRC 상수.
- `CalculateCRC32()`, 33행: CRC32 계산.
- `VerifyRegionIntegrity()`, 53행: 실행 중 메모리의 지정 영역 CRC를 계산하고 OutputDebugString으로 출력한 후 기대값과 비교.
- `VerifyUpdateIntegrity()`, 83행: Update 영역 검사.
- `VerifyHeroIntegrity()`, 95행: Initialize 영역 검사 함수.

현재 `src` 검색에서 VerifyHeroIntegrity의 호출 지점을 찾지 못했다. **정의와 기대값이 있다는 이유로 Hero CRC가 활성 검사라고 판단하면 안 된다.**

실제 Update CRC 호출은 `GameWorld.cpp` 916~925행의 플레이어 궁극기 명령 처리에 있다. 실패하면 SECURITY/INTEGRITY_FAIL을 기록하고 해당 Update 호출에서 반환한다. 이 분기 자체에는 MessageBox나 종료 코드가 없다. 모든 프레임·모든 함수에 대한 상시 검사가 아니다.

현재 SpawnProjectile 영역은 아래 Update·Initialize CRC 범위에 포함되지 않는다. HK-01 변경을 Update CRC가 직접 검출한다고 가정할 근거가 없다.

### 3.3 DLLGuard

- `Application.cpp` 124~134행: 최초 렌더링 이후 baseline 생성.
- 같은 파일 60~65행: 메인 루프에서 약 1초 간격으로 DetectNewModules 호출.
- `IntegrityGuard.cpp::CaptureLoadedDlls()`, 167행: 현재 프로세스의 DLL 전체 경로 수집.
- `InitializeDllBaseline()`, 209행: 최초 목록 고정.
- `DetectNewModules()`, 235행: baseline에 없고 이전에 보고하지 않은 DLL을 확인.
- 257행: 제한적 시스템 DLL 예외 검사.
- 265~277행: 탐지 디버그 출력 → `게임 파일이 이상합니다.` 경고창 → MessageBox 반환 후 ExitProcess(1).

이는 DLL 로드 자체를 가로막는 로더 차단 기능이 아니라 **이미 로드된 모듈을 주기적으로 검사하는 사후 탐지**다. 워커의 후킹 설치와 탐지 순서는 고정되지 않는다. 탐지 전에 효과가 발생했는지도 별도로 측정해야 한다.

IsVerifiedSystemDll(129행)은 System32 전체 경로와 고정 SHA-256이 일치하는 다섯 파일만 허용한다: oleacc.dll, clbcatq.dll, threadpoolwinrt.dll, CoreUIComponents.dll, CoreMessaging.dll. 이전 수정 시 각 파일의 Microsoft Windows 서명 Valid를 확인한 뒤 해시를 고정했다. 런타임에 모든 DLL의 Authenticode 서명을 일반 검증하는 구조는 아니다. Windows 업데이트 후 파일이 바뀌면 재검증이 필요하다.

### 3.4 SpeedGuard

`GameWorld.cpp` 860~890행에서 플레이어 이동 및 맵 제한 적용 후 실제 이동거리와 `220 * deltaSeconds * 1.15 + 0.001`을 비교한다. 초과 시 SpeedGuard 디버그 출력·SPEED_HACK_DETECTED 로그·별도 이동속도 경고 후 ExitProcess(1)이다. 정상 속도 업그레이드와 고정 상한의 정합성은 후속 검토 대상이며 이번 세션에서 전체 검증하지 않았다.

## 4. 실제 수행한 수정·빌드·실행 이력

### 4.1 수정·생성한 파일

이전 승인 작업에서 다음을 변경했다.

| 파일 | 실제 변경 내용 |
|---|---|
| `P/src/IntegrityGuard.cpp` | 정상 실행 중 탐지된 Windows DLL 다섯 개를 정확한 System32 경로·SHA-256으로만 예외 처리. crypt32 해시 계산 추가. 새 정상 빌드에 맞춰 Update 기대 CRC를 29148A8C로 갱신 |
| `P/build/Release/GameClient.exe`, PDB 등 | 위 변경의 빌드 산출물 |
| `W/GameClient.exe` | 검증한 Release 수정본으로 교체. build/Release와 해시 동일 확인 |
| `W/.hk01_validation/GameClient.before_dllguard_fix.exe` | 루트 기존 EXE 백업 |
| `W/.hk01_validation/IntegrityGuard.cpp` | 수정본 준비용 사본 |
| `W/.hk01_validation/Run-Validation.ps1` | 임시 로그 폴더·디버그 출력 수집·선택적 스모크/주입 실행용 보조 스크립트 |
| `W/.hk01_validation/Fix-VerifiedDlls.ps1` | 이전 작업의 개별 시스템 DLL 서명·해시 확인 및 수정·빌드 보조 스크립트 |
| `W/HK01_validation_result.md` | 5단계 검증 결과 및 제한사항 기록 |

위 보조 스크립트에는 실행·주입 또는 외부 소스 수정·빌드 기능이 있다. **다음 세션에서 읽기 전용 도구라고 오인해 실행하지 말 것.** 구문·인코딩·생성 로그 혼입 오류를 중간에 수정한 이력도 있으며, 재사용 전 필요한 범위만 검토해야 한다.

W/injector.exe 및 W/hook_payload.dll은 이 오탐 수정 작업에서 변경하지 않았다. 게임의 CRC 및 DLLGuard 전체를 비활성화하지 않았다. DLLGuard 비활성 B 전용 빌드도 아직 만들지 않았다.

### 4.2 빌드 및 테스트 이력의 제한

Release 솔루션 빌드에서 게임 산출물 생성은 확인했다. 중간에는 실행 중 GameTests.exe의 파일 잠금 때문에 테스트 타깃 링크가 LNK1168로 실패했고, 이후 게임 타깃만 빌드했다. **전체 GameTests 통과로 보고하면 안 된다.** 실행한 GameTests는 완료되지 않아 종료했다.

GameTests가 W/GameClient.log에 테스트 기록과 INTEGRITY_FAIL을 추가했다. 이 혼입 기록을 실제 GameClient의 공격 성공·방어 실패 근거로 사용하지 않는다. 기존 로그를 되돌렸다고 보고한 적은 없다. 이후 중요한 실행은 독립 임시 작업 폴더에서 로그를 수집했다.

### 4.3 확인한 실행 결과

| 항목 | 실제 확인 결과 |
|---|---|
| 최초 주입 | PID 34232의 OpenProcess가 Error=5, injector 종료 코드 1. 권한 실패이며 방어 탐지 성공으로 분류하지 않음 |
| 권한을 맞춘 재시도 | DLL 로드·대상 모듈 확인 성공. 처음에는 디버그 출력 미수집으로 설치는 미검증이었음 |
| 무주입 경고 재현 | PID 9520에서 baseline 생성 뒤 oleacc.dll 신규 탐지 로그 및 같은 경고 GUI 확인 |
| 수정 후 정상 실행 | 무주입 상태에서 15초간 DLLGuard 경고 없이 유지한 범위를 확인. 메뉴 화면 관찰 |
| 정상 스모크 | PID 10136, `--smoke-gui`, Update CRC=29148A8C 출력, MATCH·투사체 기록, 5프레임 렌더, 종료 코드 0 |
| 최종 주입 실행 | PID 11368, injector 종료 코드 0, 대상 모듈의 hook_payload.dll 확인 |
| 후킹 설치 | 같은 실행에서 `[HK-01] Hook installed.` 디버그 출력 확인 |
| DLLGuard | 같은 실행에서 hook_payload.dll 전체 경로의 New DLL detected 출력 확인 |
| 경고·종료 GUI | 이후 경고 문구를 읽고 확인 버튼 클릭. 창과 PID 11368이 사라진 것을 관찰. 이 최종 종료의 실제 숫자 코드는 별도 수집하지 못함 |
| detour 호출·30도 효과 | 메뉴 상태의 공격 실행으로 전투 및 입력/출력 방향 증거 없음. **미검증** |

최종 주입 debug.log에는 DLLGuard 탐지 메시지와 Hook installed가 모두 있다. 로드·설치·탐지가 공존하므로 이를 주입 자체 차단으로 기록하면 안 된다.

### 4.4 주요 증거 위치

다음은 이전 실행 당시 실제 사용한 임시 디렉터리다. 임시 파일은 향후 정리될 수 있으므로 필요 시 존재 여부만 확인하고 보존 여부를 결정한다.

```text
C:\TEMPFO~1\HK01_noinject_20261106_212534\
  무주입 oleacc.dll 탐지 및 초기화 로그
C:\TEMPFO~1\HK01_fixed_normal_20261106_214426\
  수정 후 무주입 15초 관측
C:\TEMPFO~1\HK01_fixed_normal_20261106_214615\
  정상 스모크, CRC, 전투·5프레임·종료 기록
C:\TEMPFO~1\HK01_fixed_inject_20261106_214615\
  DLL 로드 이후 Hook installed 및 DLLGuard 탐지 기록
```

Logger.h 14행은 작업 디렉터리의 GameClient.log에 append한다. OutputDebugString은 자동으로 GameClient.log에 들어가지 않으므로 위 실행에서는 debug.log를 별도 수집했다. GUI 관측과 종료 코드·디버그 출력은 같은 증거가 아니며 서로 대체하지 않는다.

## 5. 기술적 참고: 빌드·주소·해시

### 5.1 현재 빌드의 주소와 CRC

아래 값은 SHA-256 `32A1D0D4D8FBF15E2A1E45467475A6057F26EEE54B1AF5746945DE24C768A926`인 현재 Release EXE와 일치하는 PDB를 정적으로 대조한 결과다. 새 빌드의 보장값이 아니다.

| 항목 | RVA | 길이 | 확인값 |
|---|---|---|---|
| GameWorld::Initialize | 0x110D0 | 0x685 | CRC A8E8248A |
| GameWorld::Update | 0x14820 | 0x1444 | CRC 29148A8C |
| GameWorld::SpawnProjectile | 0x137E0 | 0x8B5 | 진입 14바이트 `48 8B C4 48 89 58 18 55 56 57 41 54 41 55` |
| 현재 속도 초기화 명령 후보 | 0x11158 | 7바이트 비교 | 기존 after_protect 소스의 기대 명령과 일치 |

현재 EXE에서 기존 RVA 0x10DE8 및 0x110A8의 바이트는 속도 페이로드 기대 명령과 불일치했다. 원본 오탐 수정 전 EXE의 같은 Update 범위 CRC는 7AD446CE이며 원래 소스 기대값과 일치했다. 수정 빌드의 29148A8C와 혼동하면 안 된다.

PE/PDB 대조와 CRC 계산은 파일을 읽는 분석으로 수행했다. 메모리의 모든 시점·모든 영역이 무변조라는 의미는 아니다. 새 B 빌드에서는 함수 위치·ABI·CRC·기대 바이트·PDB 식별 정보를 다시 확인해야 한다.

### 5.2 EXE 식별표

경로는 P 기준이며 같은 행의 파일은 확인 시 SHA-256이 동일했다. 전부 확인한 PE Machine은 x64(0x8664)다. 파일명만으로 방어 설정이나 세부 컴파일 플래그를 확정하지 않았다.

| 경로·용도 | SHA-256 |
|---|---|
| api_hooking_project/GameClient.exe 및 build/Release/GameClient.exe — 현재 Release 수정본 | `32A1D0D4D8FBF15E2A1E45467475A6057F26EEE54B1AF5746945DE24C768A926` |
| dll_injection_project/dll_injection_project/after_protect/GameClient.exe 및 W/.hk01_validation/GameClient.before_dllguard_fix.exe — 과거 비교본·수정 전 백업 | `BCBCE4F49D56A10AA221CACBA69A88714B712B239480E235A5F2F9A1B5E60EBC` |
| dll_injection_project/dll_injection_project/before_protect/GameClient_dll_injection.exe, build/Release/GameClient_patch_after_protect.exe, release_v10_03/GameClient_patch_after_protect.exe — 동일한 과거 비교본 | `5EE636BB83864E49ADE4C04571E220DF90DE588800E62A3E357D5E224DF628C5` |
| release_v10_03/GameClient.exe — 과거 PDF 대상 해시와 일치 | `17B41D48710F1BCF83CE120AD8871EAB765060BA80B6071CF313FF53BA9CF38F` |
| build/Release/GameClient_patched.exe 및 release_v10_03/GameClient_patched.exe — 변경 내역 미확정인 과거 변형본 | `18CA7DEFC6324C7409A62F248B15A109C2D8FC4ED9A05E7C5282518E13D35F73` |
| build/Debug/GameClient.exe — Debug 경로·Debug CRT 의존성 확인 | `5D69A9C6694C9DD6616EFCA4323F93C1ADEF937C1F0F3C21BC1DF9263AC5B113` |

현재 게임 프로젝트 설정은 `P/GameClient/GameClient.vcxproj`이며 Debug/Release x64, v143, C++17을 사용한다. 현재 게임은 MSVCP140·VCRUNTIME140·UCRT, DirectX 관련 DLL 및 추가된 CRYPT32를 import한다. Debug 바이너리에는 Debug CRT 의존성이 있다.

### 5.3 인젝터·DLL 식별표

| 파일 | SHA-256 |
|---|---|
| W/injector.exe | `EA2F820B2D62F445C9DCDA2B3E39F023ABC563D32D9F6D9C28C2B02A080931BD` |
| W/hook_payload.dll | `296769092E14E4EC1003A8B955D26E03D8820AEDA30BA07EA303648D73697591` |
| 기존 before_protect·after_protect 및 P/build/Release의 injector2.exe | `8BA53C19332B40D84307D1C44B7DC912206B19C812B2E85A3A9DCF1DE21A02CF` |
| 기존 프로젝트 최상위 payload.dll | `3EA784358158A050459EDCD0896233ED672C00F77A4A3FCCA669CB83C0A40342` |
| 기존 before_protect/payload.dll | `AE9B1D979503E6F5EC2025F643F0E17345A5AA2CF662EFCD8D1E50D71421B23D` |
| 기존 after_protect/payload.dll | `6D2722E29BF8ABCF494EAAF0B76485D3D9E088C43BBA4F11B7857C1E9BE59AF4` |

기존 페이로드의 동일한 이름을 가진 DLL들은 내용이 다르므로 파일명만으로 선택하지 않는다.

## 6. 현재 상태, 미해결 과제와 판정 기준

### 6.1 확인된 사실과 추정의 구분

| 구분 | 내용 |
|---|---|
| 확인됨 | 기존 속도 패치와 HK-01 방향 후킹은 별도 구현. 현재 DLL은 로드·설치됐고 DLLGuard가 탐지함 |
| 확인됨 | 정상 스모크는 CRC와 5프레임을 확인하고 코드 0으로 종료. 전체 GameTests 통과를 의미하지 않음 |
| 확인됨 | 현재 Hero CRC 함수는 정의됐지만 호출 지점 미발견. SpawnProjectile은 현재 CRC 대상 밖에 있음 |
| 미검증 | detour 실제 호출, ID 3 Basic의 약 30도 변화, 장시간 수동 플레이 |
| 추정 | DLLGuard 검사 전에 효과가 발생할 수 있음. 실제 발생 여부·시각은 측정 필요 |

### 6.2 게임 창 복원 문제

Computer Use는 `@oai/sky`를 사용했다. 중간의 Esc 중단은 사용자가 실수라고 설명하여 재개했다. 최종적으로 게임 창이 최소화되어 activate_window와 상태 재조회를 재시도했으나 복원하지 못했다. 입력 충돌 및 `window is minimized` 오류가 보고됐고 수동 플레이 검증은 미완료다. 최소화 또는 복원 실패를 보안 방어 성공으로 판단하면 안 된다.

### 6.3 판정 기준

- DLL 로드 성공: 대상 모듈 목록에서 의도한 DLL의 전체 경로 확인.
- 후킹 설치 성공: 대상 함수에 대한 MinHook 활성화 확인.
- 공격 효과 성공: detour 호출과 실제 투사체 방향 변경을 같은 발사 이벤트에서 확인.
- 탐지 성공: 주입 DLL과 연결되는 DLLGuard 이벤트 확인.
- 로드 차단 성공: 방어가 DLL 로드 자체를 거부했다는 증거 필요.
- 효과 차단 성공: 충분한 실행·관측 조건에서 효과 발생 전에 방어가 개입했다는 증거 필요.
- 확인 불가: 메뉴 상태, 발사 없음, 로그 부족, GUI 실패 등. 성공이나 실패로 바꾸지 않음.

기존 HK-01 로그에는 설치와 탐지가 모두 있으므로 DLL 로드 자체를 차단했다고 볼 수 없다. 경고 후 종료를 확인했어도 메뉴 상태의 실행에서 방향 변경 효과 차단까지 입증할 수는 없다.

## 7. 다음 세션 인수인계와 우선순위

### 7.1 추천 A/B/C 비교 계획 — 미실행

| 실험 | DLL 주입 | DLLGuard | 확인 항목 |
|---|---|---|---|
| A 기준선 | 없음 | 활성 | 정상 실행, 창 복원, 입력, 방향 조작, CRC, 종료 코드 |
| B 공격 기능 | 있음 | 원본과 독립된 테스트 빌드에서만 비활성 | 로드, 설치, detour 호출, 실제 30도 변경, 비대상 불변 |
| C 방어 검증 | 있음 | 활성 | 로드·설치·효과·탐지·경고·종료의 시각과 순서 |

B 구성은 계획이며 아직 만들지 않았다. 실행에는 승인이 필요하다. 원본 소스·실험 바이너리를 덮어쓰지 않고 별도 소스 작업 사본과 출력 폴더를 사용하며, DLLGuard 이외의 CRC·SpeedGuard 등은 유지한다. 새 빌드 각각의 EXE/PDB·RVA·CRC를 다시 측정한다.

### 7.2 다음 작업 순서

1. 새로운 실행·변경의 승인 범위를 확인한다. 이번 문서화 요청을 실험 승인으로 확대하지 않는다.
2. 현재 Release 전체 경로·해시로 대상을 고정하고 A에서 GUI 복원과 수동 시작·입력을 먼저 확인한다. 자동 복원이 불가능하면 수동 확인을 제안하고, 불가능한 항목은 미검증으로 남긴다.
3. 승인 후 HK-01 detour와 게임 발사 로그에 최소한의 관측을 추가하는 안을 구체화한다. 발사자 ID, kind, 입력·출력 방향, 발사 이벤트 ID를 연결한다.
4. B의 별도 테스트 구성에서 효과를 확인한 뒤 C에서 탐지와 효과 발생의 선후 관계를 측정한다. 기존 속도 공격과 혼합하지 않는다.
5. 실행별 PID·EXE/PDB/DLL/injector 해시, 전체 경로, 작업 폴더, 표준 출력·오류, 종료 코드, 모듈, 후킹 상태, 영상·화면, 게임 로그를 개별 보관한다.

방향 차이는 같은 호출의 입력·출력 벡터로 `atan2(x*y' - y*x', x*x' + y*y')`를 계산한다. 제안 기준은 비영벡터인 ID 3 Basic 여러 표본에서 +30°±0.1°, 길이 보존, 비대상 호출 불변, 실제 생성 방향 일치다. 서로 다른 시점의 Bot 조준각을 단순 비교하지 않는다.

### 7.3 승인 후 수정 검토 대상 — 이번에는 수정하지 않음

| 파일 | 수정 취지 |
|---|---|
| HK-01 hook_payload.c | 빌드 식별, detour 호출·방향 변환 관측 로그 |
| HK-01 injector.c | 기존 개선 구조 재사용. 대상 EXE 해시·전체 경로 및 DLL 전체 경로 대조 보강 |
| P/src/GameWorld.cpp | SpawnProjectile에 발사자·이벤트·실제 생성 방향 로그 |
| P/src/Application.cpp | baseline·검사·경고 시각 기록. 승인된 B 전용 구성 분리 |
| P/src/IntegrityGuard.cpp | 탐지·예외·검사 실패 구조화 기록. 원본 종료 정책 유지 |
| P/GameClient/GameClient.vcxproj | 명시적 B 테스트 구성과 독립 OutDir/IntDir |
| HK-01 README.md | 현재 소스와 다른 TODO 설명 및 실험·증거 규칙 갱신 |

Hero CRC 호출 추가는 별도 방어 개선으로 검토하며 효과 검증과 계획 없이 동시에 변경하지 않는다. 구 injector.c의 재개발은 우선순위가 낮다. 이미 개선된 HK-01 인젝터를 재사용하면 변경량을 줄일 수 있다.

### 7.4 반복해서 분석할 필요가 없는 기존 결론

- 기존 DLL Injection은 속도 초기화 코드 패치이며 HK-01은 별도의 MinHook 방향 변경 구현이다.
- 같은 이름의 GameClient.exe·payload.dll도 해시가 다르다. before/after 이름만으로 방어 구성을 추정하지 않는다.
- 구 인젝터의 성공 출력만으로는 증거가 부족하다.
- 현재 회전 조건은 ID 3 Basic만 해당하며 모든 적 Bot을 변경하는 구현이 아니다.
- 현재 빌드에서 과거 속도 RVA 두 개는 불일치했고 after_protect의 0x11158 및 HK-01의 0x137E0은 정적 조건이 일치했다. 새 빌드에 대한 보장은 아니다.
- DLLGuard는 사후 탐지이며 로드·설치·효과·탐지·종료는 별도 판정이다.
- 현재 CRC 범위와 호출 시점은 모든 변조를 탐지하는 구성이 아니다.
- 스모크 성공, 15초 실행, GUI 메뉴 관측, 장시간 수동 플레이는 서로 다른 검증 범위다.
- 실제 30도 변경은 미검증이다. 다음 핵심 과제는 기존 구현의 효과 및 방어 시계열을 관측하는 것이다.
