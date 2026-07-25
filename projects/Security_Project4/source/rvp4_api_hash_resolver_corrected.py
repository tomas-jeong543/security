#!/usr/bin/env python3
"""
RVP4 API 해시 복원 도구

목적
----
IDA 디컴파일 코드에서 발견한 32비트 API 해시 상수를 실제 Windows API 이름으로 복원한다.

동작 방식
---------
1. kernel32.dll, KernelBase.dll, ntdll.dll 등의 PE 파일을 직접 연다.
2. PE Export Directory를 파싱한다.
3. DLL이 export하는 모든 함수 이름을 열거한다.
4. 각 export 이름에 RVP4의 커스텀 해시 알고리즘을 적용한다.
5. 사용자가 입력한 목표 해시와 일치하는 API 이름을 출력한다.
6. ordinal, RVA, forwarded export 문자열도 함께 보여준다.

중요
----
이 도구는 해시를 수학적으로 역산하지 않는다.
Windows DLL이 가진 export 이름 전체를 후보로 삼아 같은 해시를 계산한 뒤,
목표값과 일치하는 이름을 찾는 사전 대입 방식이다.

예시
----
python rvp4_api_hash_resolver_commented.py 0x7A130126
python rvp4_api_hash_resolver_commented.py --name GetStdHandle
python rvp4_api_hash_resolver_commented.py --hash-file hashes.txt
"""


from __future__ import annotations

import argparse
import os
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional


# RVP4 해시 초기값과 고정 상수
FNV_OFFSET_BASIS_32 = 0x811C9DC5
FNV_PRIME_32 = 0x01000193
HASH_XOR_CONSTANT = 0x9E3779B9


# PE 파일 형식이 잘못됐을 때 사용하는 전용 예외
class PEFormatError(ValueError):
    pass


# 32비트 왼쪽 순환 회전. IDA의 __ROL4__와 동일한 역할
def rol32(value: int, count: int) -> int:
    value &= 0xFFFFFFFF
    count &= 31
    return ((value << count) | (value >> (32 - count))) & 0xFFFFFFFF


# RVP4 실행파일이 export 이름에 적용하는 실제 API 해시 알고리즘
def rvp4_name_hash(name: str) -> int:
    """
    Hash an ASCII API/export name exactly like the IDA decompilation.

    Important:
        The export-name loop processes two characters at a time. It is NOT
        equivalent to applying the module-name hash step independently to
        every character.

    For each pair (c0, c1):
        pair_step = ROL32(P * (state ^ lower(c0)), 5)
        pair_out  = ROL32(P * (pair_step ^ lower(c1) ^ C), 5)
        state     = pair_out ^ C

    If the name length is odd, the final character is handled as:
        result = ROL32(P * (state ^ lower(last)), 5)

    If the name length is even, the compared result is the final pair_out,
    not state.
    """
    # API 이름을 ASCII 소문자로 정규화한 바이트 목록
    lowered: list[int] = []

    for ch in name:
        value = ord(ch)
        if value > 0x7F:
            raise ValueError(f"non-ASCII export name: {name!r}")

        if 0x41 <= value <= 0x5A:
            value += 0x20

        lowered.append(value)

    # 해시 상태값. IDA 코드의 -2128831035와 동일한 unsigned 값
    state = FNV_OFFSET_BASIS_32
    compared_result = state
    # 짝수 길이 부분까지만 2문자씩 처리한다
    pair_end = len(lowered) & ~1

    index = 0
    # 실제 디컴파일 코드처럼 두 문자를 한 묶음으로 처리
    while index < pair_end:
        char0 = lowered[index]
        char1 = lowered[index + 1]

        pair_step = rol32(
            ((state ^ char0) * FNV_PRIME_32) & 0xFFFFFFFF,
            5,
        )
        compared_result = rol32(
            ((pair_step ^ char1 ^ HASH_XOR_CONSTANT) * FNV_PRIME_32)
            & 0xFFFFFFFF,
            5,
        )
        state = compared_result ^ HASH_XOR_CONSTANT
        index += 2

    # 이름 길이가 홀수라면 마지막 한 문자를 별도 처리
    if len(lowered) & 1:
        compared_result = rol32(
            ((state ^ lowered[index]) * FNV_PRIME_32) & 0xFFFFFFFF,
            5,
        )

    return compared_result & 0xFFFFFFFF

# PE 섹션 헤더에서 RVA를 파일 오프셋으로 변환할 때 필요한 정보
@dataclass(frozen=True)
class Section:
    virtual_address: int
    virtual_size: int
    raw_offset: int
    raw_size: int

    def contains(self, rva: int) -> bool:
        return self.virtual_address <= rva < (
            self.virtual_address + max(self.virtual_size, self.raw_size)
        )


# IMAGE_EXPORT_DIRECTORY에서 사용하는 핵심 필드
@dataclass(frozen=True)
class ExportDirectory:
    rva: int
    size: int
    ordinal_base: int
    number_of_functions: int
    number_of_names: int
    address_of_functions: int
    address_of_names: int
    address_of_name_ordinals: int


# export 하나를 복원한 결과
@dataclass(frozen=True)
class ExportEntry:
    module_path: Path
    name: str
    name_hash: int
    ordinal: int
    function_rva: int
    forwarder: Optional[str]


# DLL 파일을 직접 읽고 PE 헤더 및 Export Directory를 파싱하는 클래스
class PEImage:
    def __init__(self, path: Path):
        self.path = path.resolve()
        try:
            self.data = self.path.read_bytes()
        except OSError as exc:
            raise PEFormatError(f"cannot read {self.path}: {exc}") from exc

        self.sections: list[Section] = []
        self.export: Optional[ExportDirectory] = None
        self._parse_headers()

    def _require(self, offset: int, size: int) -> None:
        if offset < 0 or size < 0 or offset + size > len(self.data):
            raise PEFormatError(
                f"{self.path.name}: file range 0x{offset:X}+0x{size:X} is invalid"
            )

    def _u16(self, offset: int) -> int:
        self._require(offset, 2)
        return struct.unpack_from("<H", self.data, offset)[0]

    def _u32(self, offset: int) -> int:
        self._require(offset, 4)
        return struct.unpack_from("<I", self.data, offset)[0]

    def _parse_headers(self) -> None:
        if len(self.data) < 0x40 or self.data[:2] != b"MZ":
            raise PEFormatError(f"{self.path.name}: missing MZ header")

        # DOS Header.e_lfanew를 통해 NT Header 위치로 이동
        pe_offset = self._u32(0x3C)
        self._require(pe_offset, 24)

        if self.data[pe_offset : pe_offset + 4] != b"PE\0\0":
            raise PEFormatError(f"{self.path.name}: missing PE signature")

        file_header = pe_offset + 4
        section_count = self._u16(file_header + 2)
        optional_size = self._u16(file_header + 16)
        optional = file_header + 20
        self._require(optional, optional_size)

        magic = self._u16(optional)
        if magic == 0x10B:
            number_of_directories_offset = optional + 92
            data_directories_offset = optional + 96
        elif magic == 0x20B:
            number_of_directories_offset = optional + 108
            data_directories_offset = optional + 112
        else:
            raise PEFormatError(
                f"{self.path.name}: unsupported optional header 0x{magic:04X}"
            )

        number_of_directories = self._u32(number_of_directories_offset)
        export_rva = 0
        export_size = 0

        if number_of_directories >= 1:
            export_rva = self._u32(data_directories_offset)
            export_size = self._u32(data_directories_offset + 4)

        section_table = optional + optional_size
        for index in range(section_count):
            header = section_table + index * 40
            self._require(header, 40)
            self.sections.append(
                Section(
                    virtual_address=self._u32(header + 12),
                    virtual_size=self._u32(header + 8),
                    raw_offset=self._u32(header + 20),
                    raw_size=self._u32(header + 16),
                )
            )

        # Export Directory가 없는 PE라면 종료
        if export_rva == 0 or export_size == 0:
            return

        export_offset = self.rva_to_offset(export_rva)
        self._require(export_offset, 40)

        self.export = ExportDirectory(
            rva=export_rva,
            size=export_size,
            ordinal_base=self._u32(export_offset + 16),
            number_of_functions=self._u32(export_offset + 20),
            number_of_names=self._u32(export_offset + 24),
            address_of_functions=self._u32(export_offset + 28),
            address_of_names=self._u32(export_offset + 32),
            address_of_name_ordinals=self._u32(export_offset + 36),
        )

    def rva_to_offset(self, rva: int) -> int:
        first_section_rva = min(
            (section.virtual_address for section in self.sections),
            default=len(self.data),
        )

        if 0 <= rva < first_section_rva:
            self._require(rva, 1)
            return rva

        for section in self.sections:
            if section.contains(rva):
                delta = rva - section.virtual_address
                if delta >= section.raw_size:
                    raise PEFormatError(
                        f"{self.path.name}: RVA 0x{rva:X} is virtual-only"
                    )
                offset = section.raw_offset + delta
                self._require(offset, 1)
                return offset

        raise PEFormatError(f"{self.path.name}: unmapped RVA 0x{rva:X}")

    def read_u16_rva(self, rva: int) -> int:
        return self._u16(self.rva_to_offset(rva))

    def read_u32_rva(self, rva: int) -> int:
        return self._u32(self.rva_to_offset(rva))

    def read_ascii_rva(self, rva: int, max_length: int = 4096) -> str:
        offset = self.rva_to_offset(rva)
        end_limit = min(len(self.data), offset + max_length)
        end = self.data.find(b"\0", offset, end_limit)

        if end < 0:
            raise PEFormatError(
                f"{self.path.name}: unterminated string at RVA 0x{rva:X}"
            )

        try:
            return self.data[offset:end].decode("ascii")
        except UnicodeDecodeError as exc:
            raise PEFormatError(
                f"{self.path.name}: non-ASCII string at RVA 0x{rva:X}"
            ) from exc

    def enumerate_named_exports(self) -> Iterable[ExportEntry]:
        export = self.export
        if export is None:
            return

        # AddressOfNames 배열을 순회하며 모든 named export를 읽는다
        for name_index in range(export.number_of_names):
            name_rva = self.read_u32_rva(export.address_of_names + name_index * 4)
            name = self.read_ascii_rva(name_rva)

            # 같은 name index를 AddressOfNameOrdinals에 적용
            ordinal_index = self.read_u16_rva(
                export.address_of_name_ordinals + name_index * 2
            )
            if ordinal_index >= export.number_of_functions:
                continue

            # ordinal index로 AddressOfFunctions에서 실제 함수 RVA 획득
            function_rva = self.read_u32_rva(
                export.address_of_functions + ordinal_index * 4
            )
            ordinal = export.ordinal_base + ordinal_index

            forwarder = None
            # 함수 RVA가 Export Directory 내부면 코드가 아니라 forwarder 문자열
            if export.rva <= function_rva < export.rva + export.size:
                try:
                    forwarder = self.read_ascii_rva(function_rva)
                except PEFormatError:
                    forwarder = "<invalid forwarder>"

            # export 이름을 RVP4 방식으로 해시
            try:
                value = rvp4_name_hash(name)
            except ValueError:
                continue

            yield ExportEntry(
                module_path=self.path,
                name=name,
                name_hash=value,
                ordinal=ordinal,
                function_rva=function_rva,
                forwarder=forwarder,
            )


# 0x12345678 또는 1234 형식의 사용자 입력을 32비트 정수로 변환
def parse_hash(text: str) -> int:
    cleaned = text.strip().rstrip(",;")
    value = int(cleaned, 0)
    if not 0 <= value <= 0xFFFFFFFF:
        raise ValueError(f"not a 32-bit value: {text}")
    return value


# 여러 해시값을 텍스트 파일에서 한 번에 불러온다
def load_hash_file(path: Path) -> list[int]:
    values: list[int] = []

    for line_number, line in enumerate(
        path.read_text(encoding="utf-8-sig").splitlines(),
        start=1,
    ):
        content = line.split("#", 1)[0].strip()
        if not content:
            continue

        for token in content.replace(",", " ").split():
            try:
                values.append(parse_hash(token))
            except ValueError as exc:
                raise ValueError(
                    f"{path}:{line_number}: invalid hash {token!r}"
                ) from exc

    return values


# 별도 --dll 옵션이 없을 때 검사할 기본 Windows DLL 목록
def default_dlls() -> list[Path]:
    if os.name != "nt":
        return []

    system_root = Path(os.environ.get("SystemRoot", r"C:\Windows"))
    system32 = system_root / "System32"

    return [
        system32 / "kernel32.dll",
        system32 / "kernelbase.dll",
        system32 / "ntdll.dll",
        system32 / "advapi32.dll",
        system32 / "user32.dll",
        system32 / "win32u.dll",
    ]


def unique_paths(paths: Iterable[Path]) -> list[Path]:
    result: list[Path] = []
    seen: set[Path] = set()

    for path in paths:
        resolved = path.resolve()
        if resolved not in seen:
            result.append(resolved)
            seen.add(resolved)

    return result


# 지정된 DLL들의 named export를 모두 수집하고 각 이름의 RVP4 해시를 계산
def collect_exports(dll_paths: Iterable[Path]) -> list[ExportEntry]:
    exports: list[ExportEntry] = []

    for dll_path in dll_paths:
        if not dll_path.is_file():
            print(f"[skip] not found: {dll_path}", file=sys.stderr)
            continue

        try:
            image = PEImage(dll_path)
            module_exports = list(image.enumerate_named_exports())
            exports.extend(module_exports)
            print(
                f"[loaded] {dll_path.name}: {len(module_exports)} named exports",
                file=sys.stderr,
            )
        except (OSError, PEFormatError) as exc:
            print(f"[skip] {dll_path}: {exc}", file=sys.stderr)

    return exports


# 해시가 일치한 API의 DLL, 이름, ordinal, RVA, forwarder를 출력
def print_match(entry: ExportEntry) -> None:
    print(f"0x{entry.name_hash:08X} -> {entry.module_path.name}!{entry.name}")
    print(f"  path       : {entry.module_path}")
    print(f"  ordinal    : {entry.ordinal}")
    print(f"  RVA        : 0x{entry.function_rva:08X}")
    if entry.forwarder:
        print(f"  forwarder  : {entry.forwarder}")


# 명령행 인자 정의
def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Resolve RVP4 API hash constants using PE export names"
    )
    parser.add_argument(
        "hashes",
        nargs="*",
        help="target hashes, for example 0x12345678",
    )
    parser.add_argument(
        "--hash-file",
        type=Path,
        help="text file containing whitespace/comma-separated hashes",
    )
    parser.add_argument(
        "--dll",
        action="append",
        type=Path,
        default=[],
        help="DLL to inspect; repeatable. Defaults to common Windows DLLs.",
    )
    parser.add_argument(
        "--dump-common",
        action="store_true",
        help="print every export and its calculated hash",
    )
    parser.add_argument(
        "--name",
        action="append",
        default=[],
        help="calculate the hash of a known API name; repeatable",
    )
    return parser


# 전체 실행 흐름: 입력 해시 수집 → DLL export 열거 → 해시 인덱스 생성 → 결과 출력
def main() -> int:
    args = build_parser().parse_args()

    for name in args.name:
        print(f"{name} -> 0x{rvp4_name_hash(name):08X}")

    target_hashes: list[int] = []

    try:
        target_hashes.extend(parse_hash(item) for item in args.hashes)
        if args.hash_file:
            target_hashes.extend(load_hash_file(args.hash_file))
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    target_hashes = list(dict.fromkeys(target_hashes))

    if not target_hashes and not args.dump_common:
        if args.name:
            return 0
        print(
            "error: provide at least one hash, --hash-file, --name, or --dump-common",
            file=sys.stderr,
        )
        return 2

    dll_paths = unique_paths(args.dll or default_dlls())

    if not dll_paths:
        print(
            "error: no DLL paths available; specify --dll <path>",
            file=sys.stderr,
        )
        return 2

    exports = collect_exports(dll_paths)
    if not exports:
        print("error: no exports were loaded", file=sys.stderr)
        return 1

    if args.dump_common:
        for entry in sorted(
            exports,
            key=lambda item: (item.module_path.name.lower(), item.name.lower()),
        ):
            suffix = f" -> {entry.forwarder}" if entry.forwarder else ""
            print(
                f"0x{entry.name_hash:08X} "
                f"{entry.module_path.name}!{entry.name}{suffix}"
            )

    if target_hashes:
        # 해시값 → 일치하는 export 목록 형태의 빠른 검색 인덱스 생성
        index: dict[int, list[ExportEntry]] = {}
        for entry in exports:
            index.setdefault(entry.name_hash, []).append(entry)

        unresolved = 0

        for target in target_hashes:
            matches = index.get(target, [])
            print()

            if not matches:
                print(f"0x{target:08X} -> NOT FOUND")
                unresolved += 1
                continue

            for entry in matches:
                print_match(entry)

        print()
        print(
            f"summary: {len(target_hashes) - unresolved}/{len(target_hashes)} "
            "hashes resolved"
        )

        return 1 if unresolved else 0

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
