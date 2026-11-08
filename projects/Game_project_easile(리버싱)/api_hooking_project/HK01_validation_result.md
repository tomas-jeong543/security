# HK-01 검증 및 DLLGuard 오탐 수정 결과

최종 정리: 2026-11-07. 실행 증거는 2026-11-06 검증에서 수집했다.

**판정: 정상 스모크 실행·CRC 검사 통과, DLL 로드·후킹 설치 성공, DLLGuard 탐지 및 경고 확인 후 종료 성공. 후킹 함수 호출과 공격 방향 30도 변경은 미검증.**

| 단계 | 수행 내용 | 실행 결과·증거 |
|---|---|---|
| 1 | 경고 원인 및 Windows DLL 검증 | **확인.** DLL을 주입하지 않은 실행에서 `[DLLGuard] New DLL detected: c:\windows\system32\oleacc.dll`과 GUI의 `게임 파일이 이상합니다.`를 확인했다. 이후 clbcatq.dll, threadpoolwinrt.dll, CoreUIComponents.dll, CoreMessaging.dll도 정상 실행 중 순차 탐지됐다. 이 다섯 파일 모두 정확한 System32 경로에서 Microsoft Windows 서명이 `Valid`임을 확인했다. |
| 2 | 제한적 오탐 수정 및 CRC 대조 | **수정·확인.** `../src/IntegrityGuard.cpp`에서 위 다섯 DLL만 System32 전체 경로와 검증된 SHA-256이 모두 일치할 때 예외 처리한다. CRC32 및 나머지 신규 DLL 탐지·경고·종료 기능은 유지했다. 원본 Update CRC는 `7AD446CE`로 원래 기대값과 일치했다. 수정 빌드 Update CRC는 `29148A8C`로 새 기대값과 일치하며 Initialize CRC `A8E8248A`도 일치한다. |
| 3 | 수정본 정상 실행 및 GUI 확인 | **범위 내 성공.** 무주입 실행에서 15초간 DLLGuard 경고 없이 유지됐고 메뉴 화면을 확인했다. 별도 `--smoke-gui` 실행은 `[Integrity] Update CRC32 = 29148A8C`, 전투·투사체 로그, `FRAMES_RENDERED count=5`, 정상 종료 코드 `0`을 남겼다. 장시간 수동 플레이는 검증하지 않았다. 마지막 GUI 확인은 최소화된 창의 복원 실패로 미완료다. |
| 4 | 기존 injector 및 payload로 HK-01 재실행 | **DLL 로드·후킹 설치 성공.** PID `11368`에 주입한 injector 종료 코드 `0`, 대상 모듈 목록의 `hook_payload.dll`, 디버그 출력 `[HK-01] Hook installed.`를 확인했다. 후킹 대상 RVA `0x137E0`과 진입 바이트도 수정 빌드에서 유지됐다. DLLGuard는 같은 실행에서 payload 전체 경로를 신규 DLL로 탐지했다. GUI 보안 경고를 확인했고, 확인 버튼을 누른 뒤 게임 창 및 프로세스가 사라진 것을 관찰했다. |
| 5 | 후킹 호출·30도 변경 및 최종 판정 | **미검증.** 해당 공격 실행은 메뉴 상태였고 전투·detour 호출 기록 및 입력/출력 방향 비교가 없다. 설치 메시지만으로 후킹 함수 호출이나 30도 변경을 성공으로 판단하지 않는다. 공격의 최종 효과는 입증되지 않았으며, DLLGuard의 탐지·경고·종료는 확인됐다. |

CRC 불일치와 DLLGuard 경고는 별도 경로다. `GameWorld.cpp`의 CRC 실패 분기는 `INTEGRITY_FAIL`을 기록하고 반환한다. 이번에 관찰한 경고 문구는 `IntegrityGuard.cpp`의 신규 DLL 탐지 분기에서 출력된다. 중간에 실행한 GameTests가 루트 GameClient.log에 테스트 기록과 INTEGRITY_FAIL을 추가했으므로, 이 기록은 실제 GameClient의 공격 성공·실패 근거에서 제외했다. 전체 GameTests는 완료되지 않아 통과로 보고하지 않는다.

실행 증거:

- 무주입 오탐: `C:\TEMPFO~1\HK01_noinject_20261106_212534\`
- 수정 후 무주입 15초: `C:\TEMPFO~1\HK01_fixed_normal_20261106_214426\`
- 정상 스모크·CRC: `C:\TEMPFO~1\HK01_fixed_normal_20261106_214615\`
- DLL 주입·설치·탐지: `C:\TEMPFO~1\HK01_fixed_inject_20261106_214615\`

실험 폴더의 GameClient.exe에 수정 빌드를 반영했다. SHA-256은 `32A1D0D4D8FBF15E2A1E45467475A6057F26EEE54B1AF5746945DE24C768A926`이며 `../build/Release/GameClient.exe`와 동일하다. 이전 실행 파일은 `.hk01_validation/GameClient.before_dllguard_fix.exe`에 보관했다. injector.exe와 hook_payload.dll은 변경하지 않았다.

예외는 현재 PC에서 검증한 파일 해시에 고정되어 있다. Windows 업데이트로 해당 파일이 바뀌면 서명·해시 재검증이 필요하며, 자동으로 허용하지 않는다.
