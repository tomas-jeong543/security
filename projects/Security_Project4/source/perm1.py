#!/usr/bin/env python3
"""
perm1.py

RVP4의 첫 번째 opcode permutation(perm1)을 생성한다.
실행시에는 v419값 즉 permutationNameState값을 입력으로 넣으면 된다 python perm1.py 123456789ABCDEF0과 같이 말이다.
IDA 디컴파일의 핵심 흐름:

    v353 = v419 ^ 0x64C7920094671459LL;

    v436 = si128;   # 00~0F
    v437 = 4368;    # 메모리상 10 11
    v438 = 18;      # 12
    v357 = 19;

    do {
        v353 ^= ...xorshift64...;
        v358 = *((_BYTE *)&v435 + v357 + 7);
        *((_BYTE *)&v435 + v357 + 7) = v436.m128i_i8[v353 % v357];
        v436.m128i_i8[v353 % v357] = v358;
        v357--;
    } while (v357 != 1);

스택 배치를 계산하면

    *((_BYTE *)&v435 + v357 + 7)

은 19바이트 배열의 table[v357 - 1]을 가리킨다.
따라서 의미상 Fisher-Yates 셔플이다.
"""

from __future__ import annotations

import argparse

MASK64 = 0xFFFFFFFFFFFFFFFF
PERM1_SEED_XOR = 0x64C7920094671459
INITIAL_OPCODE_TABLE = list(range(0x13))  # 00~12, 총 19개


def u64(value: int) -> int:
    """C의 unsigned __int64처럼 결과를 64비트로 제한한다."""
    return value & MASK64


def xorshift64(state: int) -> int:
    """
    IDA의 중첩된 식을 순서가 드러나게 복원한 함수다.

        state ^= state << 13
        state ^= state >> 7
        state ^= state << 17

    각 단계마다 64비트 잘림을 적용해야 실제 C 동작과 같다.
    """
    state = u64(state ^ u64(state << 13))
    state = u64(state ^ (state >> 7))
    state = u64(state ^ u64(state << 17))
    return state


def format_table(table: list[int]) -> str:
    """바이트 배열을 보기 쉬운 16진수 문자열로 바꾼다."""
    return " ".join(f"{value:02X}" for value in table)


def generate_perm1(v419: int, trace: bool = False) -> tuple[list[int], int]:
    """IDA의 v419 값을 입력받아 perm1을 만든다."""

    # IDA: v353 = v419 ^ 0x64C7920094671459LL;
    state = u64(v419 ^ PERM1_SEED_XOR)

    # 초기 배열은 논리 opcode 00~12가 순서대로 들어 있다.
    table = INITIAL_OPCODE_TABLE.copy()

    if trace:
        print(f"v419       = 0x{v419:016X}")
        print(f"초기 state = 0x{state:016X}")
        print(f"초기 table = {format_table(table)}\n")

    # IDA의 v357은 19에서 시작하여 2까지 감소한다.
    for size in range(19, 1, -1):
        # 매 반복마다 PRNG 상태를 갱신한다.
        state = xorshift64(state)

        # 현재 유효 범위 0~size-1에서 교환 상대를 고른다.
        selected_index = state % size

        # *((BYTE *)&v435 + v357 + 7)는 table[v357 - 1]이다.
        last_index = size - 1

        if trace:
            print(
                f"size={size:2d}  state=0x{state:016X}  "
                f"swap table[{last_index:2d}]={table[last_index]:02X} "
                f"<-> table[{selected_index:2d}]={table[selected_index]:02X}"
            )

        # 실제 swap 수행
        table[last_index], table[selected_index] = (
            table[selected_index],
            table[last_index],
        )

        if trace:
            print(f"           table = {format_table(table)}")

    return table, state


def invert_permutation(permutation: list[int]) -> list[int]:
    """
    역매핑을 만든다.

    perm1[logical] = encoded 라면
    inverse[encoded] = logical 이다.
    """
    inverse = [0] * len(permutation)
    for logical, encoded in enumerate(permutation):
        inverse[encoded] = logical
    return inverse


def parse_u64(value: str) -> int:
    """0x 접두사 유무와 관계없이 64비트 16진수를 읽는다."""
    cleaned = value.strip().lower()
    if cleaned.startswith("0x"):
        cleaned = cleaned[2:]

    try:
        result = int(cleaned, 16)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"올바른 16진수 값이 아님: {value}") from exc

    if not 0 <= result <= MASK64:
        raise argparse.ArgumentTypeError("입력값은 64비트 범위여야 함")

    return result


def main() -> int:
    parser = argparse.ArgumentParser(
        description="RVP4 첫 번째 opcode permutation 생성"
    )
    parser.add_argument(
        "v419",
        type=parse_u64,
        help="IDA의 v419 값. 예: 123456789ABCDEF0",
    )
    parser.add_argument(
        "--trace",
        action="store_true",
        help="각 반복의 state, 인덱스, swap 결과 출력",
    )
    parser.add_argument(
        "--inverse",
        action="store_true",
        help="실제 opcode에서 논리 opcode로 가는 역매핑도 출력",
    )
    args = parser.parse_args()

    perm1, final_state = generate_perm1(args.v419, trace=args.trace)

    print()
    print(f"v419                    : 0x{args.v419:016X}")
    print(f"seed XOR 상수           : 0x{PERM1_SEED_XOR:016X}")
    print(f"초기 permutation state : 0x{u64(args.v419 ^ PERM1_SEED_XOR):016X}")
    print(f"최종 PRNG state         : 0x{final_state:016X}")
    print(f"perm1                   : {format_table(perm1)}")

    print("\n[논리 opcode -> 실제 opcode]")
    for logical, encoded in enumerate(perm1):
        print(f"logical 0x{logical:02X} -> encoded 0x{encoded:02X}")

    if args.inverse:
        inverse = invert_permutation(perm1)
        print("\n[실제 opcode -> 논리 opcode]")
        for encoded, logical in enumerate(inverse):
            print(f"encoded 0x{encoded:02X} -> logical 0x{logical:02X}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
