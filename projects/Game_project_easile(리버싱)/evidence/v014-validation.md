# EASILE v0.14 검증 기록

2026-09-29, Windows x64, Visual Studio 2022 / MSVC v143, DirectX 11.
기준: `docs/EASILE_baseline_candidate_map_jungle_bots_update.md`.

**Baseline Status: CONFIRMED · Baseline Version: 1.0 (2026-09-30).** 아래 2026-09-29 자동 검증 이후 사용자가 게임을 직접 실행해 이동, 전투, 스킬 사용과 전체 플레이 흐름을 확인했고, 이 구현을 이후 보안 실험의 기준으로 승인했다. 자동 GUI 입력은 사용자 직접 플레이와 별개의 근거다. [확정 기록](../docs/BASELINE_1.0.md)에 현재 판정을 정리했다.

## Modified Files

- `src/MathTypes.h`, `src/MapPaths.h`: 월드 1920×1440, Lane 폭 280, 간격 360, 정글 통로·방·벽, 공유 충돌·시야·길찾기.
- `src/Camera.h`, `src/Application.cpp`, `src/Renderer.h`, `src/Renderer.cpp`, `src/Snapshot.h`: Player 추적 카메라, 월드/화면 좌표, 마우스 조준과 고정 HUD.
- `src/BotHero.h`, `src/BotController.h`, `src/GameWorld.h`, `src/GameWorld.cpp`: 네 Hero, 세 Bot의 독립 FSM·타이머·표적, 팀별 이동·회복·공격, 다중 Hero 충돌·부활·Tower 표적.
- `src/Projectile.h`, `src/Hero.h`, `src/VisibilitySystem.h`: 발사자 ID/원점, 개별 XP, Ultimate 10초, 다중 Hero 시야.
- `tests/GameTests.cpp`: 기존 회귀 fixture의 확대 좌표 적용, G18~G28 추가.
- `tools/gui_input_check.ps1`, `tools/validate_v014.ps1`: 실제 창 입력 및 빌드·시험·EXE 해시 수집.
- `README.md`, `docs/EASILE_design_v0.14.md`, `docs/EASILE_2D게임_및_보안반복실험_설계서_v0.14.docx`, `docs/easile_v014_map.png`, `tools/create_design_doc_v14_word.ps1`: 현 구현 설계·조작·검증 상태와 지도.
- 본 기록, `evidence/v014/*`, `evidence/doc-v014-final/page-01.png`~`page-34.png`: 로그와 문서 검토 증거.

작업 전부터 존재하던 변경과 이전 설계서/검증 자료는 보존했다. 위 목록은 이번 작업에서 직접 수정·생성한 기능과 자료다.

## Build Result

| 구성 | 결과 | 근거 |
|---|---|---|
| Debug x64 | PASS | [빌드 로그](v014/Debug-build.txt) |
| Release x64 | PASS | [빌드 로그](v014/Release-build.txt) |

두 구성에서 GameClient.exe와 GameTests.exe가 빌드됐다. `git diff --check -- src tests README.md` 통과.

## Test Result

| 항목 | Debug | Release | 근거 |
|---|---|---|---|
| 기존 회귀 38개 | PASS | PASS | [Debug](v014/Debug-tests.txt), [Release](v014/Release-tests.txt) |
| 신규 G18~G28 11개 | PASS | PASS | 같은 로그, 구성별 총 49개 PASS |
| 실제 규칙의 Full Match 3회 | PASS | PASS | G28: 매회 Victory, 18.51초 |
| 실제 GameClient Win32/DX11/HUD 실행 | PASS | PASS | [Debug smoke](v014/Debug-smoke.log), [Release smoke](v014/Release-smoke.log): FRAMES_RENDERED count=5, EXIT |
| 확대 맵 렌더 시나리오 5개 | PASS | PASS | [Debug](v014/Debug-visual.txt), [Release](v014/Release-visual.txt) |
| 실제 창의 자동 키보드·마우스 입력 | 실행 완료 | 실행 완료 | [Debug GUI](v014/Debug-gui.txt), [Release GUI](v014/Release-gui.txt) |
| 사용자 직접 플레이 확인 | 자동 시험과 별도 | 자동 시험과 별도 | 2026-09-30 사용자가 직접 실행해 이동·전투·스킬·전체 흐름을 확인하고 Baseline 사용을 승인함. 구성별 플레이 결과는 별도로 진술하지 않음 |

G18/G19: 2:2 팀 구성, 중복 없는 ID, 생성 충돌, Bot HP/FSM/Restart 독립성.
G20: 가장 먼저 맞는 적 Hero, friendly fire 없음, 발사한 Hero에게만 XP, 동거리 ID 표적.
G21: 실제 지형 확대, Lane 폭/간격, 몸체 이동 경계.
G22: 양 팀 Basic/Ultimate의 고속 swept cross-lane 차단.
G23: 세 Lane을 잇는 정글 이동 및 Bot 길찾기.
G24: 벽 이동/투사체와 열린 중앙 출구의 다른 Lane Auto/방향 공격 차단.
G25: 정글 수풀 은폐/reveal/공유 시야.
G26: 양 팀×세 Lane Minion 경로, Tower→Nexus 공성, 정글 진입 없음.
G27: 카메라 역변환, resize 마우스, 맵 경계.
G28: 강제 HP 감소나 Tower/Nexus 즉시 파괴 없이 입력을 재생하는 전체 경기와 템포 측정.

기존 회귀에는 G01~G17, regen/XP/respawn/draw, 공격 입력/모드/focus, Tower/Minion 우선순위, 6방향 공성, Bot 회복 왕복/엄폐 사격/벽 우회, 회복 오브젝트, 반복 Restart가 포함된다.

## 측정 결과와 한계

| 측정 | Debug / Release 동일 결과 |
|---|---|
| 월드 좌표 면적 | 이전의 정확히 3배 |
| 이동 가능 지형 면적 | 이전의 약 3.26271배, 8px raster로 도로·정글 합집합에서 벽을 제외하여 비교 |
| Lane 외곽 폭 / 중심 간격 | 280 / 360, 이전의 각각 2배 |
| Base→Top/Mid/Bottom 중앙 | 4.10 / 3.43 / 4.10초 |
| Minion만 진행할 때 최초 충돌 | 5.63초 |
| 입력 재생 경기 최초 Minion 피해 | 3.74초 |
| 최초 Hero 피해 | 2.20초 |
| Blue Minion 최초 적 Tower 사거리 도달 | 8.20초 |
| Full Match 3회 / 평균 | 18.51 / 18.51 / 18.51초, 평균 18.51초 |

G28은 동일 seed와 입력을 사용한 재현성 검사다. 10ms 이동 입력, Basic 0.1초 간격, J 요청 1초 간격을 사용한다. 이는 사람의 평균 경기 길이가 아니며 이 수치로 최종 밸런스 PASS를 판단하지 않는다. 기본 공격 5·별도 쿨다운 없음, Hero 속도 220을 유지하고, 요구 Markdown에 맞춰 J는 10초, Minion 속도는 110→130으로 조정했다.

자동 GUI는 Enter, LMB 두 번, K/길게 누르기, J, Tab, WASD, 방향키, P pause/resume, Mouse J, 창 resize와 resize 후 클릭을 전송했다. 키 입력 전 foreground를 확인한다. 이 기록은 입력 전송/실행 증거이며 각 입력의 모든 게임 효과를 자동으로 판정하는 테스트는 아니다. 충돌·카메라·조준의 수치 판정은 G21~G27과 기존 회귀가 수행한다.

실제 창 캡처는 각 `build/Debug`, `build/Release`의 `gui-input-*.png`, `v014-gui-*.png`에 있다. DX11 확대 시나리오 캡처는 `v014-start.bmp`, `v014-center-combat.bmp`, `v014-jungle.bmp`, `v014-camera-edge.bmp`, `v014-retreat.bmp`다. HUD/HP bar, 지형·벽·수풀, 카메라 경계와 조준점 화면을 검토했다.

Word 설계서는 기존 v0.13을 보존하여 v0.14로 생성했다. 표지와 최신 15장에 이전 장의 상충 규칙을 대체한다고 명시했다. native Word 렌더링으로 최종 34페이지를 생성하여 모든 페이지를 시각 검토했다. 지도와 본문이 잘리지 않고 버전/날짜/최신 규칙이 표시된다. Markdown이 최신 구현의 텍스트 기준이다.

## 빌드 식별과 재현

- Debug GameClient SHA-256: `B853C0CBE546F0FAAED2064DC4E0D648F0050167F7F550B3B587689FF26C0808`
- Release GameClient SHA-256: `4AC67F48BE58140159375027C69BCF65A22696E1C62CDD04CDEE38BD5AEC5DC9`
- 해시 원본: [Debug](v014/Debug-sha256.txt), [Release](v014/Release-sha256.txt).
- 최종 검증: `powershell -ExecutionPolicy Bypass -File tools/validate_v014.ps1 -Gui`.
- GUI 캡처만 재생: `tools/gui_input_check.ps1 -Executable build/Release/GameClient.exe -Expanded`.
- 자동 검증을 실행하려면 Windows SDK와 실제 데스크톱/DX11 장치 접근이 필요하다.

## Baseline Decision (2026-09-30)

2026-09-29 검증 작성 당시에는 사용자 직접 플레이 확인 전이었다. 이후 사용자가 직접 실행해 이동, 전투, 스킬 사용과 전체 플레이 흐름을 확인하고 현재 상태를 기준 버전으로 승인했다. 사용자가 세부 조작 항목별 결과나 Debug/Release 각각의 수동 플레이 결과를 별도로 진술한 것으로 기록하지 않는다.

A1~A4 보안 공격/방어 실험은 아직 수행하지 않았다. 이후 같은 기준 빌드의 EXE 해시, seed와 입력 절차를 고정하여 비교한다. 첫 실험은 Ultimate Cooldown 10초 → 1초 Patch로 예정한다.

**EASILE Baseline 1.0 — CONFIRMED.**
