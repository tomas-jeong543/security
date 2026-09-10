# 보안 포트폴리오 | 리버싱과 시스템 기초

보안 동아리 활동에서 수행한 바이너리 분석, 분석 알고리즘 재구현, 시스템 실습을 정리한 저장소입니다.

## 대표 프로젝트

### 1. RVP4 — VM 기반 라이선스 검증 프로그램 분석

- **목표:** 입력과 내부 상태에 따라 달라지는 VM 검증 흐름 복원.
- **사용 기술:** Windows x64 PE, IDA, x64dbg, Python, API Hashing, Anti-Debugging.
- **수행한 분석:** API 동적 해석, `.text` 무결성 검사, 이름 정규화, 두 단계 Opcode Permutation과 일부 VM Handler 추적.
- **결과:** Python 분석 도구와 최종 Opcode Mapping을 정리하고 패치로 출력 상태를 검증했습니다. 완전한 라이선스 생성기 구현은 완료 범위에 포함하지 않으며, 대상 프로그램의 문제와 분석 한계를 기록했습니다.
- **배운 점:** 코드 변경이 무결성 해시에 미치는 영향, 정적·동적 분석의 교차 검증, 분석 대상의 전제 조건 검증.

[프로젝트 README](projects/Security_Project4/README.md) · [상세 보고서](projects/Security_Project4/리버싱프로젝트4_v2.pdf) · [분석 도구](projects/Security_Project4/source/)

### 2. Toy OS — x86 부팅과 보호 모드 실습

- **목표:** 부트로더와 최소 실행 환경을 통해 저수준 시스템 동작 이해.
- **사용 기술:** x86 Assembly, NASM, QEMU, BIOS, GDT, VGA 텍스트 메모리.
- **수행한 구현·검토:** `boot11.asm`에 실 모드 부팅, 추가 섹터 로딩, 보호 모드 전환, 화면 출력과 명령 문자열 검사 코드가 있습니다.
- **결과:** 단계별 Assembly 소스와 부팅 이미지, QEMU 명령 메모를 보관했습니다. 최종본 지정과 실제 실행 검증 결과는 **확인 필요**입니다.
- **배운 점:** 코드에서 다루는 학습 주제는 세그먼트 설정, 스택 초기화, 모드 전환과 화면 메모리 접근입니다. 개인 회고는 **TODO**입니다.

[프로젝트 README](projects/toyos_project/README.md) · [기존 문서 / Word](projects/toyos_project/toy_os.docx) · [실행 메모](projects/toyos_project/QEMU_내용.txt)

### 3. War Game / Reversing — 문제별 풀이와 동적 분석

- **목표:** 리버싱 문제의 데이터 변환과 실행 흐름을 추적하고 풀이를 코드·문서로 기록.
- **사용 기술:** Python, IDA, GDB/pwndbg. 사용 도구는 문제별로 다릅니다.
- **수행한 분석:** 기초 문자열 복원 코드와 `rev_randzz`의 주소 대응, 중단점 및 레지스터 조작 절차를 기록했습니다.
- **결과:** 풀이 스크립트, 패치 문제 보고서, 동적 분석 메모를 보관했습니다. 전체 문제의 풀이 성공 여부와 플랫폼·문제 출처는 **확인 필요**입니다.
- **배운 점:** 정적 분석 주소와 런타임 주소의 대응, 실행 상태 변경을 통한 분기 조건 검토. 문제별 개인 회고는 **TODO**입니다.

[프로젝트 README](War_Game/Reversing/README.md) · [패치 문제 보고서](War_Game/Reversing/rev_patch/patch문제풀이.pdf) · [동적 분석 메모](War_Game/Reversing/rev_randzz/rev_rand.txt)

## 코드와 분석 대상 구분

| 구분 | 위치와 설명 |
| --- | --- |
| 직접 작성한 분석 도구 | RVP4 `source/*.py`: 기존 프로젝트 문서에서 분석 알고리즘을 Python으로 재구현했다고 명시합니다. 파일별 외부 코드 활용 여부는 확인 필요입니다. |
| 실습 구현·풀이 코드 | Toy OS `boot*.asm`, War Game의 Python 스크립트: 실습 구현 및 풀이 자료입니다. 직접 작성·제공 코드·수정 코드의 정확한 범위는 확인 필요입니다. |
| 복원·재구성 코드 | [프로젝트 3 Go 코드](projects/Security_project3/source/reversing_project3_reconstructed.go)는 헤더에서 동작 재구성 코드임을 명시합니다. 분석 대상의 원본 소스와 구분합니다. |
| 분석 대상 및 패치본 | RVP4 `exe_files/`의 구분은 [파일 설명](projects/Security_Project4/exe_files/explanation.txt)을 참고합니다. War Game의 `chall`, `encrypted` 등은 출처와 역할 확인이 필요합니다. |
| 생성물 및 분석 작업 파일 | Toy OS의 `.bin`·`.img`, IDA의 `.i64` 등입니다. 원본·생성물의 대응 관계가 미기록된 항목은 확인 필요입니다. |

## 저장소 안내

| 디렉터리 | 내용 |
| --- | --- |
| [projects/](projects/README.md) | 리버싱 프로젝트와 Toy OS 실습 |
| [War_Game/Reversing/](War_Game/Reversing/README.md) | 리버싱 워게임 풀이 |
| [lectures/](lectures/README.md) | 날짜별 학습 기록과 실습 자료 |
| [assignments/](assignments/README.md) | 동아리 과제와 풀이 |

## 재현 및 문서 상태

프로젝트마다 필요한 환경이 다릅니다. 각 README와 기존 메모를 참고하십시오. 일부 실행 파일·DLL은 로컬에 있지만 `.gitignore`로 제외되어 Git으로 내려받은 사본에는 없을 수 있습니다. 확보 방법, 도구 버전, 예제 입력과 기대 출력은 프로젝트별 **TODO**입니다.

요약은 기존 텍스트 문서와 소스에서 확인한 범위에 한정합니다. 연결된 PDF·Word 보고서 전체와 모든 프로그램의 실행 결과를 재검증한 것은 아닙니다.
