# Penguin Rift

C++17과 SDL2로 만든 **오리지널 단일 화면 2D 플랫폼 액션 게임**입니다. 고전 아케이드 플랫폼 게임의 빠른 이동·점프·적 제거 방식을 참고했지만, 이름·코드·스테이지·그래픽은 새로 제작했습니다.

## 파일 위치와 작성 범위

아래 실행·빌드 안내의 작업 디렉터리는 [PenguinRift 폴더](PenguinRift_Windows_x64/PenguinRift/)입니다. `src/main.cpp`, `RUN_GAME.bat` 및 빌드 스크립트 경로는 이 폴더 기준입니다. [배포용 안내](PenguinRift_Windows_x64/배포용/README_PenguinRift.txt)는 별도 배포본 문서입니다.

기존 문서는 게임 제작과 리버싱 실습을 설명합니다. 저장소 소유자가 직접 작성한 코드와 제공받은 실습 대상의 범위는 **확인 필요**입니다. 일부 실행 파일·DLL·PDB는 Git에서 제외되어 있으므로 새로 내려받은 환경에서의 빌드·확보 절차는 TODO입니다.

## 바로 실행

1. Windows 10/11 x64에서 `RUN_GAME.bat`를 실행합니다.
2. 처음 한 번만 공식 SDL2 x64 런타임을 내려받아 `SDL2.dll`을 게임 폴더에 둡니다.
3. 이후에는 `PenguinRift.exe`를 직접 실행해도 됩니다.

인터넷을 사용하지 않으려면 별도로 받은 **64비트 SDL2.dll**을 `PenguinRift.exe`와 같은 폴더에 넣으십시오.

## 조작법

- 이동: `A`, `D` 또는 방향키 좌우
- 점프: `Space` 또는 방향키 위
- 눈덩이: `Z` 또는 `X`
- 재시작: `R`
- 종료: `Esc`
- 충돌 박스 표시: `F10`
- 숨겨진 리버싱 조건 확인: `F6`

적은 첫 번째 눈덩이에 얼고, 얼어 있는 동안 다시 맞히거나 빠르게 몸으로 밀면 제거됩니다. 모든 적을 제거하면 다음 스테이지로 이동합니다.

## 리버싱 연습 포인트

`PenguinRift.exe`는 최적화된 일반 실행 파일이고, `PenguinRift_Training.exe`는 최적화를 끈 연습용 실행 파일입니다. 연습용 PDB와 MAP도 함께 제공합니다.

확인하기 좋은 항목:

- import table에는 주로 `KERNEL32.dll`, `USER32.dll`만 존재하며 SDL2 API는 `LoadLibraryA`와 `GetProcAddress`로 동적 해석
- 문자열 XREF에서 `PATCH_ME_LEVEL_GATE`, `F6_CHECKS_ICE_KEY`, `SCORE_COMPARE_7777` 추적
- export table의 `Challenge_TransformKey`, `Challenge_CheckUnlock`, `Challenge_GrantScore`, `Challenge_GetMarker`
- 전역 변수 `g_ReversingScore`, `g_LevelGate`, `g_IceKey`, `g_DebugView`, `g_DebuggerObserved`
- `IsDebuggerPresent` 결과는 기록만 하며 게임 실행을 방해하지 않음
- XOR로 감춘 엔딩 문구와 키 변환 로직
- 점수 비교, 레벨 종료 비교, 무적 시간, 적 동결 시간 등을 런타임 패치하는 연습

### 추천 난이도 순서

1. 문자열과 export table 찾기
2. `g_ReversingScore` 값을 x64dbg에서 변경하기
3. `g_LevelGate`를 바꾸어 한 스테이지 만에 엔딩 보기
4. `Challenge_CheckUnlock`의 조건 분기를 패치하기
5. `g_IceKey`에 들어갈 올바른 입력을 역산하기
6. SDL2 동적 API 해석 테이블을 복원하기

## 직접 다시 빌드

Windows용 LLVM이 PATH에 등록되어 있다면 `build_windows_llvm.bat`를 실행하십시오. 이 프로젝트는 SDL2 헤더나 import library 없이 빌드되며, SDL2 함수는 실행 중 동적으로 불러옵니다.

생성 파일:

- `PenguinRift.exe`: 최적화 Release
- `PenguinRift_Training.exe`: O0 연습용
- `PenguinRift_Training.pdb`: 디버그 심볼
- `PenguinRift_Training.map`: 주소·심볼 맵

## 구조

- `src/main.cpp`: 게임 및 리버싱 과제 전체 소스
- `build/*.def`: 최소 Windows import library를 만들기 위한 모듈 정의
- `RUN_GAME.bat`: SDL2 설치 확인 후 실행
- `get_sdl2_and_run.ps1`: 공식 SDL2 x64 런타임 자동 설치
- `build_windows_llvm.bat`: Windows x64 재빌드 스크립트
