#!/usr/bin/env python3
"""
hashfunc.py

RVP4 패치된 실행파일의 메모리상 .text 섹션 해시를 계산한다.

IDA 디컴파일 코드의 핵심 대응:

    v316 = .text의 VirtualSize
    v317 = .text의 VirtualAddress
    v318 = -1585668547          # 32비트로는 0xA17C9E3D
    v319 = 0
    v407 = v317

    do {
        v408 = v318 ^ imageBase[v407];

        v409 = (
            ((unsigned int)(73244475 * v408) >> 25)
            + 785358208 * v408
            + 2135587861
        ) ^ imageBase[v407 + 1];

        v318 = (
            785358208 * v409
            + ((73244475 * v409) >> 25)
            + 2135587861
        );

        v319 += 2;
        v407 += 2;
    } while ((v316 & 0xFFFFFFFE) != v319);

이 프로그램은 디스크상의 PE 파일을 읽지만, 원본 코드는 로드된 이미지의
ImageBaseAddress + VirtualAddress를 해시한다. 따라서 .text의 원시 파일 위치
PointerToRawData에서 데이터를 읽고, VirtualSize가 더 크면 나머지를 0으로
채워 메모리상 섹션을 재현한다.
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


MASK32 = 0xFFFFFFFF

# IDA: v318 = -1585668547;
INITIAL_HASH = (-1585668547) & MASK32  # 0xA17C9E3D

MUL_A = 73_244_475
MUL_B = 785_358_208
ADD_C = 2_135_587_861


@dataclass(frozen=True)
class Section:
    name: str
    virtual_size: int
    virtual_address: int
    raw_size: int
    raw_offset: int


def u32(value: int) -> int:
    """C의 unsigned int 연산처럼 결과를 32비트로 자른다."""
    return value & MASK32


def read_u16(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 2 > len(data):
        raise ValueError(f"잘못된 uint16 읽기 위치: 0x{offset:X}")
    return struct.unpack_from("<H", data, offset)[0]


def read_u32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise ValueError(f"잘못된 uint32 읽기 위치: 0x{offset:X}")
    return struct.unpack_from("<I", data, offset)[0]


def parse_sections(pe_data: bytes) -> list[Section]:
    """PE 헤더에서 섹션 테이블을 직접 읽는다. 외부 라이브러리는 필요 없다."""
    if len(pe_data) < 0x40 or pe_data[:2] != b"MZ":
        raise ValueError("유효한 DOS/PE 파일이 아니다: MZ 헤더가 없음")

    pe_offset = read_u32(pe_data, 0x3C)

    if pe_offset + 24 > len(pe_data):
        raise ValueError("PE 헤더 위치가 파일 범위를 벗어남")

    if pe_data[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise ValueError("유효한 PE 서명이 없음")

    file_header = pe_offset + 4
    number_of_sections = read_u16(pe_data, file_header + 2)
    size_of_optional_header = read_u16(pe_data, file_header + 16)

    # IMAGE_SECTION_HEADER 배열의 시작 위치
    section_table = file_header + 20 + size_of_optional_header

    sections: list[Section] = []

    for index in range(number_of_sections):
        offset = section_table + index * 40

        if offset + 40 > len(pe_data):
            raise ValueError("섹션 헤더가 파일 범위를 벗어남")

        raw_name = pe_data[offset : offset + 8]
        name = raw_name.split(b"\0", 1)[0].decode("ascii", errors="replace")

        # IMAGE_SECTION_HEADER 기준:
        # +0x08 Misc.VirtualSize
        # +0x0C VirtualAddress
        # +0x10 SizeOfRawData
        # +0x14 PointerToRawData
        virtual_size = read_u32(pe_data, offset + 8)
        virtual_address = read_u32(pe_data, offset + 12)
        raw_size = read_u32(pe_data, offset + 16)
        raw_offset = read_u32(pe_data, offset + 20)

        sections.append(
            Section(
                name=name,
                virtual_size=virtual_size,
                virtual_address=virtual_address,
                raw_size=raw_size,
                raw_offset=raw_offset,
            )
        )

    return sections


def get_loaded_section_bytes(pe_data: bytes, section: Section) -> bytes:
    """
    디스크상의 섹션을 메모리에 로드된 형태로 재구성한다.

    원본 해시 루프는 SizeOfRawData가 아니라 VirtualSize(v316)를 사용하며,
    ImageBaseAddress + VirtualAddress(v317)에서 읽는다.
    """
    if section.virtual_size == 0:
        return b""

    raw_start = section.raw_offset
    raw_end = raw_start + section.raw_size

    if raw_start > len(pe_data):
        raise ValueError(
            f"{section.name}의 PointerToRawData가 파일 범위를 벗어남: "
            f"0x{raw_start:X}"
        )

    # 파일 끝보다 raw_size가 크게 기록된 비정상 PE도 안전하게 처리한다.
    raw = pe_data[raw_start : min(raw_end, len(pe_data))]

    # 로더는 VirtualSize까지만 메모리상 섹션으로 사용한다.
    loaded = raw[: section.virtual_size]

    # VirtualSize가 원시 데이터보다 크면 나머지는 0으로 초기화된다.
    if len(loaded) < section.virtual_size:
        loaded += b"\x00" * (section.virtual_size - len(loaded))

    return loaded


def mix_word(value: int) -> int:
    """
    IDA 식:

        ((unsigned int)(73244475 * value) >> 25)
        + 785358208 * value
        + 2135587861

    모든 산술을 unsigned 32비트로 재현한다.
    """
    product_a = u32(MUL_A * value)
    shifted = product_a >> 25  # unsigned 값이므로 논리적 우측 시프트
    product_b = u32(MUL_B * value)

    return u32(shifted + product_b + ADD_C)


def rvp4_text_hash(text: bytes, trace: bool = False) -> int:
    """IDA의 2바이트 단위 .text 해시 루프를 그대로 재현한다."""
    current_hash = INITIAL_HASH
    processed = 0
    even_length = len(text) & 0xFFFFFFFE

    while processed != even_length:
        # IDA: v408 = v318 ^ imageBase[v407];
        first_mixed = u32(current_hash ^ text[processed])

        # IDA: v409 = mix(v408) ^ imageBase[v407 + 1];
        second_mixed = u32(mix_word(first_mixed) ^ text[processed + 1])

        # IDA: v318 = mix(v409);
        current_hash = mix_word(second_mixed)

        if trace:
            print(
                f"[0x{processed:08X}] "
                f"b0={text[processed]:02X} "
                f"b1={text[processed + 1]:02X} "
                f"v408={first_mixed:08X} "
                f"v409={second_mixed:08X} "
                f"hash={current_hash:08X}"
            )

        processed += 2

    # VirtualSize가 홀수인 경우 마지막 1바이트 처리
    if len(text) & 1:
        last_mixed = u32(current_hash ^ text[processed])
        current_hash = mix_word(last_mixed)

        if trace:
            print(
                f"[0x{processed:08X}] "
                f"last={text[processed]:02X} "
                f"v410={last_mixed:08X} "
                f"hash={current_hash:08X}"
            )

    return current_hash


def parse_expected_hash(value: str) -> int:
    """EF424767, 0xEF424767 같은 입력을 모두 허용한다."""
    cleaned = value.strip().lower()
    if cleaned.startswith("0x"):
        cleaned = cleaned[2:]

    try:
        result = int(cleaned, 16)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            f"올바른 16진수 해시가 아님: {value}"
        ) from exc

    if not 0 <= result <= MASK32:
        raise argparse.ArgumentTypeError("해시는 32비트 범위여야 함")

    return result


def main() -> int:
    parser = argparse.ArgumentParser(
        description="RVP4 패치된 PE의 메모리상 .text 섹션 해시 계산"
    )
    parser.add_argument("exe", type=Path, help="분석할 패치된 EXE 파일")
    parser.add_argument(
        "--expected",
        type=parse_expected_hash,
        help="x64dbg에서 확인한 값과 비교 (예: EF424767)",
    )
    parser.add_argument(
        "--trace",
        action="store_true",
        help="각 2바이트 처리 단계의 중간값 출력",
    )
    parser.add_argument(
        "--list-sections",
        action="store_true",
        help="PE 섹션 목록도 출력",
    )

    args = parser.parse_args()

    try:
        pe_data = args.exe.read_bytes()
        sections = parse_sections(pe_data)

        if args.list_sections:
            print("[PE sections]")
            for section in sections:
                print(
                    f"{section.name:<8} "
                    f"VA=0x{section.virtual_address:08X} "
                    f"VS=0x{section.virtual_size:08X} "
                    f"RAW=0x{section.raw_offset:08X} "
                    f"RS=0x{section.raw_size:08X}"
                )
            print()

        text_section = next(
            (section for section in sections if section.name == ".text"),
            None,
        )

        if text_section is None:
            raise ValueError(".text 섹션을 찾지 못함")

        loaded_text = get_loaded_section_bytes(pe_data, text_section)
        result = rvp4_text_hash(loaded_text, trace=args.trace)

        print(f"파일                  : {args.exe}")
        print(f".text VirtualAddress  : 0x{text_section.virtual_address:08X}")
        print(f".text VirtualSize     : 0x{text_section.virtual_size:08X}")
        print(f".text RawOffset       : 0x{text_section.raw_offset:08X}")
        print(f".text RawSize         : 0x{text_section.raw_size:08X}")
        print(f"초기 해시             : 0x{INITIAL_HASH:08X}")
        print(f"최종 textSectionHash  : 0x{result:08X}")

        if args.expected is not None:
            print(f"x64dbg 기대값         : 0x{args.expected:08X}")

            if result == args.expected:
                print("검증 결과              : 일치")
            else:
                print("검증 결과              : 불일치")
                return 2

        return 0

    except (OSError, ValueError, struct.error) as exc:
        print(f"오류: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
