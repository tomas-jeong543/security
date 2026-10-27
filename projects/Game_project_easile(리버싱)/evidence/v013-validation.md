# EASILE v0.13 수정 및 검증 기록

검증일: 2026-09-28. 기준: [docs의 수정 지시](../docs/EASILE_3라인_분기외곽_라인간공격차단_Codex_지시.md).

## 수정 내용과 원인

- 기존 도로는 사각형과 별도 접합 삼각형으로 그려 이동 영역의 둥근 선분 끝과 외곽이 달랐다. 공유 경로의 모든 선분과 둥근 끝을 외곽 전체→내부 전체 순서로 그리도록 수정했다. Base에서 화면 끝으로 이어지던 연장 선분을 제거했다. 이동은 같은 경로의 반경 70px 캡슐 안으로 몸체 반경을 고려해 제한한다. 렌더의 원은 24면 근사로 반경 70px에서 최대 차이가 약 0.6px다.
- 라인 사이에는 공격을 차단하는 물리 벽이 없었다. `MapPaths::walls`의 두 직사각형(x=300..980, y=250..290 및 430..470)을 렌더·몸체 이동·투사체·Auto/HUD·Bot·Tower·Minion이 공유한다. 벽은 분기·합류 전에 끝난다.
- Basic/Ultimate는 이전→새 위치 선분에서 가장 먼저 만나는 벽 또는 적 유닛/구조물을 처리한다. 고속 탄환도 벽을 넘지 못한다. 궁극기는 단일 투사체이고 광역 피해는 없다. 수풀 은폐와 벽 차단은 별도로 검사한다.
- 저체력 Bot의 방어점은 같은 라인 Red Tower+(30,0), 왕복은 Tower.x+12..40이다. 짧은 정지 후 방향을 바꾸며 유효한 같은 라인 Blue Minion을 우선 공격한다. 55% 회복 시 Push, Recover 18초 시 임시 Push와 재후퇴 제한 3초를 적용한다. 사망·부활·Restart에서 임시 상태를 초기화한다.
- 라인 변경 중 Bot이 도로 밖 방향으로 벽 끝에 멈추는 문제를 발견해 현재 라인 Tower→오른쪽 합류→목표 라인 Tower 경유 단계를 유지하도록 수정했다.
- 기존 피해량, 공격 주기, 궁극기 쿨다운 및 승패 규칙은 유지했다.

## 빌드와 자동 검사

| 항목 | 결과 | 기록 |
|---|---|---|
| Debug x64 빌드 | 성공, 종료 코드 0 | [빌드 로그](v013-build-debug.txt) |
| Release x64 빌드 | 성공, 종료 코드 0 | [빌드 로그](v013-build-release.txt) |
| Debug 전체 검사 | 38개 PASS | [결과](v013-tests-debug.txt) |
| Release 전체 검사 | 38개 PASS | [결과](v013-tests-release.txt) |
| 실제 Win32/DX11 시나리오 | 명중·회복·전진 PASS | [결과](v013-visual-results.txt), [피해 로그](v013-visual-damage.log) |
| GameClient 실제 입력 | Enter/LMB/K/K hold/J/Tab/K 및 창 크기 변경 성공 | [결과](v013-gui-input.txt), [입력 로그](v013-gui-input.log) |

검사 수는 출력 PASS 항목 기준이며 내부 반복 조합을 별도로 더하지 않았다. 기존 G01~G17을 모두 실행했다.

| 요구 동작 | 검사 |
|---|---|
| Player/Bot/Minion 도로 진입·복귀 및 Base 접근 | Road bounds, Road junction round trip, Connected lane sieges x6 |
| 양 진영 상↔중/중↔하 Basic/Ultimate 고속 차단 | Lane walls: swept Basic/Ultimate x16; 같은 라인·벽 끝 공격 및 벽 앞 유닛 우선 명중 포함 |
| Auto/HUD/LMB/Mouse/Bot 벽 너머 표적 제외 | Wall Auto/HUD/LMB/Mouse; G14~G17; 수풀 가시성 회귀 |
| Tower/Minion 양 진영 벽 차단·같은 라인 피해 | Wall Tower/Minion rules; Tower attack visuals; Minion target priority |
| 저체력 Bot 세 라인 왕복·명중·무표적 무사격 | Bot defensive patrol x3; Bot cover fire; 실제 GUI 시나리오 |
| Bot 여섯 라인 변경 조합 | Bot wall detour x6; 1400틱 동안 도로 내부 및 목표 라인 도달 검사 |
| 55% 복귀·18초 제한·사망·부활·Restart | Bot defensive patrol x3; G13; Bot FSM 3-cycle; Flow/Restart x10 |
| 회복 오브젝트·공성·공격 주기·승패 | Center pickup; Bot route/pickup limit; Bot attack cadence; Full match flow |

## 수정 전후 화면

`docs/actual_image.png`와 `docs/easile_v012_road_plan.png`를 직접 열어 비교했다. 같은 이름의 중복 이미지는 없었다. 변경 전 자료는 저장소에 있던 화면이며 이번 작업 중 과거 코드를 재실행한 캡처는 아니다.

| 장면 | 캡처 |
|---|---|
| 변경 전 저장소 참고 화면 | [변경 전](v013-before-reference.png) |
| 이전 작업에서 남긴 도로 변경 전·후 | [전](road-before.bmp), [후](road-after.bmp) |
| 변경 후 게임 시작 직후 | [실제 창과 HUD](v013-gui-input-start.png), [DX11 원본](v013-visual-start.png) |
| 변경 후 전투 3.85초 | [전투](v013-visual-combat.png) |
| 실제 Mouse 모드·크기 변경 | [Mouse](v013-gui-input-mouse.png), [876×541 클라이언트](v013-gui-input-resized.png) |
| 저체력 Bot, 미니언 없음 | [무표적](v013-visual-defense-empty.png) |
| 같은 라인 미니언 등장·사격 | [등장](v013-visual-defense-before.png), [탄환](v013-visual-defense-shot.png) |
| 미니언 피해 후 | [명중](v013-visual-defense-hit.png) |
| Bot 회복·전선 복귀 | [Push 전환](v013-visual-defense-push.png), [전진](v013-visual-defense-advance.png) |

DX11 backbuffer 캡처에는 GDI HUD가 포함되지 않는다. 실제 GameClient 창 캡처에는 HUD가 포함된다. 시작 창은 최대화 상태이며 작업 표시줄이 함께 찍혔다. 전체 도로 비교에는 HUD 없는 1280×720 원본을 사용한다. 변경 전 시작 화면을 동일 시간·위치로 새로 촬영하지는 못했으므로 엄밀히 일치하는 시작 화면 전후 비교는 남아 있다.

## 저체력 Bot의 실제 명중 근거

Win32 창과 DX11 Renderer/HUD를 초기화하고 `GameWorld.Update`를 0.01초 간격으로 실행했다. Bot은 (1050,360), HP25, Recover에서 시작한다. 미니언 없는 0.5초 무사격 후 Blue Minion ID100을 (920,360), ID101을 (940,180)에 HP40으로 배치한다. Bot 단독 피해를 분리하기 위해 Blue Tower를 제거하고 살아 있는 Red Tower의 공격 대기 시간을 1000초로 설정했다. 이는 검증 시나리오 설정이며 일반 게임의 Tower 규칙은 변경하지 않았다.

```text
simulation=0.510 DEFENSE_SHOT target=BlueMinion
simulation=0.600 MINION_HIT id=100;attacker=Red;hp=40->35
simulation=1.610 MINION_HIT id=100;attacker=Red;hp=35->30
simulation=2.620 MINION_HIT id=100;attacker=Red;hp=30->25
simulation=3.600 MINION_HIT id=100;attacker=Red;hp=25->20
simulation=7.500 RECOVER_TO_ADVANCE hp_ratio>=55%
```

4초 사격 후 같은 라인 HP20, 다른 라인 HP40, Bot HP43을 검사했다. 회복 이후 Push 및 HP55 이상을 확인하고 추가 0.5초 동안 x좌표가 10px 이상 감소해 실제 전진한 것도 검사했다. 방향 전환 로그는 x=1032/1060 왕복을 보여준다.

GUI 입력은 운영체제 키보드·마우스 이벤트로 실제 GameClient에 전달했다. 샌드박스에서는 전경 포커스, Windows SDK 접근 및 C++ 파일 로그 쓰기가 제한되어 권한을 승인받아 재실행했다. 최종 빌드·입력·피해 기록은 성공한 실행 결과다.

## 문서 반영 및 검증 범위

- [설계서 v0.13](../docs/EASILE_2D게임_및_보안반복실험_설계서_v0.13.docx): v0.12를 바탕으로 새 15장과 최신 맵 그림을 반영했다. 이전 장과 상충하면 15장을 적용한다. 기존 버전 파일은 이 작업에서 수정하지 않았다.
- [최신 맵 그림](../docs/easile_v013_wall_plan.png)과 [README](../README.md)에 벽·연결 경로·저체력 Bot 규칙을 반영했다.
- Python/LibreOffice 렌더 도구가 없어 설치된 Microsoft Word로 31쪽 전체를 PNG로 렌더하고 열어 확인했다. 새 15장·그림의 잘림이나 겹침은 없었다. 재생성 도구는 `tools/create_design_doc_v13.ps1`, 렌더 도구는 `tools/render_design_doc_word.ps1`이다.
- 검증은 자동 테스트와 실제 GUI의 자동 입력·렌더 검증이다. 사람이 장시간 직접 플레이한 조작감·밸런스 평가를 완료한 것으로 간주하지 않는다. 새 baseline 태그는 만들지 않았다.
