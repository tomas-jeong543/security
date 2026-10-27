# EASILE 2D Anti-Cheat Game 프로젝트 파일 설명서

> 목적: 프로젝트를 다시 열었을 때 **어떤 파일이 무슨 역할인지**, **게임은 어떻게 조작하는지**, **Patch → Defense → 재공격 실험에서 각 실행 파일이 무엇인지** 빠르게 확인하기 위한 간단한 안내서.
>
> 이 문서는 전체 소스의 세부 명세서가 아니라 **중요 파일 중심의 프로젝트 지도**이다.

---

## 1. 프로젝트 개요

EASILE 프로젝트는 2D Top-Down 게임을 먼저 구현한 뒤, 동일한 게임을 대상으로 다음과 같은 보안 실험을 반복하는 프로젝트이다.

```text
Baseline 게임
   ↓
Patch 공격
   ↓
방어 코드 적용
   ↓
방어 적용 빌드 재공격
   ↓
이후 DLL Injection / Hooking 실험으로 확장
```

현재 Patch 실험에서는 대표적으로 **궁극기 쿨타임(기본 10초)을 실행 파일 패치로 변경**하고, 이후 이를 탐지하거나 무력화하는 방어 로직을 추가하는 방식으로 진행한다.

---

# 2. 게임 기본 조작

| 입력 | 기능 |
|---|---|
| `W A S D` | 플레이어 이동 |
| `↑ ↓ ← →` | 플레이어 이동 |
| `LMB` (마우스 왼쪽 클릭) | 마우스 방향으로 기본 공격 |
| `K` | 기본 공격 |
| `J` | 궁극기 사용 |
| `Tab` | 자동 조준 / 마우스 방향 조준 방식 전환 |

### 공격 방식 요약

- 기본 공격은 별도의 10초 쿨타임 없이 사용한다.
- 궁극기 `J`는 정상 게임 기준 **10초 쿨타임**을 가진다.
- Auto 방식에서는 사거리 안의 가까운 보이는 적을 우선하여 공격한다.
- Mouse 방식에서는 마우스 방향을 기준으로 공격 방향을 정한다.

---

# 3. 최상위 프로젝트 파일

## `EASILE.sln`

Visual Studio에서 프로젝트 전체를 여는 솔루션 파일.

보통 프로젝트를 작업할 때는 개별 `.cpp`를 먼저 여는 것이 아니라 다음과 같이 시작한다.

```text
EASILE.sln
    → Visual Studio 실행
    → 구성: Debug 또는 Release
    → 플랫폼: x64
    → GameClient 빌드/실행
```

---

## `GameClient/GameClient.vcxproj`

실제 게임 클라이언트를 빌드하기 위한 Visual Studio C++ 프로젝트 설정 파일.

주요 설정은 다음과 관련된다.

- x64 Debug / Release 빌드
- 포함할 `.cpp`, `.h`
- 컴파일러 옵션
- 링커 옵션
- 실행 파일 생성 위치

일반적인 게임 로직 수정에서는 직접 수정할 일이 많지 않다.

---

# 4. 핵심 소스 파일

## `src/main.cpp`

프로그램의 시작점.

주요 역할:

- 애플리케이션 객체 생성
- 게임 초기화
- 메인 루프 시작
- 프로그램 종료 처리

게임 규칙을 고치는 파일이라기보다는 **프로그램 진입점**에 해당한다.

---

## `src/Application.h`
## `src/Application.cpp`

Windows 애플리케이션의 전체 실행 흐름을 관리한다.

주요 역할:

- 윈도우 생성
- 메시지 루프 처리
- 게임 초기화/종료
- 프레임 단위 Update / Render 호출
- 포커스 및 실행 상태 관리

게임 창 자체나 프로그램 실행 흐름에 문제가 생겼을 때 우선 확인할 파일이다.

---

## `src/GameTime.h`
## `src/GameTime.cpp`

게임 내부 시간 처리를 담당한다.

주요 역할:

- Delta Time 계산
- 게임 경과 시간 관리
- 프레임 간 시간 계산

이동 속도, 회복 속도, 쿨타임 등 **시간에 의존하는 로직**의 기반이다.

---

## `src/GameWorld.h`
## `src/GameWorld.cpp`

현재 프로젝트에서 가장 중요한 파일 중 하나.

게임 한 판의 전체 상태와 주요 게임 규칙을 관리한다.

주요 내용:

- 플레이어 및 Bot 상태
- 미니언
- 타워
- Nexus
- 공격 및 투사체 처리
- 부활
- HP 회복
- XP / Level Up
- Bot AI 상태 전환
- 궁극기 사용 시간
- 맵 오브젝트
- 승패 조건

최근 Patch 방어 실험에서 사용하는 `lastUltimateUseTime_` 같은 **런타임 쿨타임 상태값**도 이 영역과 밀접하다.

게임 규칙에 문제가 생기면 가장 먼저 찾아볼 파일이다.

---

## `src/CombatSystem.h`
## `src/CombatSystem.cpp`

전투 관련 계산을 가능한 한 GameWorld에서 분리하기 위한 파일.

주요 역할의 예:

- 공격 가능 여부 판단
- 기본 공격 / 궁극기 관련 처리
- 데미지 처리
- 쿨타임 관련 검사
- 공격 결과 적용

보안 실험에서 공격 값이나 쿨타임을 방어하는 로직이 들어갈 수 있는 핵심 위치이다.

> `GameWorld.cpp`와 함께 수정되는 경우 선언/정의 위치가 서로 맞는지 반드시 확인한다.

---

## `src/IntegrityGuard.h`
## `src/IntegrityGuard.cpp`

Patch 공격에 대응하기 위한 **무결성 검증(Integrity Check)** 관련 파일.

주요 목적:

- 실행 파일 또는 보호 대상 값의 변조 여부 확인
- 예상 값과 현재 상태 비교
- Patch 흔적 발견 시 방어 처리

Patch 방어 실험의 핵심 보안 파일이다.

일반 게임 기능 구현과 달리 **보안 실험 단계에서 주로 수정**한다.

---

## `src/InputManager.h`
## `src/InputManager.cpp`

키보드와 마우스 입력을 관리한다.

주요 입력:

- WASD
- 방향키
- K
- J
- Tab
- LMB

입력이 먹지 않거나 한 프레임 입력이 누락되는 문제는 이 파일과 Application의 메시지 처리 부분을 함께 확인한다.

---

## `src/Renderer.h`
## `src/Renderer.cpp`

DirectX 11 기반 화면 출력을 담당한다.

주요 역할:

- 맵 렌더링
- 캐릭터 표시
- 미니언 / 타워 / Nexus 표시
- 투사체 표시
- HUD
- 조준점
- 체력 및 게임 정보 출력

게임의 **모양이나 HUD 문제**는 대부분 이 파일을 먼저 확인한다.

---

# 5. 게임 객체 관련 파일

## `src/Hero.h`

플레이어와 Bot Hero가 공유하는 기본 Hero 데이터 구조.

예:

- 위치
- HP
- 이동 관련 값
- 팀 정보
- 공통 상태

---

## `src/PlayerHero.h`

사용자가 직접 조작하는 Hero 관련 정의.

플레이어 전용 상태가 필요할 때 사용한다.

---

## `src/BotHero.h`

AI Bot Hero 관련 정의.

Bot의 상태값 또는 Player와 구분되는 정보가 들어간다.

AI 행동 자체의 큰 흐름은 `GameWorld.cpp`와 함께 확인한다.

---

## `src/Projectile.h`

기본 공격/궁극기 등 투사체 정보를 표현한다.

예:

- 위치
- 진행 방향
- 속도
- 공격력
- 소유 팀
- 활성 여부

공격이 여러 번 맞거나, 이상한 방향으로 날아가거나, 충돌 판정에 문제가 생길 때 확인할 수 있다.

---

## `src/MapPaths.h`

3-Lane 맵에서 미니언 등이 이동할 **경로(Path)** 를 정의하는 중요 파일.

주요 내용:

- Top / Mid / Bottom 경로
- Base에서 Lane으로 이어지는 이동 경로
- Nexus 방향 이동 경로

미니언이 길을 벗어나거나 Nexus를 지나쳐 가는 등의 문제는 이 파일과 GameWorld의 이동 로직을 같이 확인한다.

---

## `src/MathTypes.h`

게임에서 사용하는 기본 수학 자료형 및 계산 보조 코드.

예:

- 2D Vector
- 거리 계산
- 방향 계산
- 정규화

이동/조준/투사체 계산의 기반 파일이다.

---

## `src/Snapshot.h`

특정 시점의 게임 상태를 표현하거나 검증하기 위한 데이터 구조.

게임 테스트 또는 상태 비교 기능에서 사용할 수 있다.

---

# 6. 테스트 관련 파일

## `GameTests/GameTests.cpp`

자동 테스트 프로젝트의 중심 파일.

주요 목적:

- 주요 게임 규칙 확인
- 회귀 테스트
- 수정 후 기존 기능이 깨지지 않았는지 확인

게임 코드 수정 후에는 가능하면 다음 순서로 확인한다.

```text
1. GameTests 빌드
2. 테스트 실행
3. GameClient Release 빌드
4. 실제 게임 직접 실행
```

자동 테스트 PASS만으로 실제 조작 문제가 모두 검증되는 것은 아니므로 마지막에는 직접 플레이 확인도 필요하다.

---

# 7. 문서 관련 파일

## `README.md`

프로젝트 전체를 처음 보는 사람이 가장 먼저 확인하는 간단한 안내 문서.

권장 내용:

- 프로젝트 목적
- 빌드 방법
- 실행 방법
- 조작키
- 주요 폴더
- 테스트 방법

이 문서는 `README.md`보다 조금 더 상세한 **파일 구조 설명서** 역할을 한다.

---

## `docs/`

설계 문서와 보안 실험 기록을 저장하는 폴더.

대표 문서 유형:

- EASILE 게임 설계서
- 버전별 구현/검증 기록
- Patch 공격 실험 기록
- 방어 실험 결과
- 실험 스크린샷
- SHA-256 / Commit / Tag 기록

### 설계서

게임 규칙, 맵, 공격, AI, 보안 실험 방법 등 **현재 기준 명세**를 기록한다.

### 구현/검증 기록

특정 버전에서 무엇을 구현했고 어떤 테스트를 통과했는지를 남긴다.

### 공격·방어 실험 보고서

공격 전후 파일, 패치 위치, 원본/변경 바이트, SHA-256, 실행 결과 등을 기록한다.

---

# 8. 스크린샷에 나온 파일 설명

스크린샷 기준 다음 파일들이 존재한다.

---

## `EASILE_PATCH01_공격_방어_실험보고서_....pdf`

**Patch 실험 1의 최종 보고서 PDF**.

프로젝트 소스가 아니라 실험 결과를 제출하거나 보관하기 위한 문서이다.

들어가는 핵심 정보의 예:

- 실험 환경
- Baseline 실행 파일
- Patch 대상
- RVA
- 원본 바이트
- 변경 바이트
- 공격 결과
- 방어 코드
- 방어 적용 결과
- 재공격 결과
- 각 실행 파일 SHA-256

즉, Patch 01 실험의 **최종 증거/정리 문서**라고 보면 된다.

---

## `GameClient.exe`

일반적으로 Visual Studio에서 현재 소스를 빌드하여 생성한 **현재 GameClient 실행 파일**.

현재 실험 흐름에서는 방어 코드를 적용한 뒤 다시 Release 빌드했다면 이 파일이 **Defense Build** 역할을 할 수 있다.

다만 파일명만으로 실험 단계를 확정하면 헷갈릴 수 있으므로 보고서에는 반드시 SHA-256과 생성 단계도 같이 기록하는 것이 좋다.

예:

```text
GameClient.exe
→ 현재 소스 기준 정상 빌드
→ Defense 코드 적용 후 빌드했다면 Defense Build
```

---

## `GameClient_patched.exe`

방어 적용 전 Baseline 실행 파일을 직접 수정하여 만든 **Patch 공격 파일**.

현재 Patch 01에서는 대표적으로 궁극기 쿨타임 상수를 변경한 공격 결과물을 의미한다.

예:

```text
정상:
Ultimate Cooldown = 10 sec

Patch:
Ultimate Cooldown = 0 sec
```

원본 프로그램과 분리해서 저장함으로써 공격 전/후의 SHA-256 및 동작을 비교할 수 있다.

---

## `GameClient_patch_after_protect.exe`

**방어 코드를 적용한 실행 파일을 다시 Patch 공격한 결과물**.

실험 순서로 표현하면 다음 위치에 해당한다.

```text
GameClient.exe (Baseline)
        ↓ Patch
GameClient_patched.exe
        ↓
방어 코드 작성 및 재빌드
        ↓
GameClient.exe (Defense Build)
        ↓ 다시 Patch
GameClient_patch_after_protect.exe
```

즉 이 파일은

> "방어 코드가 존재하는 상태에서도 동일하거나 유사한 Patch 공격이 가능한가?"

를 검증하기 위한 재공격 파일이다.

방어가 정상 작동한다면 실행 시 변조를 탐지하거나, 단순 Patch만으로 실제 쿨타임 우회가 되지 않도록 하는 것이 목표이다.

---

# 9. Patch 01 파일 관계 요약

```text
[1] Baseline / 정상 빌드
        GameClient.exe
               │
               │ IDA / Hex Patch
               ▼
[2] 공격 결과
        GameClient_patched.exe
               │
               │ 방어 코드 구현
               ▼
[3] Defense Build
        GameClient.exe
               │
               │ 다시 Patch
               ▼
[4] 방어 후 재공격 결과
        GameClient_patch_after_protect.exe
```

※ `GameClient.exe`는 빌드할 때마다 덮어써질 수 있으므로, 보고서의 SHA-256을 기준으로 어느 단계의 파일인지 구분하는 것이 가장 안전하다.

---

# 10. Patch 실험에서 확인하는 값

Patch 분석 시 주로 다음 값을 기록한다.

| 항목 | 의미 |
|---|---|
| RVA | 모듈 기준 상대 주소 |
| VA | 프로세스에 로드된 실제 가상 주소 |
| File Offset | EXE 파일 내부 실제 위치 |
| Original Bytes | Patch 전 기계어 바이트 |
| Patched Bytes | Patch 후 기계어 바이트 |
| Baseline SHA-256 | 원본 실행 파일 해시 |
| Patched SHA-256 | Patch 실행 파일 해시 |
| Defense SHA-256 | 방어 적용 실행 파일 해시 |

단순 실험 재현에는 RVA와 원본/변경 바이트가 핵심이고, 파일의 동일성까지 보장하려면 SHA-256을 같이 기록한다.

---

# 11. 파일을 수정할 때 우선 확인할 곳

## 게임 규칙 변경

```text
GameWorld.cpp
CombatSystem.cpp
Hero 관련 Header
```

## 키 입력 문제

```text
InputManager.cpp
Application.cpp
```

## 화면/HUD 문제

```text
Renderer.cpp
```

## 맵/미니언 경로 문제

```text
MapPaths.h
GameWorld.cpp
```

## 공격/투사체 문제

```text
CombatSystem.cpp
Projectile.h
GameWorld.cpp
```

## 쿨타임 문제

```text
GameWorld.cpp
CombatSystem.cpp
GameTime.cpp
```

## Patch 탐지/무결성 방어

```text
IntegrityGuard.cpp
IntegrityGuard.h
```

## 프로그램 시작/종료 문제

```text
main.cpp
Application.cpp
```

---

# 12. 직접 수정하지 않는 것이 좋은 파일/폴더

다음은 특별한 이유가 없다면 직접 편집하지 않는 것이 좋다.

```text
x64/
Debug/
Release/
.vs/
*.obj
*.pdb
*.ilk
```

이들은 대부분 Visual Studio가 자동으로 생성하는 빌드 결과나 캐시 파일이다.

실행 파일(`.exe`) 자체를 수정하는 것은 **Patch 공격 실험을 할 때만** 수행하고, 정상 개발에서는 원본 소스 코드를 수정한 뒤 다시 빌드한다.

---

# 13. 권장 프로젝트 확인 순서

프로젝트를 오랜만에 다시 열었을 때는 다음 순서가 가장 빠르다.

```text
1. README.md 확인
2. docs의 최신 설계서 확인
3. GameWorld.cpp 확인
4. CombatSystem.cpp 확인
5. IntegrityGuard.cpp 확인
6. GameTests 실행
7. GameClient Release x64 빌드
8. 직접 플레이
9. 보안 실험용 EXE의 SHA-256 비교
```

---

# 14. 현재 핵심 파일만 빠르게 정리

| 중요도 | 파일 | 한 줄 설명 |
|---|---|---|
| ★★★★★ | `GameWorld.cpp` | 게임 전체 규칙 및 상태 관리 |
| ★★★★★ | `CombatSystem.cpp` | 공격/데미지/쿨타임 관련 핵심 로직 |
| ★★★★★ | `IntegrityGuard.cpp` | Patch 방어 및 무결성 검증 |
| ★★★★☆ | `Renderer.cpp` | DX11 화면 및 HUD 출력 |
| ★★★★☆ | `InputManager.cpp` | 키보드/마우스 입력 |
| ★★★★☆ | `MapPaths.h` | 3라인 이동 경로 |
| ★★★☆☆ | `Application.cpp` | 프로그램/게임 루프 |
| ★★★☆☆ | `GameTime.cpp` | 게임 시간 계산 |
| ★★★☆☆ | `Projectile.h` | 공격 투사체 데이터 |
| ★★★☆☆ | `GameTests.cpp` | 자동 회귀 테스트 |
| ★★☆☆☆ | `MathTypes.h` | 벡터/거리 등 수학 처리 |
| ★★☆☆☆ | `Snapshot.h` | 상태 기록/검증용 구조 |
| ★★☆☆☆ | `Hero.h` | Hero 공통 데이터 |
| ★☆☆☆☆ | `main.cpp` | 프로그램 진입점 |

---

# 15. 한 문장으로 프로젝트 구조 이해하기

> **Application이 프로그램을 돌리고, InputManager가 입력을 받고, GameWorld와 CombatSystem이 게임 규칙을 처리하며, Renderer가 화면을 그리고, IntegrityGuard가 Patch 공격을 방어하고, GameTests가 수정 결과를 검증한다.**

