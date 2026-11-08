# EASILE HK-01 — Injector + MinHook TODO 템플릿

## 포함 파일
- `injector.c`: 기존 RemoteThread + LoadLibrary 주입 코드 개선 버전. PID 및 DLL 절대 경로 인수 지원, 오류 검사/정리/로딩 확인.
- `hook_payload.c`: MinHook 초기화, DLL 로딩, 비동기 초기화 워커 포함. **실제 Hook은 미구현(TODO)**.
- `injector.vcxproj`, `hook_payload.vcxproj`, `EASILE_HK01.sln`: VS2022 / x64 프로젝트.

## 사전 조건
- Visual Studio 2022 C++ 데스크톱 워크로드 / Windows SDK.
- MinHook v1.3.4 정적 라이브러리 (`libMinHook.x64.lib`)를 이미 Release x64로 빌드했어야 함.
- `hook_payload.vcxproj`의 `MinHookRoot`는 현재 알려진 경로 `D:\Downloads\minhook-1.3.4\minhook-1.3.4`로 지정함.
- **권장 빌드: Release / x64**. 확인된 MinHook 라이브러리가 Release x64이므로 DLL 역시 Release로 빌드하기.
- `hook_payload`의 CRT를 `/MT`로 지정하여 MinHook Release 정적 라이브러리(`LIBCMT`)와 맞춤. Debug 구성도 MinHook Release 정적 라이브러리와 `/MT`를 쓰므로, 디버그 CRT 진단이 필요할 때는 MinHook Debug 버전도 새로 빌드해 재설정해야 함.
- 두 프로젝트의 중간 파일 디렉터리는 `obj\x64\Release\injector\`, `obj\x64\Release\hook_payload\`로 격리되어 MSB8028/C1041의 동일 PDB 접근 충돌을 방지함.

## 빌드
1. EASILE_HK01.sln 열기.
2. Release / x64 선택.
3. 솔루션 빌드.
4. 산출물: `out\x64\Release\injector.exe`, `out\x64\Release\hook_payload.dll`.
5. **이전 버전에서 덮어쓴 경우**, 솔루션 빌드 전에 두 프로젝트 각각 **정리(Clean)** 후 빌드하거나, 기존 `obj`/`out` 폴더를 삭제하고 다시 빌드하기.

## 시험 실행
`injector.exe <GameClient_PID> "D:\...\hook_payload.dll"`

DLL이 로딩되더라도 **Hook 설치는 아직 되지 않음**. DebugView 또는 Visual Studio OutputDebugString 로그에 `TODO: SpawnProjectile hook is not installed yet.`가 출력되도록 구성.

## 과제 단계
1. **TODO 1**: `GameWorld::SpawnProjectile()` 실제 함수 선언/정의 확보. 호출 규약 및 매개변수 확인.
2. **TODO 2**: 원본 함수 포인터 타입 선언.
3. **TODO 3**: Detour 함수 작성. 적 Bot 기본 공격만 방향 +30° (좌표계/단위 확인 후) 변환하고 나머지는 통과.
4. **TODO 4**: 대상 EXE의 SHA-256/PDB/RVA 확인. 임의 빌드에서 `0x137E0` 사용 금지.
5. **TODO 5**: `MH_CreateHook()` → `MH_EnableHook()` 연결 및 오류 처리.
6. **TODO 6**: Hook 제거 및 DLL 언로드 시 안전한 종료 절차 추가.

## 유의점
- 주입 성공과 Hook 설치 성공은 별개의 상태.
- `hook_payload.c`는 학습용 스타터이며 완제품/실전 방어 우회 코드가 아님.
- 설치 이후 함수 주소/시그니처가 달라진 상태에서 실행하면 대상 게임이 크래시할 수 있음.
- Injector는 x64 대상만 지원하며, 접근 보호가 있는 프로세스나 다른 사용자의 프로세스에서 동작을 보장하지 않음.
- 동일 이름의 DLL이 이미 로딩되었다면 Injector는 혼동 방지를 위해 중단함.

## 이전 스타터에서 수정한 부분 (v2)
- `injector.vcxproj` / `hook_payload.vcxproj` 중간 빌드 경로에 `$(Platform)\$(Configuration)\$(ProjectName)`을 포함: C1041, MSB8028 해결.
- `hook_payload.vcxproj`의 CRT를 `/MD`에서 `/MT`로 변경: 컴파일 로그의 LNK4098 `LIBCMT` 충돌 원인 해소 목적.
- 출력 경로에 `x64`를 포함해 구성 및 플랫폼별 산출물 분리.
- `injector.c`의 `LoadLibraryW` 종료값은 64비트 핸들을 완전히 나타내지 못하므로, DLL 로딩 여부는 모듈 목록 검증을 기준으로 판정.

이 ZIP의 MSBuild XML 및 경로 격리는 정적 검증했으나 Windows/VS2022에서 실제 빌드까지 검증한 것은 아님.
