# Penguin Rift 리버싱 실습

먼저 `PenguinRift.exe`로 직접 분석하고, 막힐 때만 `PenguinRift_Training.map` 또는 PDB를 확인하는 방식을 권장합니다.

## 1단계: 정적 분석

1. PE가 x64인지 확인합니다.
2. import table에서 SDL2 함수가 직접 보이지 않는 이유를 찾습니다.
3. `SDL2.dll`, `SDL_Init`, `SDL_CreateWindow` 문자열의 XREF를 따라 동적 API 테이블을 복원합니다.
4. export table에서 `Challenge_*` 함수와 `g_*` 전역 변수를 찾습니다.
5. `PATCH_ME_LEVEL_GATE`, `SCORE_COMPARE_7777`, `F6_CHECKS_ICE_KEY` 문자열을 기준으로 관련 코드를 찾습니다.

## 2단계: 동적 분석

1. `g_ReversingScore`에 하드웨어 브레이크포인트를 걸고 점수 증가 지점을 찾습니다.
2. 적을 맞혔을 때 동결 시간이 저장되는 위치를 찾습니다.
3. `g_DebugView`를 1로 바꾸어 F10을 누르지 않고도 히트박스를 표시합니다.
4. `g_LevelGate`를 2로 바꾸어 첫 스테이지 뒤에 일반 엔딩으로 진입합니다.
5. `IsDebuggerPresent` 반환값이 어디에 저장되는지 추적합니다. 이 값은 실행을 차단하지 않습니다.

## 3단계: 패치 과제

1. 목숨 감소 명령을 NOP 처리합니다.
2. 눈덩이 재사용 대기시간을 제거합니다.
3. 첫 번째 눈덩이 한 발로 적이 즉시 제거되게 분기를 바꿉니다.
4. 점수가 7,777점 이상인지 확인하는 비교를 우회합니다.
5. `Challenge_CheckUnlock`이 항상 1을 반환하게 패치합니다.

## 4단계: 알고리즘 복원

`Challenge_TransformKey`는 다음 단계를 사용합니다.

- 입력과 상수 XOR
- 32비트 왼쪽 회전
- 상수 덧셈
- 다시 XOR
- 두 번째 회전

목표값 `0x6A15FDDC`를 만족하는 입력을 역산한 뒤 `g_IceKey`에 기록하고 F6을 누르십시오. 정답은 이 문서에 적지 않았습니다.

## 분석 파일 선택

- `PenguinRift.exe`: 최적화가 적용되어 실제 리버싱에 가까움
- `PenguinRift_Training.exe`: O0라서 함수 구조가 더 직접적임
- `PenguinRift_Training.pdb`: IDA 또는 Visual Studio에서 심볼 로드 가능
- `PenguinRift_Training.map`: 심볼과 RVA를 텍스트로 빠르게 확인 가능
