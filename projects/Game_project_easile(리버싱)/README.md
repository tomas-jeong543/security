# EASILE game-baseline-1.0

게임 기능의 기준은 `docs/EASILE_2D게임_및_보안반복실험_설계서_v0.5.docx`이고, 이번 시각·가독성 개선에는 `docs/EASILE_game-baseline-1.0_visual_design_spec.md`를 적용했습니다. 쿨다운은 후자의 명시적 Basic 짧게/Ultimate 길게 요구를 우선했습니다.

## 빌드와 실행

Visual Studio 2022에서 `EASILE.sln`을 열고 `Debug|x64` 또는 `Release|x64`를 빌드합니다. MSVC v143, C++17, Windows SDK, DirectX 11이 필요합니다. `build/<구성>/GameClient.exe`를 실행합니다. `build/<구성>/GameTests.exe`는 TC G01~G12와 추가 흐름 검증을 수행하고 실패 시 종료 코드 1을 반환합니다. `GameClient.exe --smoke-gui`는 3.85초의 시뮬레이션을 진행해 양쪽 공격을 생성한 뒤 Win32/DX11 5프레임을 렌더하고, DX11 화면을 `build/<구성>/visual-smoke.bmp`에 저장한 후 종료합니다. BMP에는 Win32 자식 창인 HUD가 포함되지 않습니다.

## 조작

- Enter: Menu에서 경기 시작. WASD/방향키: 이동. 마우스: 조준.
- LMB/K: 같은 Basic 공격. J: Ultimate. P: 일시정지/재개.
- 1/2/3: 레벨업 시 HP/Attack/MoveSpeed 강화. R: Result에서 재시작.
- 포커스를 잃은 창은 gameplay 입력을 받지 않습니다.

## 구현 범위

- FR G01~G12: 경기 초기화/상태, Player/Bot, 이동/공격, 독립 쿨다운, 12초 3기 미니언 wave, 타워/기지 파괴, Bot FSM, Brush 은폐와 1.5초 reveal, XP/최대 7레벨, 일시정지, 5~11초 부활을 실행 경로에 연결했습니다.
- 10ms 고정 시뮬레이션 단계에서 투사체와 전투를 갱신합니다. 10ms 내 양쪽 기지 파괴는 Draw입니다. `GameWorld`가 투사체를 소유하며 `CombatSystem`은 피해 판정과 생성 파라미터를 담당합니다.
- Renderer는 읽기 전용 `GameWorldSnapshot`을 사용하고, HUDRenderer는 `HUDViewModel`의 복사본만 Win32 자식 창에 표시합니다. 맵 양 진영/중앙 lane, 오브젝트별 실루엣·외곽선·HP bar, Brush 패턴, 방향이 표시된 Hero, Basic/Ultimate별 크기와 패턴이 다른 투사체를 그립니다. HUD는 HP/XP bar, Level, 두 공격의 READY/남은 시간, 상태와 중앙 Menu/Paused/Respawning/Result 오버레이를 표시합니다.
- `GameClient.log`에는 wall time, simulation time, category, event, entity, detail을 기록합니다. 시작/종료, Hero 사망/부활, Tower/Base 파괴 등이 포함됩니다.

## 구현 세부 선택

- 화면·맵의 논리 좌표는 1280×720입니다. 설계서에 좌표가 없는 양 진영 시작점과 크기 약 240×160인 Brush 두 개는 대칭 배치했습니다. Hero 이동은 맵 경계에서 제한합니다. MD의 Player/Bot 겹침 방지 요구에 따라 살아 있는 두 Hero가 40px 미만으로 접근하면 양쪽을 동일 거리만큼 분리합니다. Tower/Base 이동 차단은 적용하지 않고, 투사체 충돌에 기존 circle/AABB를 사용합니다.
- MD는 Basic/Ultimate 쿨다운의 정확한 수치를 지정하지 않습니다. 짧은 Basic은 1초, 긴 Ultimate는 기존 10초를 사용합니다. 입력·피해량·투사체 속도·1.5초 reveal 등 다른 전투 규칙은 유지합니다.
- Bot은 100ms마다 일반 상태를 결정합니다. 초기 Patrol 이후 Push로 진행하고, 기준 거리/HP/Base 위협에 따라 Engage/Retreat/Defend/Dead로 전환합니다. HP가 30% 이하이면 Tower 중심이 아닌 Tower→자기 Base 방향의 defensive point로 Retreat하고, 도착 뒤 Tower/Base 안전 범위에서 delta time 기준 4 HP/s로 Recover합니다. HP 55% 이상에서 Push로 재출전하며, Tower 파괴 시 Base를 안전 목표로 사용합니다. Recover 중에는 160px 이내의 적에게만 Basic 반격하고 추격하지 않습니다. Bot 강화 선택은 설계서에 정해져 있지 않아 Attack을 선택합니다.
- HUD는 Win32 GDI 자식 창, 게임 도형은 DirectX 11로 표시합니다. 외부 이미지 에셋은 사용하지 않고 Windows의 Segoe UI를 사용합니다. 화면 캡처 BMP는 DX11 백버퍼만 담으므로 HUD는 별도의 GDI 페인트 픽셀 검사와 텍스트 검사로 검증합니다.
- 승리/패배/Draw 및 재시작은 GameWorld 테스트로 검증했고, GUI 스모크는 창/DX11/HUD 초기화와 5프레임 렌더/정상 종료 및 DX11 캡처를 검증합니다. 전체 경기를 직접 키보드·마우스로 수동 플레이하는 검증은 자동 테스트에 포함되지 않습니다.

## 제외 범위

A1~A4 보안 실험과 공격/방어 기능은 구현하지 않았습니다.
