# EASILE 프로젝트 작업 규칙

이 규칙은 이 디렉터리와 하위 경로의 작업에 적용한다. 사용자 지시와 승인 범위를 우선하며, 계획 문서와 과거 승인 기록을 새 구현의 승인으로 해석하지 않는다.

## 저장소와 빌드

- `src/`: 게임 규칙, Win32 실행·입력, DX11/GDI 표시, 클라이언트 보안 구현. 상세 사실은 [저장소 조사](docs/server-extension/repository-audit.md)를 참조한다.
- `GameClient/`: `GameClient.vcxproj`, `GameTests.vcxproj`. 실제 소스는 `../src/`, 테스트는 `../tests/`를 참조한다.
- `EASILE.sln`: GameClient·GameTests, Debug/Release x64. VS2022/MSVC v143, C++17, Windows SDK 10.0, DX11. 산출물은 `build/<Configuration>/`.
- `tests/GameTests.cpp`: 자체 실행기 기반 49개 등록 테스트와 시각 검증 옵션. 등록 수와 실제 통과 수를 구분한다.
- `api_hooking_project/`: HK-01 개발 솔루션, Dll2, 실행 파일·로그·보고서. 루트 게임 솔루션과 별도이다.
- `api_hooking_v11-7/`: HK-01 소스·실험 바이너리·보고서 묶음. 독립 빌드 프로젝트로 단정하지 않는다.
- `dll_injection_project/`: 이동속도 초기화 코드 패치 및 보호 전후 실험 자료.
- `docs/`, `evidence/`, `release_v10_03/`, `build/`: 기존 문서·검증·배포·빌드 자료. 기존 산출물도 실험 증거일 수 있다.
- 신규 GameServer와 통신 코드는 아직 없다. [서버 확장 문서](docs/server-extension/README.md)는 설계 초안이다.

빌드 명령 예시이며 실행 승인이 아니다. 증거 보존 뒤 승인된 검증 작업에서 사용한다.

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' EASILE.sln /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' EASILE.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
```

## 보존과 변경 범위

1. 승인된 마일스톤의 파일·동작만 구현한다. 현재 승인은 AGENTS.md·Skills·서버 설계 초안 작성이다. 서버 구현, 소스·솔루션·기존 테스트 변경, 게임·공격 실행은 포함하지 않는다.
2. Baseline, 원본 공격 소스·바이너리, 기존 로그·보고서·문서를 덮어쓰거나 정리하지 않는다. 재현 실험은 식별 가능한 별도 출력·증거 경로를 사용한다. 빌드가 기존 EXE/PDB를 바꾸기 전에 해시와 보존 사본을 확보한다.
3. IntegrityGuard의 CRC, DLLGuard, SpeedGuard 및 런타임 쿨다운의 임의 삭제·비활성·우회·예외 확대를 금지한다. 경계 분리 시 기존 호출·실패 처리·주기를 보존하고 변화는 검토 가능한 diff로 제시한다.
4. 방어를 끄는 기존 Dll2 실험 자료는 보존하되 일반 클라이언트 통합에 사용하지 않는다. 방어 활성 실행과 우회 실행의 결과를 합치지 않는다.
5. 기존 테스트의 삭제·건너뛰기·기대값 완화·실패 무시로 실패를 은폐하지 않는다. 요구사항 변경에 따른 테스트 수정은 근거와 사람 검토를 남기고, 기존 실패와 수정 후 결과를 함께 보존한다.
6. 외부 저장소의 기존 변경을 되돌리거나 stage·commit하지 않는다. Git 최상위가 이 프로젝트보다 상위일 수 있으므로 먼저 확인하고 경로를 제한한다.

## 사실과 검증

- 파일·함수의 존재, 빌드 포함, 호출 연결, 런타임 활성, 효과 관측을 별도로 기록한다. `VerifyHeroIntegrity()` 구현만으로 보호 활성이라고 쓰지 않는다.
- 고정 RVA·CRC는 빌드에 종속된다. 과거 EXE와 현재 소스·PDB·테스트 EXE의 대응 관계를 추정하지 않는다.
- 클라이언트 자체 보고 해시·보안 성공 신호는 서버의 신뢰 증거가 아니다. 조사자가 파일에서 직접 계산한 해시는 실험 식별용이다.
- 실행하지 않은 검사는 `미실행`, 증거 부족은 `미검증`, 설계 미결정은 `TBD`로 기록한다. 과거 PASS를 현재 트리에 승계하지 않는다.
- 모든 변경 작업에서 작업 전후 `git status --short -- .`, `git diff -- .`, `git diff --check -- .`와 검증 명령·범위·종료 코드·증거 위치를 남긴다. 새 미추적 파일은 일반 diff에서 빠지므로 `git diff --no-index -- /dev/null <file>`로 별도 검토한다(추가 diff의 종료 코드 1은 차이를 뜻한다). Git 상태상 보이지 않으면 ignore·추적 상태를 조사하고 누락을 숨기지 않는다.
- 문서만 변경한 경우에는 링크·Skill 메타데이터·diff·보존 확인을 수행하고 게임 빌드·테스트 미실행 이유를 기록한다. 검사 결과를 게임 기능 PASS로 표현하지 않는다.
- 각 작업 기록에는 승인 범위, 변경 파일, 실제 검사, 미실행 검사, 알려진 실패·TBD, 사람 검토 항목을 포함한다. 기존 증거 파일은 갱신하지 않고 새 실행 ID로 저장한다.

## 프로젝트 Skills

- [easile-build-validation](.agents/skills/easile-build-validation/SKILL.md): 구성별 빌드·테스트와 결과 기록.
- [easile-security-regression](.agents/skills/easile-security-regression/SKILL.md): 기존 방어의 호출·활성과 회귀 검토.
- [easile-experiment-evidence](.agents/skills/easile-experiment-evidence/SKILL.md): 공격·방어 조건과 증거 분리.
- [easile-server-extension](.agents/skills/easile-server-extension/SKILL.md): 승인된 서버 확장 마일스톤의 설계·변경.

마일스톤 경계와 사람 검토는 [개발 계획](docs/server-extension/implementation-plan.md)을 따른다. 해당 문서의 완료 조건은 아직 달성 결과가 아니다.
