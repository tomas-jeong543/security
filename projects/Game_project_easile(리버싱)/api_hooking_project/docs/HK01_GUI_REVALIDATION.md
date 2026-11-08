# EASILE HK-01 — GameClient GUI 재검증

검증일: 2026-11-07, Asia/Seoul. JSONL 시각은 UTC다.

**결과: 실행·창 생성·메뉴 접근성 확인 및 보안 경고 확인 후 종료는 검증했다. Computer Use의 실제 플레이 검증은 완료하지 못했다.** Enter API가 반환된 실행에서도 시작·전투 로그는 없었다. WASD·기본 공격·궁극기는 실행하지 않았으며 성공으로 기록하지 않는다.

사용자가 동일 PC에서 지정 EXE를 직접 실행하면 정상 표시·플레이된다는 사실을 전제로 조사했다. 이번 자동화 실패를 게임 자체의 실행 불가로 판단하지 않는다. 기존 EXE·소스·방어 기능은 변경하지 않았고 빌드·DLL 주입·후킹도 수행하지 않았다.

## 1. 대상·환경·기존 자료

```text
P = D:\GitHub\security\projects\Game_project_easile(리버싱)
W = P\api_hooking_project
E = W\.hk01_validation\gui_revalidation
대상 = W\GameClient.exe
```

대상 SHA-256:

```text
32A1D0D4D8FBF15E2A1E45467475A6057F26EEE54B1AF5746945DE24C768A926
```

P\build\Release\GameClient.exe도 동일하다. 실행 전 남은 GameClient 프로세스는 없었다. 이번 테스트에서 생성한 프로세스만 구분해 정리했다.

기존 [인수인계](HK01_CONTEXT_HANDOFF.md), [공격·방어 보고서](HK01_ATTACK_DEFENSE_RESULT.md)의 분석을 재사용했다. 사용자 추가 지시에 따라 상위 P의 목록, README, src/main.cpp, Application.cpp, tools/gui_input_check.ps1, evidence/v014-validation.md, docs/BASELINE_1.0.md를 필요한 범위만 확인했다.

상위 자료의 핵심:

- README와 BASELINE 문서에 사용자 직접 플레이 승인 및 과거 GUI 시험 기록이 있다. 과거 시험을 이번 실행의 성공으로 대체하지 않는다.
- Application.cpp는 일반 Win32 OVERLAPPEDWINDOW를 만들고 30행에서 `ShowWindow(window_, SW_SHOWDEFAULT)`를 호출한다. main.cpp는 일반 실행과 `--smoke-gui`만 구분한다. 이번 명령에는 smoke 인자가 없다.
- src 검색에서 게임 메인 창을 명시적으로 최소화하는 코드는 확인되지 않았다. HUDRenderer의 SW_HIDE는 HUD overlay에 대한 코드다. 따라서 최소화 오류를 게임의 명시적 최소화 처리 탓으로 단정할 근거가 없다.
- InputManager.cpp 29행은 `GetForegroundWindow() != window`이면 입력을 버린다. 포커스 확보 없이 API 호출이 반환되는 것만으로 입력 성공을 인정할 수 없다.
- tools/gui_input_check.ps1은 예전 Win32 ShowWindow/SetForegroundWindow/keybd_event 방식의 GUI 시험 스크립트다. 읽기만 했고 실행하지 않았다. 이번에는 Computer Use 경로를 평가했다.

## 2. 실행 방법·명령

파일·프로세스 확인은 PowerShell 읽기 전용 조회, GUI 조작은 computer-use 스킬의 `node_repl + @oai/sky`를 사용했다.

```powershell
Get-FileHash -LiteralPath 'GameClient.exe' -Algorithm SHA256
Get-Process -Name GameClient -ErrorAction SilentlyContinue |
  Select-Object Id,Path,SessionId,MainWindowHandle,MainWindowTitle,Responding
Get-CimInstance Win32_Process -Filter "Name='GameClient.exe' OR Name='explorer.exe'" |
  Select-Object ProcessId,ParentProcessId,ExecutablePath,SessionId,CommandLine
```

일반 권한 CIM 조회는 HRESULT `0x80041003`, Access denied로 실패했다. 정식 권한 요청 후 조회했으며 보안 설정을 변경하거나 제한을 우회하지 않았다. 일반 조회의 MainWindowHandle=0만으로 창 부재를 판정하지 않았다. 같은 프로세스를 권한 있는 조회로 확인하면 실제 HWND와 제목이 반환됐다.

첫 실행은 Computer Use가 직접 수행했다.

```javascript
await sky.launch_app({
  app: 'D:\\GitHub\\security\\projects\\Game_project_easile(리버싱)\\api_hooking_project\\GameClient.exe'
});
await sky.list_windows();
// 반환된 정확한 app/id로 get_window, activate_window, get_window_state 수행
```

비교 실행에는 새 [Collect-Gui.ps1](../.hk01_validation/gui_revalidation/Collect-Gui.ps1)을 사용했다. 이 스크립트에는 빌드·인젝터·후킹 기능이 없다. 독립 CWD에서 로그를 수집하고 대상 PID의 OutputDebugString과 종료 코드를 기록한다.

```powershell
& '.\.hk01_validation\gui_revalidation\Collect-Gui.ps1' -Seconds 600
# 스크립트 내부의 실제 실행:
Start-Process -FilePath $exe -WorkingDirectory $runDir -WindowStyle Normal -PassThru
```

이를 두 번 실행했다. 명시적 Normal 창 표시, 알려진 CWD, 실행별 로그를 사용해 Computer Use 기본 실행과 비교한 것이다. 두 수집기는 게임 종료를 관찰하고 각각 종료 코드 0으로 끝났으며, 게임 자체 종료 코드는 별도로 수집했다.

## 3. 프로세스·창·세션과 실제 수행 순서

| 실행 | PID / 메인 창 식별자 | 실행 경로·상태 | 결과 |
|---|---|---|---|
| A: Computer Use 직접 실행 | 20764 / 12847860 | 지정 EXE. 부모 PID 23260은 codex-computer-use.exe. 게임·실행기·Explorer 모두 세션 2 | 창 열거 성공. 상태 조회는 최소화 오류. 복원 및 표준 시스템 메뉴/종료 단축키는 입력 충돌 오류 |
| B: Normal + 접근성 조회 | 34660 / 20648322 | 지정 EXE, 독립 CWD normal_20261107_103127, 세션 2 | 초기화·baseline·Menu 접근성 확인. Enter API 반환. UIAutomationCore 신규 탐지·보안 모달 확인. 확인 버튼 후 게임 종료 코드 1 |
| C: Normal + 화면 전용 조회 시도 | 23540 / 17893508 | 지정 EXE, 독립 CWD normal_20261107_103405, 세션 2 | 활성화에서 입력 충돌, 화면 조회에서 최소화 오류. wintypes 신규 탐지·보안 모달 확인. 확인 버튼 후 게임 종료 코드 1 |

A의 12847860은 권한 있는 Get-Process 조회의 실제 MainWindowHandle과 Computer Use id가 일치했다. B/C 표의 값과 보안 경고의 15993356·14224092는 Computer Use 반환 창 id이며, 원시 HWND로 독립 재조회하지 않았다. 원래 메인 창과 다른 소유 대화상자를 구분해 선택했다.

### A — 최소화·입력 충돌 재현과 복원 시도

1. 지정 EXE를 직접 실행하고 반환된 게임 창을 선택했다.
2. `get_window_state(... include_screenshot:true, include_text:true)`에서 `window is minimized` 재현.
3. 창 목록 재수집 → 새 get_window → activate_window → get_window 재조회 → 상태 캡처 절차를 적용했다. `user input was detected in this window`가 발생했다.
4. 오류 안내에 따라 상태를 다시 조회했으나 최소화 오류가 반복됐다.
5. Computer Use JavaScript 연결을 초기화하고 list_apps부터 다시 선택했다. 활성화 후 상태 캡처에서도 최소화 오류가 재현됐다. 캐시된 JS 객체만의 문제라고 볼 수 없었다.
6. 표준 `Alt_L+space` 시스템 메뉴 경로도 입력 충돌로 실패했다. 마지막 `Alt_L+F4` 역시 같은 오류로 실패했다.

단순히 같은 입력을 반복하지 않고 새 창 객체, 연결 초기화, 표준 창 메뉴, 다른 실행 경로를 비교했다. 최종적으로 이 실행만 외부 정리했다. [initial-process-cleanup.json](../.hk01_validation/gui_revalidation/initial-process-cleanup.json)의 종료 코드는 정상 게임 종료 증거가 아니다.

### Explorer를 통한 일반 실행 경로 비교

기존 `api_hooking_project - 파일 탐색기`에서 GameClient.exe 목록 항목을 접근성으로 확인하고 더블클릭을 시도했다. `coordinate input geometry is unavailable`로 실패했고 새 게임 프로세스는 생성되지 않았다. screenshot+text 상태 재수집 및 Explorer의 표준 `Raise` 동작도 시험했다.

중요하게도 반환된 접근성 트리는 Explorer 파일 목록이었지만, 캡처 이미지는 선택한 Explorer가 아닌 브라우저·터미널이 보이는 화면이었다. 게임 캡처에서도 같은 불일치가 관찰됐다. 따라서 해당 이미지를 게임 화면 증거로 인정하거나 첨부하지 않았다. 터미널에는 GUI 입력을 하지 않았다.

### B — 메뉴 확인, Enter 반환, 실제 모달·종료 확인

Normal 실행은 초기 상태 조회가 성공해 다음 텍스트를 반환했다.

```text
State: Menu HP: 100/100 Level: 1 XP: 0/100
Basic (LMB/K): PRESS Ultimate (J): READY ...
```

이 관측 뒤 `sky.press_key({window, key:'Return'})`가 정상 반환했다. 이어진 상태 갱신은 최소화 오류였으며, GameClient.log에는 MATCH나 발사 기록이 추가되지 않았다. 따라서 **Enter 전달 API 성공과 게임 시작 성공을 구분**한다.

별도 창 목록에서 `EASILE 보안 경고`를 발견했다. 해당 대화상자의 접근성 문구는 `게임 파일이 이상합니다.`, 버튼은 `확인`이었다. 확인 버튼 클릭 뒤 수집기가 게임 종료 코드 **1**을 기록했다.

### C — 접근성 트리 없이도 복원 장애 재현

새 실행에서는 게임 메인 창에 대해 `include_text:false`로 화면 전용 캡처를 시도했다. 활성화는 입력 충돌, 재조회는 최소화 오류였고 wintypes.dll 신규 탐지가 기록됐다. 이후 진단·종료를 위해 경고창 접근성만 조회했다.

처음 screenshot 없이 텍스트만 수집한 경고창의 확인 버튼 클릭은 `coordinate input geometry is unavailable`로 실패했다. **동일 경고창의 screenshot+text를 새로 수집한 후 최신 인덱스로 재클릭하자 성공**했고 게임 종료 코드 **1**이 수집됐다. 이 좌표 준비 오류는 구체적인 복구 방법을 확인했다. 다만 이것이 게임 메인 창 활성화 문제까지 해결하지는 않았다.

## 4. 새 로그 증거와 원인 구분

### B: UIAutomationCore 신규 모듈 탐지

증거: [normal_20261107_103127/events.jsonl](../.hk01_validation/gui_revalidation/normal_20261107_103127/events.jsonl), 같은 폴더 GameClient.log.

```text
01:31:28.211Z Baseline initialized (52 DLLs)
01:31:28.225Z Baseline created after first render
01:32:00.138Z Allowed verified system DLL: ...\oleacc.dll
01:32:00.147Z New DLL detected: c:\windows\system32\uiautomationcore.dll
01:33:17.896Z GAME_EXIT code=1
```

`C:\Windows\System32\UIAutomationCore.dll`의 실제 검증:

- Authenticode `Valid`, 서명자 Microsoft Windows.
- SHA-256 `6889FCAE6BAE830366A16DD1BD710C6D0C125AFE88A847E51C5DD6D0BD68EA23`.
- 근거: [uiautomationcore-verification.json](../.hk01_validation/gui_revalidation/uiautomationcore-verification.json).

**확인된 사실:** DLL 주입 없이 정상 서명 시스템 DLL을 DLLGuard가 탐지했고, 보안 경고 확인 후 코드 1로 종료했다. 현재 IsVerifiedSystemDll의 5개 예외에 UIAutomationCore는 없다. 따라서 이 실행의 경고·종료 원인은 CRC 불일치가 아니라 신규 모듈 탐지 경로다.

**분석에 따른 추정:** Windows 접근성 조회와 시간적으로 연결된 UIAutomationCore 로드가 도구 개입과 방어 오탐을 이어 주는 유력 원인이다. 실제 로더 호출 스택이나 모듈 로드 이벤트 추적은 수집하지 않았으므로 어떤 API 호출이 DLL을 로드했는지까지 확정하지 않는다.

### C: wintypes 신규 모듈 탐지

증거: [normal_20261107_103405/events.jsonl](../.hk01_validation/gui_revalidation/normal_20261107_103405/events.jsonl), 같은 폴더 GameClient.log.

```text
01:34:05.876Z Baseline created after first render
01:34:06.833Z Allowed verified system DLL: ...\threadpoolwinrt.dll
01:34:19.986Z Allowed verified system DLL: ...\coreuicomponents.dll
01:34:19.997Z Allowed verified system DLL: ...\coremessaging.dll
01:34:20.054Z New DLL detected: c:\windows\system32\wintypes.dll
01:37:19.469Z GAME_EXIT code=1
```

화면 전용 조회로 바꾸는 것만으로는 이번 환경의 오탐·복원 장애가 해소되지 않았다. wintypes는 직전 공격·방어 검증에서도 Valid Microsoft Windows 서명과 해시가 확인된 파일이며, 이번 보고서에서는 그 기존 검증과 새 실행의 탐지 로그를 구분한다. C 실행의 로드 원인을 접근성 트리 조회라고 단정하지 않는다.

두 새 GameClient.log는 각각 WINDOW_CREATED / DX11_READY / HUD_READY(smoke=0)의 3행뿐이다. MATCH·전투·PLAYER 입력 결과는 없다. W 루트 GameClient.log는 이번 시작 전 10:14:02에 갱신된 과거 기록이므로 이번 플레이 증거로 사용하지 않았다. A의 실행 CWD와 로그 위치는 확정하지 못했고, B/C는 독립 폴더로 해결했다.

### 최소화 오류와 입력 충돌의 범위

확인된 사실:

- 동일 세션 2에서 게임 창과 모달이 열거됐다. 다른 Windows 세션에서 실행됐다는 가설은 관측과 맞지 않는다.
- 일반 권한 프로세스 조회와 권한 있는 조회의 창 핸들 반환에 차이가 있었다. 이것만으로 게임과 Computer Use 사이의 무결성 수준 차이나 입력 실패 원인을 확정할 수 없다.
- Computer Use가 최소화 상태를 보고했고, 새 객체·연결 초기화·Normal 실행 후에도 반복됐다.
- `user input was detected`는 도구가 반환한 입력 보호 오류다. 실제 사용자가 키를 눌렀다고 입증된 것은 아니다.
- 게임 메인 창은 정확한 전경·키보드 포커스 확보를 입증하지 못했다. 잘못된 화면 캡처도 관측됐다.
- 소유 보안 모달이 실제 존재했고 게임 종료까지 이어졌다. 모달 존재는 일부 시점의 입력 차단을 설명하지만, 모든 최소화 오류의 원인이라고 단정하지 않는다.

**미확인:** native IsIconic/IsWindowVisible 값, 실제 전경 HWND, GetGUIThreadInfo의 키보드 포커스 HWND, 게임·실행기 토큰 무결성 수준. 현재 공개 sky API가 해당 원시 진단 필드를 제공하지 않아 수치로 기록하지 못했다. 화면 캡처 불일치의 내부 원인도 확정되지 않았다.

## 5. 성공 기준별 최종 판정

| 요구 항목 | 이번 판정 | 근거·한계 |
|---|---|---|
| 정확한 EXE 실행 | 성공 | 전체 경로·해시·PID·세션·창 생성 확인 |
| 창 생성 | 성공 | HWND·제목 열거, B/C 초기화 로그 |
| 실제 게임 화면 표시·활성화 | 부분 확인 / 미검증 | Menu 접근성은 확인. 신뢰할 게임 화면 캡처와 전경 포커스는 확보하지 못함 |
| 최소화 복원 | 해결 못 함 | 표준 활성화·새 상태·연결 초기화·시스템 메뉴·Normal 실행 비교 후에도 오류 |
| Enter로 게임 시작 | 미검증 | B에서 API 반환, 실제 시작 로그·Playing 상태 없음 |
| WASD 이동 | 미검증·미실시 | 시작·포커스 확보 실패 상태에서 입력을 보내지 않음 |
| 기본 공격 | 미검증·미실시 | 같은 이유 |
| 궁극기 | 미검증·미실시 | 같은 이유. 런타임 CRC 호출 성공도 이번 미검증 |
| 정상 게임 진행 | 미검증 | 초기화·프로세스 생존을 플레이 성공으로 보지 않음 |
| 정상적인 플레이 종료 | 미검증 | B/C는 보안 경고 확인 후 ExitProcess(1). A는 외부 정리 |
| 보안 경고의 GUI 확인·종료 | 성공 | 두 실행에서 실제 모달 문구·확인 클릭·종료 코드 1 |

## 6. 보존·제한과 후속 작업

새로 만든 것은 E 아래 GUI 전용 수집 스크립트·로그·진단 JSON과 본 문서다. 원본 EXE 및 IntegrityGuard.cpp / Application.cpp / GameWorld.cpp를 수정하지 않았다. [preserved-files.json](../.hk01_validation/gui_revalidation/preserved-files.json)의 해시는 기존 값과 같다. DLLGuard·CRC·SpeedGuard 예외 추가·비활성화·빌드·주입·후킹은 없다. 모든 이번 테스트 프로세스는 정리했다.

상위 `gui_input_check.ps1`은 과거 직접 Win32 입력 방식이라 현재 도구와 다른 경로다. 읽기는 했지만 실행하지 않았다. 적용한 [computer-use SKILL.md](<C:/Users/정명훈/.codex/plugins/cache/openai-bundled/computer-use/26.1029.61003/skills/computer-use/SKILL.md>)가 참조하는 [guidance.md](<C:/Users/정명훈/.codex/plugins/cache/openai-bundled/computer-use/26.1029.61003/docs/guidance.md>)의 명시 규칙은 “Do not mix direct PowerShell UI Automation code in the same turn as Computer Use.”다. 따라서 직접 PowerShell UI 자동화로 도구 제한을 우회하지 않았다. 이 제한과 별개로, 실제 도구 재현·표준 복원·다른 실행 경로·GUI 모달 대응·로그 분석은 수행했다.

다음 우선순위:

1. **Computer Use 도구 문제 분리:** 선택 창과 다른 화면이 캡처되는 문제, 최소화/입력 충돌 순환을 이번 app/id·세션·오류 기록으로 점검한다. 정확한 원시 전경·포커스 진단이 가능한 공식 도구 지원이 있으면 그 값으로 보완한다.
2. **방어 오탐은 별도 변경 과제로:** 정상 시스템 DLL의 지연 로드가 경고를 일으키는 현상을 현재 실행 증거로 재현했다. UIAutomationCore·wintypes를 예외 처리할지 또는 초기화 정책을 개선할지는 별도 방어 설계 변경으로 다룬다. 이번 “방어 수정 금지” 조건에서는 적용하지 않았다.
3. **플레이 재시험:** 도구의 정상 화면·입력 경로를 확보하고 무주입 기준선이 경고 없이 유지될 때 Enter → Playing/MATCH 확인 → WASD 위치 변화 → LMB/K 발사 → J 및 CRC 로그 → 창 닫기·종료 코드 0을 확인한다. 사용자의 직접 플레이와 Codex 자동화 결과를 계속 별도로 기록한다.

이번에 구체적으로 해결한 것은 **경고창 클릭의 geometry 오류 복구 및 경고 후 종료 코드 수집**이다. 새로 좁힌 장애는 **Computer Use의 화면·활성화 오류와, 정상 Windows DLL 지연 로드에 대한 DLLGuard 오탐**이다. 게임이 실행되지 않는다는 결론이나 실제 플레이 성공이라는 결론은 내리지 않는다.

## 7. 추가 요청: Release 구조 대조 후 API 폴더 자체에서 실행

2026-11-07 10:42 KST에 두 EXE의 SHA-256이 다시 동일함을 확인했다. src/main.cpp → Application::Run → Win32 창 → DX11 → HUD → ShowWindow → 입력/렌더 루프 구조다. 확인한 Renderer 초기화의 셰이더는 소스에 내장돼 있으며 Release 폴더의 별도 셰이더 파일을 읽지 않는다. Logger는 CWD의 GameClient.log에 append한다. Release 폴더에 있는 옛 payload·인젝터·PDB는 이 초기화 경로에서 직접 로드하는 대상으로 확인되지 않았다.

이번에는 **W\GameClient.exe를 W 자체를 작업 폴더로 지정해 Normal 창으로 실행**했다. EXE·소스·방어는 변경하지 않았다.

```powershell
$exe = (Get-Item -LiteralPath '.\GameClient.exe').FullName
Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -WindowStyle Normal -PassThru
```

PID=27284, 세션=2, 실제 MainWindowHandle=9046974, 제목=EASILE GameClient, WaitForInputIdle=true. 프로세스는 실행 상태로 남겨 두었다. 이 요청에서는 접근성 조회로 인한 방어 오탐 개입을 추가하지 않도록 게임의 접근성 트리·자동 입력을 호출하지 않았다. 따라서 실행·창 생성·초기화와 실제 화면 표시·플레이 검증은 구분한다.

실행 직전 GameClient.log 길이 286943바이트를 기록해, 그 이후 추가분만 [api-folder-launch.log](../.hk01_validation/gui_revalidation/api-folder-launch.log)에 복사했다. 실행 경로·해시·CWD·PID·창 핸들 기록은 [api-folder-launch.json](../.hk01_validation/gui_revalidation/api-folder-launch.json)에 있다. 이 추가 실행은 앞 절의 프로세스 정리 이후 새로 시작한 것이며 자동 종료하지 않았다.

## 8. 추가 비교: build/Release의 EXE를 직접 실행

사용자 지시에 따라 이번에는 **P\build\Release\GameClient.exe 자체**를 그 Release 폴더를 CWD로 지정하고 Normal 창으로 직접 실행했다. PID=36588, 세션=2, MainWindowHandle=7867562, WaitForInputIdle=true. [release-folder-launch.json](../.hk01_validation/gui_revalidation/release-folder-launch.json)에 기록했다.

Computer Use에서 정확히 build/Release 경로의 반환 창을 선택했다. 접근성 트리는 요청하지 않고 화면 캡처를 사용했다. 첫 캡처에서는 게임 메뉴가 다른 창 뒤에 보였다. Enter 키 API가 반환된 뒤 재캡처에서 **실제 게임 메뉴 및 `EASILE 보안 경고` / `게임 파일이 이상합니다.` / 확인 버튼**이 명확히 표시됐다. 이번에는 이전과 달리 실제 게임 화면 캡처를 확보했다.

새 Release GameClient.log는 WINDOW_CREATED·DX11_READY·HUD_READY(smoke=0)만 남겼고 이번 실행의 MATCH 시작은 확인되지 않았다. 따라서 Release 경로에서도 프로그램·게임 화면은 표시됐지만 Computer Use 개입 상태에서 경고가 떠 플레이 검증으로 진행하지 못했다. 이번 실행의 탐지 DLL 디버그 출력은 수집하지 않았으므로 경고 원인 모듈을 특정하지 않는다. 경고창은 사용자가 볼 수 있도록 닫지 않고 남겨 두었다. EXE·방어 코드는 변경하지 않았다.

## 9. 후속 재시도: 실제 Playing 화면 확인

사용자가 입력을 멈출 준비가 됐다고 알린 뒤, 새 Computer Use 인터페이스(cua_repl)로 Release EXE를 직접 실행했다. 반환된 정확한 build/Release 경로의 메인 창 id는 11209954였다. Menu 접근성 상태와 실제 게임 화면을 확인한 뒤 Enter, 화면 클릭 후 Enter를 시도했다. API 반환 직후 접근성 텍스트는 Menu로 남아 있었다.

이후 `getAXStateAndScreenshot()`의 **실제 게임 화면에서 `PLAYER / Playing` 표시, 메뉴 오버레이 제거, 이동 중인 전투 개체와 경기 화면**을 확인했다. 같은 호출의 접근성 텍스트는 Menu로 남아 있어 접근성 상태와 실제 렌더 화면의 불일치도 확인했다. 따라서 이 실행의 게임 시작·실제 Playing 화면은 확인됐으며, 앞 절의 모든 실행이 끝내 Menu였다는 식으로 일반화하면 안 된다.

다만 시작 전환이 어느 입력에 의해 발생했는지, 사용자 입력이 별도로 있었는지는 독립적으로 입증하지 못했다. 이후 이동·기본 공격·궁극기 입력은 `user input was detected in this window` 및 `window is minimized` 오류로 완료되지 않았다. **실제 Playing 관측과 Codex가 이동·공격을 성공적으로 제어했다는 판정은 구분한다.** 이번 후속 작업에서도 EXE·방어 코드 수정이나 DLL 주입은 하지 않았다.
