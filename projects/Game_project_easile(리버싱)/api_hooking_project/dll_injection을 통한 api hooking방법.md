# EASILE DLL 주입 실행 가이드

현재 EASILE 게임에서 사용하는 `injector.exe`는 DLL을 두 개 순서대로 불러오는 실험용 프로그램이다. 첫 번째는 DLLGuard 함수 후킹용 `Dll2.dll`, 두 번째는 기존 실험용 `payload.dll`이다. 아래 내용은 직접 만든 `GameClient.exe`를 대상으로 로컬에서 테스트하는 방법을 정리한 것이다.

## 1. 실행 전 준비

Visual Studio에서 인젝터와 두 DLL을 **x64 Release**로 빌드하고, 다음과 같이 배치한다.

```text
Release/
├─ GameClient.exe       # 게임 실행 파일 (다른 폴더에 있어도 무방)
├─ injector.exe         # 두 DLL을 순서대로 주입하는 인젝터
├─ Dll2.dll             # DLLGuard 후킹 DLL
└─ payload.dll          # 기존 이동속도 변경 실험 DLL
```

DLL 두 개의 **상대경로는 `injector.exe`가 있는 폴더를 기준**으로 처리한다. 따라서 게임 EXE와 인젝터가 꼭 같은 폴더에 있을 필요는 없다.

## 2. DLL 주입 순서

**① GameClient.exe 실행**

먼저 게임을 실행한 다음 PowerShell에서 프로세스 PID를 확인한다.

```powershell
Get-Process GameClient | Select-Object Id, ProcessName
```

**② 인젝터 실행**

PowerShell에서 인젝터가 들어 있는 폴더로 이동해 실행한다. 아래 `12345`는 확인한 PID로 바꾼다.

```powershell
.\injector.exe 12345
```

기본 설정에서 첫 번째 DLL은 `Dll2.dll`, 두 번째 DLL은 `payload.dll`이다. 별도 경로 인자를 넣을 필요가 없다.

**③ 첫 번째 DLL의 후킹 설치 확인**

인젝터가 `Dll2.dll`을 먼저 로드한 후 입력을 기다린다. 게임 실행 파일 옆의 `guard_observer.log`에서 다음과 같은 **Hook installed** 로그가 있는지 확인한다.

```text
[GUARD-OBS] Hook installed at GameClient + 0xEF90
```

여기서 주의할 점은 **DLL 로드 성공과 후킹 설치 성공은 다르다**는 것이다. 로그가 없다면 바로 다음 단계로 넘어가지 말고 원인을 확인한다.

**④ 두 번째 DLL 주입**

후킹 설치를 확인했으면 인젝터 창에서 **Enter**를 누른다. 그러면 `payload.dll` 주입을 시도한다. 중단하려면 `q`를 입력하고 Enter를 누르면 된다.

**⑤ 결과 확인**

인젝터의 `Both DLLs are present in the target module list.` 메시지는 두 DLL이 모듈 목록에서 확인되었다는 의미다. 실제 후킹 동작은 `guard_observer.log`에서, 게임의 동작 변화나 방어 반응은 게임 화면과 별도 로그에서 확인한다. **두 DLL이 로드되었다는 사실만으로 방어 우회 성공이라고 판단하지 않는다.**

## 3. 다른 폴더에 DLL이 있을 때

DLL 두 개가 각각 하위 폴더에 있다면 상대경로를 직접 지정할 수 있다.

```powershell
.\injector.exe 12345 ".\hook\Dll2.dll" ".\attack\payload.dll"
```

후킹 DLL 하나만 주입하고 싶을 때는 세 번째 DLL 경로 대신 `-`를 넣는다.

```powershell
.\injector.exe 12345 ".\Dll2.dll" -
```

## 4. 자주 발생하는 문제

- **DLL file not found**: DLL 이름, 위치, 상대경로가 올바른지 확인한다.
- **OpenProcess failed**: PID가 정확한지, 게임이 종료되지 않았는지, 접근 권한이 충분한지 확인한다.
- **Hook installed 로그 없음**: `Dll2.dll`은 로드됐더라도 후킹 초기화가 실패했을 수 있다. 게임 빌드와 대상 RVA가 일치하는지도 확인한다.
- **이미 로드된 DLL이라는 오류**: 같은 프로세스에 중복 주입하지 말고, 테스트 게임을 재시작해 다시 시도한다.

> 실험 기록에는 대상 `GameClient.exe` 빌드와 SHA-256, 사용한 DLL, 성공/실패 로그를 함께 남기는 것이 좋다. 특히 RVA `0xEF90`은 분석했던 특정 빌드를 기준으로 하므로 다른 빌드에 그대로 적용하면 안 된다.
