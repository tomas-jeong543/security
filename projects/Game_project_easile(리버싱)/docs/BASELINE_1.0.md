# EASILE Baseline 1.0 확정 기록

- **Baseline Status: CONFIRMED**
- **Baseline Version: 1.0** (현재 v0.14 게임 구현)
- **확정일: 2026-09-30**
- **범위: 현재 작업 트리의 게임 구현을 후속 Patch / DLL Injection / Hooking 실험의 기준으로 지정**

## 승인과 검증

사용자는 게임을 직접 실행하여 캐릭터 이동, 전투, 스킬 사용 및 전체적인 플레이 흐름을 확인했고, 현재 구현 상태를 이후 보안 실험의 기준으로 사용하기에 충분하다고 승인했다. 이는 사용자 직접 플레이 확인이다. 특정 빌드 구성별 수동 플레이 결과나 세부 조작·밸런스 수치를 추가로 추정하지 않는다.

| 항목 | Baseline 확정 시 상태 | 근거 |
|---|---|---|
| Debug x64 빌드 | PASS | 2026-09-30 `EASILE.sln` MSBuild 실행 |
| Release x64 빌드 | PASS | 2026-09-30 `EASILE.sln` MSBuild 실행 |
| Debug 자동 테스트 | 49 PASS, 0 FAIL, 종료 코드 0 | 2026-09-30 `build/Debug/GameTests.exe` 실행 |
| Release 자동 테스트 | 49 PASS, 0 FAIL, 종료 코드 0 | 2026-09-30 `build/Release/GameTests.exe` 실행 |
| DX11/HUD 및 자동 GUI 검증 | 이전 검증에서 PASS/실행 완료 | [v0.14 검증 기록](../evidence/v014-validation.md). 이번 확정 작업에서는 재실행하지 않음 |
| 사용자 직접 플레이 | 확인 및 승인 | 사용자의 [확정 지시](EASILE_Baseline_1.0_Confirm_Request.md). 자동 GUI 검증과 별도 |

현재 빌드의 GameClient SHA-256은 Debug `B853C0CBE546F0FAAED2064DC4E0D648F0050167F7F550B3B587689FF26C0808`, Release `4AC67F48BE58140159375027C69BCF65A22696E1C62CDD04CDEE38BD5AEC5DC9`이다. 기존 [해시 기록](../evidence/v014-validation.md#빌드-식별과-재현)과 일치한다.

이 확정은 **게임 상태에 대한 사용자 승인과 문서상의 공식 지정**이다. 현재 작업 트리에는 기존의 커밋되지 않은 변경이 있으며, 이 작업에서 커밋·태그·스냅샷을 만들지 않았다. 후속 실험의 정확한 재현을 위해 이 상태의 소스와 위 실행 파일을 별도로 보존하고, 실험 결과에 빌드 구성과 해시를 함께 기록한다.

## 다음 보안 실험

첫 실험 대상은 **Ultimate Cooldown Patch: 10 seconds → 1 second**다. 현재 Baseline의 Ultimate 쿨다운은 10초이며, 이번 확정 작업에서 쿨다운 변경이나 실제 EXE 패치는 수행하지 않았다.
