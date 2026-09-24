# EASILE 최종 검증 체크리스트

## 목적

현재 구현된 Bot FSM 및 게임 빌드 상태를 최종 확인한다.  
새 기능 추가나 구조 변경은 하지 말고, 아래 검증만 수행한다.

---

## 1. Release GameClient 재빌드

현재 실행 중인 `GameClient.exe`가 있다면 종료한다.

그 후:

- Release x64 `GameClient` 빌드
- 빌드 성공 여부 확인
- 빌드 실패 시 실제 컴파일/링크 오류인지, 파일 점유 문제인지 구분해서 기록

---

## 2. 실제 플레이 수동 검증

실제 게임을 최소 3회 실행하여 아래 항목을 확인한다.

### Bot 회복 FSM

- Bot HP가 30% 이하가 되면 Retreat 상태로 진입하는가
- Bot이 Tower 중심과 겹치지 않고 Tower→Base 방향의 defensive point로 이동하는가
- defensive point 도착 후 Recover 상태로 들어가는가
- 안전 구역 안에서만 HP가 회복되는가
- HP가 55% 이상이 되면 다시 전투/진출 상태로 복귀하는가
- Recover 중 불필요하게 Player를 추격하지 않는가

### 예외 상황

- Tower가 파괴된 경우 Bot이 Base 방향으로 fallback하는가
- Bot 사망 후 부활했을 때 Retreat/Recover 임시 상태가 남지 않는가
- Restart 후 이전 Bot FSM 상태가 남지 않는가

---

## 3. 문서 확인

`docs/EASILE_2D게임_및_보안반복실험_설계서_v0.5.docx`를 직접 열어 확인한다.

확인 항목:

- 문서 내부 버전이 v0.6으로 반영되었는가
- 새 Bot FSM 관련 문단이 정상적으로 보이는가
- 글자 잘림, 깨진 문단, 이상한 페이지 배치가 없는가

문서 내용이 정상이라면 파일명을 다음과 같이 변경한다.

`EASILE_2D게임_및_보안반복실험_설계서_v0.6.docx`

---

## 4. 완료 보고 형식

검증 완료 후 아래 형식으로만 요약한다.

- Release x64 GameClient 빌드: PASS / FAIL
- 실제 플레이 3회: PASS / FAIL
- Bot Retreat → Recover → Re-engage: PASS / FAIL
- Tower 파괴 시 Base fallback: PASS / FAIL
- 사망/부활 상태 초기화: PASS / FAIL
- Restart 상태 초기화: PASS / FAIL
- 설계서 v0.6 확인 및 파일명 변경: PASS / FAIL

FAIL 항목이 있다면 원인과 관련 파일만 간단히 적는다.

모든 항목이 PASS일 경우 현재 game-baseline 구현을 완료 상태로 판단한다.
