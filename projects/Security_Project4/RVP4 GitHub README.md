# RVP4 – State-Coupled Virtual License System

Windows x64 기반의 **가상 머신(VM)형 라이선스 검증 프로그램을 정적·동적으로 분석한 리버싱 프로젝트​**입니다.

IDA와 x64dbg를 이용해 프로그램의 실행 흐름을 추적하고,  
API 동적 로딩, PEB 기반 Anti-Debugging, `.text` Section Integrity Check, 입력값에 따라 변화하는 Opcode Permutation 및 Custom VM 동작 구조를 분석했습니다.

---

## 1. Project Overview

일반적인 라이선스 검증 프로그램과 달리 이 프로그램은 다음과 같은 분석 방해 요소를 포함합니다.

- Import Table을 사용하지 않는 Windows API 동적 호출
- PEB 및 Windows API를 이용한 다중 Anti-Debugging
- `.text` Section Hash를 이용한 코드 무결성 검사
- 사용자 입력값에 의존하는 내부 상태 생성
- 2단계 Opcode Permutation
- Custom VM을 이용한 라이선스 검증

따라서 단순한 조건 분기 패치가 아니라 프로그램의 내부 상태가 어떻게 생성되고 VM 실행에 전달되는지를 단계적으로 복원하는 것을 목표로 분석했습니다.

---

## 2. Environment

### Analysis Tools

- IDA Free / Hex-Rays Decompiler
- x64dbg
- Python

### Target

- Windows x64 PE Executable
- Custom Virtual Machine 기반 License Validation Program

---

## 3. Analysis Flow

```text
Program Entry
     │
     ▼
Input / Name Normalization
     │
     ▼
License Parsing
     │
     ▼
PEB / Export Table Traversal
     │
     ▼
Windows API Hash Resolution
     │
     ▼
Anti-Debugging Detection
     │
     ▼
.text Section Hash
     │
     ▼
Name-dependent VM State
     │
     ▼
Opcode Permutation #1
     │
     ▼
Opcode Permutation #2
     │
     ▼
Final Opcode Mapping
     │
     ▼
Custom VM Execution
     │
     ▼
License Result
```

---

# 4. 주요 분석 내용

## 4.1 Input Normalization 복원

프로그램의 이름 입력 처리 과정을 분석하여 다음 동작을 복원했습니다.

- 입력 끝의 `CR/LF` 제거
- 좌우 공백 제거
- 연속된 공백을 하나의 공백으로 축소
- 영문 대문자를 소문자로 변환
- Printable ASCII 범위 검사
- 정규화된 문자열 길이 검사

예시:

```text
Input
"   Alice   Bob   "

Normalized
"alice bob"
```

이후 정규화된 이름이 내부 Hash 및 Opcode Permutation 생성에 사용됨을 확인했습니다.

---

## 4.2 License Format 분석

입력된 라이선스 문자열이 Hex Parsing을 거쳐 **24-byte Binary Data**로 변환되는 과정을 분석했습니다.

내부 접근 방식을 기준으로 다음과 같은 구조로 해석했습니다.

```c
#pragma pack(push, 1)

typedef struct LicenseFields
{
    uint32_t field0;
    uint64_t field1;
    uint64_t field2;
    uint32_t field3;
} LicenseFields;

#pragma pack(pop)
```

전체 크기:

```text
4 + 8 + 8 + 4 = 24 bytes
```

각 필드의 크기와 offset은 복원했으며, 분석 단계에서는 의미가 확정되지 않은 필드에 임의의 의미를 부여하지 않고 구조만 복원했습니다.

---

## 4.3 Import-less API Resolution 분석

대상 프로그램은 일반적인 Import Table을 사용하지 않고 함수 포인터를 통해 Windows API를 호출했습니다.

함수 호출 인자와 실행 흐름을 분석하여 다음 API들을 식별했습니다.

```text
ReadFile
WriteFile
GetStdHandle
ExitProcess
IsDebuggerPresent
...
```

또한 PEB Module List와 PE Export Table을 직접 순회하면서 API 이름을 Hash와 비교하는 Resolver 구조를 분석했습니다.

API Hash Algorithm을 Python으로 재구현하여 프로그램 내부에 존재하는 API Hash 값을 실제 Windows API 이름으로 복원했습니다.

관련 코드:

```text
rvp4_api_hash_resolver_corrected.py
rvp4_module_hash_resolver.py
```

---

## 4.4 Anti-Debugging 분석

프로그램이 하나의 Anti-Debugging 기법만 사용하는 것이 아니라 여러 검사 결과를 하나의 `environmentFlags`에 결합한다는 점을 확인했습니다.

분석한 항목:

```text
0x001  PEB.BeingDebugged
0x002  PEB.NtGlobalFlag
0x004  IsDebuggerPresent
0x008  CheckRemoteDebuggerPresent
0x010  NtQueryInformationProcess
0x020  NtQueryInformationProcess
0x040  NtQueryInformationProcess
0x080  Debug Register / Exception Context
0x100  Timing Check
```

x64dbg를 이용한 동적 분석에서 실제 디버거 환경이 다음과 같이 감지되는 것을 확인했습니다.

```text
PEB.BeingDebugged       → Detected
PEB.NtGlobalFlag        → Detected
IsDebuggerPresent()     → Detected
```

각 검사를 개별적으로 모두 패치하는 대신, 최종적으로 VM에 전달되는 `environmentFlags` 상태를 추적하여 분석했습니다.

---

## 4.5 `.text` Section Integrity Check

프로그램이 실행 중 자신의 PE Header를 탐색하여 `.text` Section을 찾고 코드 영역에 대한 Hash를 계산하는 것을 확인했습니다.

복원된 주요 값:

```text
.text VirtualSize : 0x33CE
.text RawOffset   : 0x400
.text RawSize     : 0x3400

Final textSectionHash
→ 0x19F8806B
```

소프트웨어 Breakpoint 또는 코드 패치가 `.text` 영역을 변경하면 Hash 값에도 영향을 줄 수 있다는 점을 확인했습니다.

따라서 이후 분석에서는 Hardware Breakpoint 등을 활용하여 코드 변형으로 인한 내부 상태 변화를 최소화했습니다.

---

## 4.6 Name-dependent State 복원

정규화된 사용자 이름을 이용하여 VM 동작에 필요한 내부 State를 생성하는 과정을 분석했습니다.

주요 중간 상태를 다음과 같이 정리했습니다.

```text
normalizedName
      │
      ▼
normalizedNameHash64
      │
      ▼
nameHashFolded
      │
      ▼
nameHashProduct
      │
      ▼
permutationNameState
```

이를 통해 **동일한 프로그램이라도 입력 이름에 따라 Opcode Mapping이 달라질 수 있는 구조**임을 확인했습니다.

---

# 5. Opcode Permutation 복원

VM Opcode는 고정된 값으로 사용되지 않고 두 번의 Permutation 과정을 거칩니다.

Permutation 생성 알고리즘을 분석한 결과 내부적으로 State를 갱신하면서 배열을 섞는 구조를 확인했으며 이를 Python으로 재구현했습니다.

```python
for i in range(19, 1, -1):
    state = xorshift(state)

    temp = table[i - 1]
    table[i - 1] = table[state % i]
    table[state % i] = temp
```

Permutation 계산 코드는 다음 파일에 구현했습니다.

```text
rvp4_permutation.py
```

예를 들어 `John Smith` 입력의 경우:

```text
Original Name   : John Smith
Normalized Name : john smith

perm1 seed
→ 0x7F47A7175D977D9F

perm1
→ 04 07 0D 05 0B 09 02 03 08
   00 10 11 12 0C 0E 06 01 0F 0A
```

이후 `.text` Section Hash를 포함한 상태를 이용하여 두 번째 Permutation인 `perm2` 역시 복원했습니다.

---

# 6. Final Opcode Mapping

`perm1`과 `perm2`가 실제 VM Dispatch 과정에서 어떻게 사용되는지 참조 관계를 추적했습니다.

실제 실행 흐름은 개념적으로 다음과 같이 동작합니다.

```text
Logical Opcode
      │
      ▼
Permutation #1
      │
      ▼
Permutation #2 Lookup
      │
      ▼
VM Handler
```

이를 통해 입력에 따라 변경되는 Opcode를 다시 실제 VM 명령으로 대응시키는 Mapping을 복원했습니다.

확인한 VM 명령의 예:

```text
NOP
MOV_IMM64
```

`MOV_IMM64`의 경우 Opcode 이후 목적 레지스터와 8-byte Immediate Value를 이용하여 VM Register에 64-bit 상수를 저장하는 구조임을 확인했습니다.

---

# 7. Dynamic Analysis & Patching

IDA의 정적 분석 결과를 x64dbg를 이용해 검증했습니다.

주요 검증 대상:

- Name Normalization 결과
- PEB Anti-Debugging Flag
- `.text` Section Hash
- Permutation Array
- VM State
- 최종 License Result Buffer

최종 출력 생성 과정에서는 결과 문자열이 내부 State와 XOR되는 구조를 추적했고, 해당 State가 결과에 어떤 영향을 미치는지 분석했습니다.

```text
Result: LICENSE ACCEPTED
```

까지의 데이터 흐름을 확인하고 패치를 통해 결과를 검증했습니다.

---

# 8. Python Analysis Tools

분석 과정에서 반복적인 계산을 자동화하기 위해 Python 도구를 작성했습니다.

```text
rvp4_api_hash_resolver_corrected.py
    └─ API Hash → Windows API Name 복원

rvp4_module_hash_resolver.py
    └─ PEB Module Hash 분석

rvp4_permutation.py
    └─ Name Hash 계산
       Permutation Seed 계산
       Perm1 생성
       Perm2 생성
       Opcode Mapping 복원
```

단순히 디버거에서 값을 확인하는 것에 그치지 않고 분석한 알고리즘을 별도 코드로 재구현하여 결과를 비교·검증했습니다.

---

# 9. Project Result

이 프로젝트를 통해 다음 구조를 분석했습니다.

```text
Input Normalization
        +
Dynamic API Resolution
        +
PEB Anti-Debugging
        +
Self Integrity Check
        +
Name-dependent State
        +
Opcode Permutation
        +
Custom VM
```

특히 **Opcode가 고정되어 있지 않고 사용자 입력과 프로그램 상태에 따라 Mapping이 변화하는 구조**를 추적하여 최종 VM Opcode Mapping까지 복원했습니다.

---

# 10. Limitation

분석 후반부에서 대상 Challenge 자체에 논리적 문제가 있음을 확인했습니다.

수정된 실행 파일은 라이선스 형식만 만족해도 성공 결과가 출력되는 문제가 있었으며, 원본 실행 파일 역시 특정 내부 상태에 따라 정상 입력에서 일관된 결과가 나오지 않는 문제가 존재했습니다.

따라서 프로젝트의 목표를 무리하게 완전한 License Generator 제작으로 확장하지 않고,

```text
VM Opcode Mapping 복원
+
VM Handler 일부 분석
+
최종 출력 State 추적
```

까지를 검증 가능한 분석 범위로 설정하여 프로젝트를 마무리했습니다.

이 경험을 통해 **리버싱 대상 자체의 동작과 전제 조건 역시 분석 전에 충분히 검증해야 한다는 점**을 배웠습니다.

---

# 11. What I Learned

프로젝트를 통해 다음 내용을 실제 바이너리를 분석하며 학습했습니다.

- Windows PE 구조
- PEB / Loader Module List
- Export Table Traversal
- Dynamic API Resolution
- API Hashing
- x86-64 Assembly
- Static / Dynamic Analysis
- Anti-Debugging
- Self Integrity Check
- Hardware Breakpoint 활용
- Binary Data Structure 복원
- Custom VM 분석
- Opcode Dispatch
- Opcode Permutation
- Python을 이용한 분석 자동화

---

# 12. Detailed Report

전체 분석 과정과 IDA/x64dbg 스크린샷, 중간 추론 과정은 별도의 상세 보고서에 정리했습니다.

```text
리버싱프로젝트4_v2.pdf
```

상세 보고서는 최종 결과뿐만 아니라 각 변수와 함수의 역할을 어떻게 추론하고 정적 분석 결과를 동적 분석으로 검증했는지를 단계별로 기록하고 있습니다.