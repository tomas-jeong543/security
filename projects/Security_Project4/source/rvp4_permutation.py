r"""
rvp4_permutation.py

RVP4의 이름 기반 상태값, perm1, perm2, 그리고 최종 opcode 매핑을 계산한다.

현재 구현 범위
--------------
1. 사용자 이름 정규화
2. 정규화된 이름으로 v298 계산
3. v298 -> v330 -> v334 -> v419 계산
4. v419로 첫 번째 permutation seed 생성
5. 19개 opcode(0x00~0x12)를 Fisher-Yates 방식으로 섞어 perm1 생성

향후 확장
---------
두 번째 permutation(perm2)은 아래 값이 추가로 필요하다.

    textSectionHash

디컴파일 코드 기준:

    v355 = v419 ^ ((uint64_t)textSectionHash << 32)
    perm2_seed = v355 ^ 0x53A9D1E794671459

그래서 이 파일은 이름 분석과 공통 셔플 로직을 분리해 두었다.
나중에 generate_perm2()만 추가하거나 활성화하면 된다.


## 기본 사용

먼저 파일이 있는 폴더로 이동:

```powershell
cd "D:\Downloads\Security_project4_challenge\Security_project4\challenge"
```

이름으로 `v419`와 `perm1` 계산:

```powershell
python rvp4_permutation.py Tomas
```

공백이 있는 이름은 따옴표 사용:

```powershell
python rvp4_permutation.py "John Smith"
```

## 검증

`Tomas` 입력이 x64dbg에서 확인한 값과 일치하는지 검사:

```powershell
python rvp4_permutation.py --self-test
```

정상 기준:

```text
python rvp4_permutation.py Tomas --text-hash 19F8806Bv419 = 0x0B5860F65C1EC6FC
```

## perm2까지 계산

패치된 `.text` 해시가 `0x19F8806B`이라면:

```powershell
python rvp4_permutation.py Tomas --text-hash 19F8806B
```

출력에는 다음이 포함돼.

```text
정규화 이름
v298
v330
v334
v419
perm1 seed
perm1
perm2 seed
perm2
```

## 셔플 과정 자세히 보기

각 단계의 PRNG 상태와 교환 위치를 출력:

```powershell
python rvp4_permutation.py Tomas --trace
```

perm2 과정까지 같이 보려면:

```powershell
python rvp4_permutation.py Tomas --text-hash 19F8806B --trace
```



perm2활용하려면 어떤 함수를 활성화 해야 돼?

따로 활성화할 함수는 없어. 이미 코드에 `perm2` 계산 함수가 들어가 있어.

사용할 핵심 함수는 두 개야.

```python
generate_perm2_seed(v419, text_section_hash)
generate_perm2(v419, text_section_hash)
```

명령줄에서는 `--text-hash` 옵션을 주면 `main()`이 자동으로 `generate_perm2()`를 호출해.

```powershell
python rvp4_permutation.py Tomas --text-hash 19F8806B
```

코드 내부에서 직접 사용할 때는:

```python
state = generate_perm1_from_name("Tomas")

perm2 = generate_perm2(
    v419=state.v419,
    text_section_hash=0x19F8806B,
)

print(format_bytes(perm2))
```

즉 수정하거나 주석을 해제할 부분은 없고, **`textSectionHash`를 전달해야 perm2가 계산되는 구조**야. `perm2`의 seed만 필요하면 `generate_perm2_seed()`를 사용하면 돼.

## 최종 opcode 매핑 계산

최종 opcode 매핑까지 계산하려면 이름, textSectionHash,
그리고 `--final-map` 옵션을 함께 전달한다.

```powershell
python rvp4_permutation_final_mapping.py "John  Smith" --text-hash 19F8806B --final-map
```

입력값의 의미:

- `"John  Smith"`
  - 프로그램에 넣을 원본 이름이다.
  - 내부에서 `john smith`로 정규화된다.
  - 이름이 바뀌면 v298, v419, perm1, perm2, 최종 매핑도 달라질 수 있다.

- `--text-hash 19F8806B`
  - 실행 중 계산되는 32비트 `.text` 섹션 해시다.
  - perm2 seed 계산에 사용된다.
  - 패치된 실행파일과 원본 실행파일은 이 값이 달라질 수 있으므로,
    반드시 실제 실행 환경에서 확인한 값을 넣어야 한다.
  - `0x` 접두사는 생략하고 8자리 16진수로 넣어도 된다.

- `--final-map`
  - perm1과 perm2를 합성해서 최종 handler 매핑을 출력한다.
  - `--text-hash` 없이 사용할 수 없다.

출력값의 의미:

```text
원본 논리 opcode -> perm1 인코딩 값 -> 최종 VM handler
```

예:

```text
0x02 -> 0x11 -> handler 0x0B
```

의미:

1. 원래 bytecode의 논리 opcode가 `0x02`다.
2. bytecode 재작성 단계에서 `perm1[0x02] == 0x11`이므로
   실행용 opcode 바이트가 `0x11`로 바뀐다.
3. VM dispatch는 `perm2[k] == 0x11`인 위치 `k`를 찾는다.
4. 그 결과 최종적으로 handler `0x0B`가 실행된다.

즉 최종 매핑 공식은:

```python
final_handler = inverse_perm2[perm1[original_opcode]]
```

주의:

- `handler 0x0B`는 아직 명령 이름이 아니다.
- 이후 VM handler 분석을 통해 handler 0x0B가 XOR, ADD, CMP,
  PUSH, POP 같은 어떤 동작인지 별도로 이름 붙여야 한다.

final mapping사용방법
  python rvp4_permutation_final_mapping.py "John  Smith" --text-hash 19F8806B --final-map 과 같이 입력하면 되며 긱각의 구성요소는
  "John Smith": 프로그램에 넣는 원본 이름
--text-hash 19F8806B: 실제 실행파일에서 확인한 .text 섹션 해시
--final-map: perm1과 perm2를 합성해 최종 handler 매핑 출력 
의 의미를 같는다.

"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass


MASK64 = 0xFFFFFFFFFFFFFFFF

# 이름 해시 초기값
# IDA: v298 = 0xCBF29CE484222325uLL;
NAME_HASH_INITIAL = 0xCBF29CE484222325

# 이름 해시에서 사용하는 FNV-1a 계열 곱셈 상수
NAME_HASH_PRIME = 0x100000001B3

# v334 계산 상수
V334_MULTIPLIER = 0x9E3779B185EBCA87

# 첫 번째 permutation seed용 XOR 상수
PERM1_SEED_XOR = 0x64C7920094671459

# 두 번째 permutation seed용 XOR 상수
# generate_perm2() 확장용으로 미리 정의한다.
PERM2_SEED_XOR = 0x53A9D1E794671459

# opcode 초기 배열:
# xmmword_140005200 = 00~0F
# v437 = 0x1110        -> 메모리상 10 11
# v438 = 0x12
INITIAL_OPCODE_TABLE = tuple(range(0x13))


@dataclass(frozen=True)
class NameState:
    """이름에서 perm1까지 이어지는 중간값을 모두 보관한다."""

    original_name: str
    normalized_name: str
    normalized_bytes: bytes
    v298: int
    v330: int
    v334: int
    v419: int
    perm1_seed: int
    perm1: tuple[int, ...]


def u64(value: int) -> int:
    """C의 unsigned __int64처럼 결과를 64비트로 제한한다."""
    return value & MASK64


def rol64(value: int, count: int) -> int:
    """64비트 rotate-left."""
    count &= 63
    value &= MASK64

    if count == 0:
        return value

    return u64((value << count) | (value >> (64 - count)))


def normalize_name(name: str) -> str:
    """
    IDA의 이름 정규화 루틴을 재현한다.

    규칙:
    - 앞뒤 공백 제거
    - 연속된 공백은 하나로 축약
    - A~Z는 a~z로 변환
    - 허용 문자는 출력 가능한 ASCII 0x20~0x7E
    - 정규화 후 길이는 3~24바이트

    프로그램은 바이트 단위로 처리하므로, Python에서도 ASCII만 허용한다.
    """
    try:
        raw = name.encode("ascii")
    except UnicodeEncodeError as exc:
        raise ValueError("이름은 ASCII 문자만 사용할 수 있다.") from exc

    # 프로그램 입력 처리에서 앞뒤 공백을 먼저 제거한다.
    raw = raw.strip(b" ")

    normalized = bytearray()
    previous_was_space = False

    for byte in raw:
        # 디컴파일 코드의 유효 범위는 출력 가능한 ASCII 0x20~0x7E다.
        if not 0x20 <= byte <= 0x7E:
            raise ValueError(
                f"허용되지 않는 문자가 포함됨: 0x{byte:02X}"
            )

        if byte == 0x20:
            # 연속된 공백은 첫 번째 공백 하나만 남긴다.
            if previous_was_space:
                continue

            normalized.append(byte)
            previous_was_space = True
            continue

        # ASCII 대문자를 소문자로 바꾼다.
        if 0x41 <= byte <= 0x5A:
            byte += 0x20

        normalized.append(byte)
        previous_was_space = False

    if not 3 <= len(normalized) <= 24:
        raise ValueError(
            "정규화된 이름 길이는 3~24바이트여야 한다. "
            f"현재 길이: {len(normalized)}"
        )

    return normalized.decode("ascii")


def update_name_hash(current_hash: int, byte: int) -> int:
    """
    이름 바이트 하나를 v298에 반영한다.

    IDA의 홀수 바이트 처리:

        t = 0x100000001B3 * (v298 ^ byte)

        v298 =
            t
            ^ (
                (0x80000000D9800000 * (v298 ^ byte))
                | (t >> 41)
              )

    여기서:

        0x80000000D9800000
        == (0x100000001B3 << 23) mod 2^64

    이므로 괄호 부분은 정확히 rol64(t, 23)이다.

    따라서 의미상:

        t = PRIME * (hash ^ byte)
        hash = t ^ rol64(t, 23)

    로 복원할 수 있다.
    """
    multiplied = u64(NAME_HASH_PRIME * (current_hash ^ byte))
    return u64(multiplied ^ rol64(multiplied, 23))


def calculate_v298(normalized_name: str) -> int:
    """
    정규화된 이름 전체로 v298을 계산한다.

    IDA는 최적화 때문에 두 바이트씩 처리하지만,
    각 바이트마다 수행되는 상태 갱신을 순서대로 풀면
    아래 반복문과 동일하다.
    """
    current_hash = NAME_HASH_INITIAL

    for byte in normalized_name.encode("ascii"):
        current_hash = update_name_hash(current_hash, byte)

    return current_hash


def derive_v419(v298: int) -> tuple[int, int, int]:
    """
    IDA 데이터 흐름:

        v330 = v298 ^ (v298 >> 29);
        v334 = 0x9E3779B185EBCA87 * v330;
        v419 = v334 ^ HIDWORD(v334);

    HIDWORD(v334)는 상위 32비트이므로 Python에서는 v334 >> 32로 표현한다.

    반환:
        (v330, v334, v419)
    """
    v330 = u64(v298 ^ (v298 >> 29))
    v334 = u64(V334_MULTIPLIER * v330)
    v419 = u64(v334 ^ (v334 >> 32))

    return v330, v334, v419


def xorshift64(state: int) -> int:
    """
    permutation 루프의 PRNG를 재현한다.

    IDA의 중첩 표현은 의미상 다음 세 단계다.

        state ^= state << 13
        state ^= state >> 7
        state ^= state << 17

    unsigned __int64 동작을 재현하기 위해 각 단계마다 64비트로 자른다.
    """
    state = u64(state ^ u64(state << 13))
    state = u64(state ^ (state >> 7))
    state = u64(state ^ u64(state << 17))
    return state


def shuffle_opcode_table(seed: int, trace: bool = False) -> tuple[int, ...]:
    """
    0x00~0x12 배열을 IDA와 같은 방식으로 섞는다.

    IDA의:

        *((_BYTE *)&v435 + v357 + 7)

    은 스택 배치상:

        table[v357 - 1]

    이다.

    따라서 의미상 Fisher-Yates shuffle:

        for size in range(19, 1, -1):
            state = xorshift64(state)
            index = state % size
            swap(table[size - 1], table[index])
    """
    state = u64(seed)
    table = list(INITIAL_OPCODE_TABLE)

    for size in range(len(table), 1, -1):
        state = xorshift64(state)
        selected_index = state % size
        last_index = size - 1

        if trace:
            print(
                f"size={size:2d} "
                f"state=0x{state:016X} "
                f"swap[{last_index:02d}]={table[last_index]:02X} "
                f"<-> [{selected_index:02d}]={table[selected_index]:02X}"
            )

        table[last_index], table[selected_index] = (
            table[selected_index],
            table[last_index],
        )

    return tuple(table)


def generate_perm1_from_name(name: str, trace: bool = False) -> NameState:
    """
    이름 하나를 입력받아 v298, v419, perm1까지 한 번에 계산한다.
    """
    normalized_name = normalize_name(name)
    normalized_bytes = normalized_name.encode("ascii")

    v298 = calculate_v298(normalized_name)
    v330, v334, v419 = derive_v419(v298)

    # IDA: v353 = v419 ^ 0x64C7920094671459;
    perm1_seed = u64(v419 ^ PERM1_SEED_XOR)
    perm1 = shuffle_opcode_table(perm1_seed, trace=trace)

    return NameState(
        original_name=name,
        normalized_name=normalized_name,
        normalized_bytes=normalized_bytes,
        v298=v298,
        v330=v330,
        v334=v334,
        v419=v419,
        perm1_seed=perm1_seed,
        perm1=perm1,
    )


def generate_perm2_seed(v419: int, text_section_hash: int) -> int:
    """
    향후 perm2 구현에서 사용할 seed 계산만 미리 제공한다.

    IDA:

        v355 = v419 ^ ((uint64_t)textSectionHash << 32);
        v359 = v355 ^ 0x53A9D1E794671459;

    textSectionHash는 32비트 값으로 제한한다.
    """
    text_section_hash &= 0xFFFFFFFF
    v355 = u64(v419 ^ (text_section_hash << 32))
    return u64(v355 ^ PERM2_SEED_XOR)


def generate_perm2(
    v419: int,
    text_section_hash: int,
    trace: bool = False,
) -> tuple[int, ...]:
    """
    perm2도 perm1과 같은 shuffle을 사용한다.

    현재 파일을 나중에 확장하기 쉽도록 이미 함수 형태로 제공한다.
    """
    seed = generate_perm2_seed(v419, text_section_hash)
    return shuffle_opcode_table(seed, trace=trace)


def invert_permutation(
    permutation: tuple[int, ...],
) -> tuple[int, ...]:
    """
    permutation의 역방향 표를 만든다.

    입력 permutation의 의미가 다음과 같다고 가정한다.

        permutation[logical_opcode] = encoded_opcode

    그러면 반환되는 inverse의 의미는 다음과 같다.

        inverse[encoded_opcode] = logical_opcode

    예:
        permutation[0x02] == 0x11 이라면
        inverse[0x11] == 0x02 가 된다.

    RVP4에서는 perm2 배열의 각 원소가 VM dispatch handler와 대응한다.

        perm2[handler_index] = 실제 비교되는 opcode 값

    따라서 VM bytecode에서 opcode 값 X를 읽었을 때 실제 handler 번호는:

        inverse_perm2[X]

    로 구할 수 있다.
    """
    size = len(permutation)
    inverse = [-1] * size

    for logical_opcode, encoded_opcode in enumerate(permutation):
        if not 0 <= encoded_opcode < size:
            raise ValueError(
                "permutation 원소가 유효 범위를 벗어났다: "
                f"index={logical_opcode}, value=0x{encoded_opcode:02X}"
            )

        if inverse[encoded_opcode] != -1:
            raise ValueError(
                "permutation에 중복 값이 존재한다: "
                f"0x{encoded_opcode:02X}"
            )

        inverse[encoded_opcode] = logical_opcode

    if any(value == -1 for value in inverse):
        raise ValueError("permutation에 빠진 opcode가 존재한다.")

    return tuple(inverse)


def compose_final_opcode_mapping(
    perm1: tuple[int, ...],
    perm2: tuple[int, ...],
) -> tuple[int, ...]:
    """
    RVP4의 최종 opcode 매핑을 계산한다.

    디컴파일 코드에서 확인한 실제 흐름:

    1. 원본/논리 bytecode opcode를 v367로 읽는다.

           v367 = byte_7FF68D6A6280[v365];

    2. perm1을 사용해 opcode를 다시 인코딩한다.

           byte_7FF68D6A6280[v366] = v436.m128i_u8[v367];

       의미:

           encoded_opcode = perm1[original_logical_opcode]

    3. VM 실행 루프는 해당 encoded_opcode를 읽는다.

           v384 = byte_7FF68D6A6280[v377];

    4. v384를 perm2의 각 원소와 비교한다.

           if (perm2[0] == v384) -> handler 0
           if (perm2[1] == v384) -> handler 1
           ...
           if (perm2[18] == v384) -> handler 18

       따라서 encoded_opcode가 어느 handler로 가는지는:

           handler_index = inverse_perm2[encoded_opcode]

       이다.

    최종 식:

        final_handler[original_opcode]
            = inverse_perm2[perm1[original_opcode]]

    반환값의 의미:

        result[original_opcode] = 실제 실행되는 VM handler 번호

    주의:
    - 이 함수가 반환하는 값은 opcode 바이트 자체가 아니라
      VM dispatch 체인에서 선택되는 handler의 논리 번호다.
    - 아직 각 handler의 의미(LOAD_IMM, XOR, ADD 등)를 이름으로
      붙인 것은 아니다.
    """
    if len(perm1) != len(perm2):
        raise ValueError(
            "perm1과 perm2의 길이가 다르다: "
            f"{len(perm1)} != {len(perm2)}"
        )

    inverse_perm2 = invert_permutation(perm2)

    return tuple(
        inverse_perm2[perm1[original_opcode]]
        for original_opcode in range(len(perm1))
    )


def format_final_mapping(
    perm1: tuple[int, ...],
    perm2: tuple[int, ...],
    final_mapping: tuple[int, ...],
) -> str:
    """
    사람이 검증하기 쉬운 표 형식으로 최종 매핑을 출력한다.

    각 행의 의미:

        원본 opcode
        -> perm1로 인코딩된 opcode
        -> perm2 역매핑으로 결정된 최종 handler

    예:

        0x02 -> 0x11 -> handler 0x11

    단, 마지막 handler 번호는 실제 opcode 바이트가 아니라
    디컴파일 dispatch 체인에서의 handler 인덱스다.
    """
    lines = [
        "[원본 논리 opcode -> perm1 인코딩 값 -> 최종 VM handler]",
    ]

    for original_opcode, final_handler in enumerate(final_mapping):
        encoded_opcode = perm1[original_opcode]
        lines.append(
            f"0x{original_opcode:02X} "
            f"-> 0x{encoded_opcode:02X} "
            f"-> handler 0x{final_handler:02X}"
        )

    return "\n".join(lines)


def format_bytes(values: tuple[int, ...]) -> str:
    return " ".join(f"{value:02X}" for value in values)


def run_self_test() -> None:
    """
    현재까지 x64dbg로 확인한 Tomas 사례 검증.

    Tomas -> tomas
    v419 = 0x0B5860F65C1EC6FC

    이 값이 다르면 이름 해시 또는 v419 계산 구현이 잘못된 것이다.
    """
    state = generate_perm1_from_name("Tomas")
    expected_v419 = 0x0B5860F65C1EC6FC

    if state.v419 != expected_v419:
        raise AssertionError(
            "자체 검증 실패: "
            f"예상 v419=0x{expected_v419:016X}, "
            f"계산값=0x{state.v419:016X}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="RVP4 이름 기반 v419와 opcode permutation 계산"
    )
    parser.add_argument(
        "name",
        nargs="?",
        help='분석할 이름. 공백이 있으면 따옴표로 감싼다. 예: "John Smith"',
    )
    parser.add_argument(
        "--trace",
        action="store_true",
        help="permutation의 각 교환 과정을 출력",
    )
    parser.add_argument(
        "--text-hash",
        type=lambda value: int(value, 16),
        help=(
            "32비트 textSectionHash를 주면 perm2도 함께 계산한다. "
            "예: --text-hash 19F8806B"
        ),
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="Tomas 사례로 구현 정확성을 검사",
    )
    parser.add_argument(
        "--final-map",
        action="store_true",
        help=(
            "perm1과 perm2를 합성해 "
            "'원본 논리 opcode -> 최종 VM handler' 매핑을 출력한다. "
            "이 옵션은 --text-hash와 함께 사용해야 한다."
        ),
    )

    args = parser.parse_args()

    try:
        if args.final_map and args.text_hash is None:
            raise ValueError(
                "--final-map을 사용하려면 perm2 계산에 필요한 "
                "--text-hash도 함께 지정해야 한다."
            )
        if args.self_test:
            run_self_test()
            print("자체 검증 성공: Tomas의 v419가 x64dbg 값과 일치한다.")

            if args.name is None:
                return 0

        if args.name is None:
            parser.error("name 인수가 필요하다.")

        state = generate_perm1_from_name(args.name, trace=args.trace)

        print(f"원본 이름      : {state.original_name!r}")
        print(f"정규화 이름    : {state.normalized_name!r}")
        print(f"정규화 바이트  : {state.normalized_bytes.hex(' ').upper()}")
        print(f"v298            : 0x{state.v298:016X}")
        print(f"v330            : 0x{state.v330:016X}")
        print(f"v334            : 0x{state.v334:016X}")
        print(f"v419            : 0x{state.v419:016X}")
        print(f"perm1 seed      : 0x{state.perm1_seed:016X}")
        print(f"perm1           : {format_bytes(state.perm1)}")

        print("\n[논리 opcode -> perm1 opcode]")
        for logical_opcode, encoded_opcode in enumerate(state.perm1):
            print(
                f"0x{logical_opcode:02X} -> 0x{encoded_opcode:02X}"
            )

        if args.text_hash is not None:
            if not 0 <= args.text_hash <= 0xFFFFFFFF:
                raise ValueError("textSectionHash는 32비트 범위여야 한다.")

            v355 = u64(
                state.v419 ^ ((args.text_hash & 0xFFFFFFFF) << 32)
            )

            perm2_seed = generate_perm2_seed(state.v419, args.text_hash)
            perm2 = generate_perm2(
                state.v419,
                args.text_hash,
                trace=args.trace,
            )

            print(f"v355            : 0x{v355:016X}")
            print(f"\ntextSectionHash : 0x{args.text_hash:08X}")
            print(f"perm2 seed      : 0x{perm2_seed:016X}")
            print(f"perm2           : {format_bytes(perm2)}")

            if args.final_map:
                inverse_perm2 = invert_permutation(perm2)
                final_mapping = compose_final_opcode_mapping(
                    state.perm1,
                    perm2,
                )

                print(
                    f"inverse perm2   : "
                    f"{format_bytes(inverse_perm2)}"
                )
                print()
                print(
                    format_final_mapping(
                        state.perm1,
                        perm2,
                        final_mapping,
                    )
                )

        return 0

    except (ValueError, AssertionError) as exc:
        print(f"오류: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())