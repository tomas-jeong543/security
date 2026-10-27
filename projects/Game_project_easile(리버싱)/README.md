# EASILE Baseline 1.0 (v0.14) — CONFIRMED

Player + Ally Bot 대 Enemy Bot 2명이 확장된 3 Lane과 중앙 Jungle에서 싸우는 Windows x64 / DirectX 11 게임입니다. 월드는 1920×1440, Lane 폭은 280이며 Player 중심 1280×720 카메라를 사용합니다.

기준: [v0.14 설계서](docs/EASILE_2D게임_및_보안반복실험_설계서_v0.14.docx), [Baseline 1.0 확정 기록](docs/BASELINE_1.0.md), [검증 기록](evidence/v014-validation.md).

**Baseline Status: CONFIRMED · Baseline Version: 1.0 (2026-09-30).** 사용자가 게임을 직접 실행해 이동, 전투, 스킬 사용과 전체 플레이 흐름을 확인하고 이후 보안 실험의 기준으로 승인했습니다. 빌드·자동 테스트 결과와 사용자 확인의 출처는 확정 기록에 구분해 적었습니다.

## 빌드와 실행

Visual Studio 2022에서 EASILE.sln을 열어 Debug|x64 또는 Release|x64로 빌드합니다. MSVC v143, C++17, Windows SDK, DirectX 11이 필요합니다.

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' EASILE.sln /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' EASILE.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
& .\build\Debug\GameClient.exe
& .\build\Release\GameClient.exe
```

각 구성 GameTests.exe는 G01~G28과 추가 회귀를 검사하고 실패 시 종료 코드 1입니다. GameClient.exe --smoke-gui는 실제 Win32/DX11/HUD와 5프레임 캡처를 검사합니다. GameTests.exe --visual-expanded는 시작·중앙 전투·정글·카메라 경계·후퇴 장면을 캡처합니다. DX11 BMP에는 GDI 자식 HUD가 없어 별도의 paint/text 테스트와 GUI 캡처로 확인합니다.

## 조작

- Enter: 시작. WASD / 방향키: 이동. P: 일시정지·재개. R: Result에서 재시작.
- LMB: 항상 마우스 방향 Basic. K: 기본 Auto, 사거리 700 안 가까운 보이는 유효 적.
- J: Ultimate, 기본 Auto 사거리 1000, **10초 쿨다운**. Tab: K/J Auto·Mouse 전환.
- LMB/K는 새 누름마다 즉시 한 발, 길게 누름은 반복하지 않으며 동시 누름은 한 발입니다.
- 1/2/3: 이후 레벨업 HP/Attack/MoveSpeed 선호. 기본 Attack.
- 포커스 상실 시 gameplay 입력을 받지 않습니다.

## 게임 구조

- 팀별 Hero 2명, Tower 3개와 Nexus 1개. Bot별 HP·위치·FSM·표적·공격 주기·회복·XP가 독립적입니다.
- 공통 MapPaths.h가 넓어진 Lane과 정글 통로·전투 공간·벽의 렌더링, 이동, 길찾기, 공격 판정을 제공합니다. 몸체·고속 투사체의 벽 관통과 다른 Lane 직접 공격을 막습니다.
- Bush 6개, 공격 후 1.5초 reveal, 정글 회복 오브젝트 시작 20초 / HP 20 / 획득 후 30초 재생성.
- 12초마다 팀·Lane별 Minion 2기. 자기 Lane Tower→적 Minion→Nexus 순으로 공격합니다. 확대 맵 템포를 위해 Minion 속도 110→130, Hero 속도 220 유지.
- Push/Engage/Defend, Retreat→Recover→Push. Bot은 자기 Tower 뒤에서 왕복·Minion 사격·안전 시 Hero 견제를 계속하고 정글·연결 도로로 Lane을 이동합니다.
- Basic 5, Ultimate 30, 처치 XP·최대 7레벨·자연 회복·Nexus 주변 부활·10ms 동시 Nexus 파괴 Draw 유지. XP는 실제 발사한 Hero에 귀속됩니다.
- Player 카메라 offset을 월드 렌더링과 마우스 조준에 적용하며 HUD는 화면에 고정합니다.

## 검증과 후속 실험

빌드·기존/신규 회귀·GameClient 실행·GUI 입력·전체 경기의 결과는 검증 기록을 확인하세요. 자동 입력 Match Duration은 사람의 평균 경기 시간을 뜻하지 않습니다. Baseline 1.0 승인과 이후 실험 식별 방법은 확정 기록을 확인하세요.

A1~A4 보안 실험은 아직 수행하지 않았습니다. 다음 실험은 **Ultimate Cooldown Patch: 10초 → 1초**이며, 현재 Baseline의 게임 수치와 코드는 변경하지 않았습니다.
