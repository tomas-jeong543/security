#!/usr/bin/env python3
"""
RVP4 module-name-hash based PE export resolver.

Reconstructed behavior from resolveExportRva():

1. Module hash
   state = 0x811C9DC5
   for each ASCII character before the first '.':
       convert A-Z to a-z
       state = ROL32(((state ^ ch) * 0x01000193) & 0xffffffff, 5)
       state ^= 0x9E3779B9

2. Export resolution
   - resolves an export by exact ASCII name or ordinal (#123)
   - detects forwarded exports when the function RVA lies inside the
     IMAGE_EXPORT_DIRECTORY range
   - recursively resolves strings such as:
         KERNELBASE.Sleep
         NTDLL.#123
   - mirrors the sample's forwarder recursion guard:
         initial depth counter 0, +2 for each named forwarder,
         processing allowed while counter <= 4

This script parses PE files directly and has no third-party dependency.
"""

from __future__ import annotations

import argparse
import os
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional


FNV_OFFSET_BASIS_32 = 0x811C9DC5
FNV_PRIME_32 = 0x01000193
HASH_XOR_CONSTANT = 0x9E3779B9


class PEFormatError(ValueError):
    """Raised when a file is not a supported or valid PE image."""


def rol32(value: int, count: int) -> int:
    value &= 0xFFFFFFFF
    count &= 31
    return ((value << count) | (value >> (32 - count))) & 0xFFFFFFFF


def module_name_hash(name: str) -> int:
    """
    Hash a module name exactly like the decompiled loop.

    Hashing stops at the first '.', so all of these hash identically:
        kernel32
        kernel32.dll
        KERNEL32.DLL
    """
    state = FNV_OFFSET_BASIS_32

    for ch in name:
        if ch == ".":
            break

        code = ord(ch)
        if code > 0x7F:
            raise ValueError("module hash accepts ASCII module names only")

        if 0x41 <= code <= 0x5A:
            code += 0x20

        state = rol32(((state ^ code) * FNV_PRIME_32) & 0xFFFFFFFF, 5)
        state ^= HASH_XOR_CONSTANT

    return state & 0xFFFFFFFF


@dataclass(frozen=True)
class Section:
    virtual_address: int
    virtual_size: int
    raw_offset: int
    raw_size: int

    def contains_rva(self, rva: int) -> bool:
        mapped_size = max(self.virtual_size, self.raw_size)
        return self.virtual_address <= rva < self.virtual_address + mapped_size


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


@dataclass(frozen=True)
class ResolvedExport:
    module_path: Path
    module_hash: int
    query: str
    ordinal: int
    function_rva: int
    virtual_address: int
    forwarded_from: tuple[str, ...]


class PEImage:
    def __init__(self, path: Path):
        self.path = path.resolve()
        try:
            self.data = self.path.read_bytes()
        except OSError as exc:
            raise PEFormatError(f"cannot read {self.path}: {exc}") from exc

        self.image_base: int
        self.sections: list[Section]
        self.export: Optional[ExportDirectory]
        self._parse_headers()

    def _u16(self, offset: int) -> int:
        self._require(offset, 2)
        return struct.unpack_from("<H", self.data, offset)[0]

    def _u32(self, offset: int) -> int:
        self._require(offset, 4)
        return struct.unpack_from("<I", self.data, offset)[0]

    def _u64(self, offset: int) -> int:
        self._require(offset, 8)
        return struct.unpack_from("<Q", self.data, offset)[0]

    def _require(self, offset: int, size: int) -> None:
        if offset < 0 or size < 0 or offset + size > len(self.data):
            raise PEFormatError(
                f"{self.path.name}: offset 0x{offset:X} size 0x{size:X} is outside file"
            )

    def _parse_headers(self) -> None:
        if len(self.data) < 0x40 or self.data[:2] != b"MZ":
            raise PEFormatError(f"{self.path.name}: missing DOS MZ header")

        pe_offset = self._u32(0x3C)
        self._require(pe_offset, 24)

        if self.data[pe_offset : pe_offset + 4] != b"PE\0\0":
            raise PEFormatError(f"{self.path.name}: missing PE signature")

        file_header = pe_offset + 4
        number_of_sections = self._u16(file_header + 2)
        size_of_optional_header = self._u16(file_header + 16)
        optional_header = file_header + 20
        self._require(optional_header, size_of_optional_header)

        magic = self._u16(optional_header)
        if magic == 0x10B:  # PE32
            self.image_base = self._u32(optional_header + 28)
            number_of_rva_and_sizes_offset = optional_header + 92
            data_directory_offset = optional_header + 96
        elif magic == 0x20B:  # PE32+
            self.image_base = self._u64(optional_header + 24)
            number_of_rva_and_sizes_offset = optional_header + 108
            data_directory_offset = optional_header + 112
        else:
            raise PEFormatError(
                f"{self.path.name}: unsupported optional-header magic 0x{magic:04X}"
            )

        number_of_rva_and_sizes = self._u32(number_of_rva_and_sizes_offset)

        export_rva = 0
        export_size = 0
        if number_of_rva_and_sizes >= 1:
            export_rva = self._u32(data_directory_offset)
            export_size = self._u32(data_directory_offset + 4)

        section_table = optional_header + size_of_optional_header
        self.sections = []

        for index in range(number_of_sections):
            section_header = section_table + index * 40
            self._require(section_header, 40)
            self.sections.append(
                Section(
                    virtual_address=self._u32(section_header + 12),
                    virtual_size=self._u32(section_header + 8),
                    raw_offset=self._u32(section_header + 20),
                    raw_size=self._u32(section_header + 16),
                )
            )

        if export_rva == 0 or export_size == 0:
            self.export = None
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
        # Header RVAs map directly to file offsets.
        first_section_rva = min(
            (section.virtual_address for section in self.sections),
            default=len(self.data),
        )
        if 0 <= rva < first_section_rva:
            self._require(rva, 1)
            return rva

        for section in self.sections:
            if section.contains_rva(rva):
                delta = rva - section.virtual_address
                if delta >= section.raw_size:
                    raise PEFormatError(
                        f"{self.path.name}: RVA 0x{rva:X} lies in a virtual-only area"
                    )
                offset = section.raw_offset + delta
                self._require(offset, 1)
                return offset

        raise PEFormatError(f"{self.path.name}: unmapped RVA 0x{rva:X}")

    def read_c_string(self, rva: int, max_length: int = 4096) -> str:
        offset = self.rva_to_offset(rva)
        end_limit = min(len(self.data), offset + max_length)
        terminator = self.data.find(b"\0", offset, end_limit)

        if terminator < 0:
            raise PEFormatError(
                f"{self.path.name}: unterminated ASCII string at RVA 0x{rva:X}"
            )

        try:
            return self.data[offset:terminator].decode("ascii")
        except UnicodeDecodeError as exc:
            raise PEFormatError(
                f"{self.path.name}: non-ASCII export string at RVA 0x{rva:X}"
            ) from exc

    def read_u16_rva(self, rva: int) -> int:
        return self._u16(self.rva_to_offset(rva))

    def read_u32_rva(self, rva: int) -> int:
        return self._u32(self.rva_to_offset(rva))

    def function_rva_by_ordinal(self, ordinal: int) -> tuple[int, int]:
        export = self._require_export()
        index = ordinal - export.ordinal_base

        if index < 0 or index >= export.number_of_functions:
            raise LookupError(
                f"{self.path.name}: ordinal {ordinal} is outside "
                f"[{export.ordinal_base}, "
                f"{export.ordinal_base + export.number_of_functions})"
            )

        function_rva = self.read_u32_rva(export.address_of_functions + index * 4)
        if function_rva == 0:
            raise LookupError(f"{self.path.name}: ordinal {ordinal} is not exported")

        return function_rva, ordinal

    def function_rva_by_name(self, name: str) -> tuple[int, int]:
        export = self._require_export()

        for index in range(export.number_of_names):
            name_rva = self.read_u32_rva(export.address_of_names + index * 4)
            if self.read_c_string(name_rva) != name:
                continue

            ordinal_index = self.read_u16_rva(
                export.address_of_name_ordinals + index * 2
            )
            if ordinal_index >= export.number_of_functions:
                raise PEFormatError(
                    f"{self.path.name}: invalid export ordinal index {ordinal_index}"
                )

            function_rva = self.read_u32_rva(
                export.address_of_functions + ordinal_index * 4
            )
            if function_rva == 0:
                raise LookupError(f"{self.path.name}: export {name!r} has RVA 0")

            return function_rva, export.ordinal_base + ordinal_index

        raise LookupError(f"{self.path.name}: export {name!r} was not found")

    def is_forwarder_rva(self, function_rva: int) -> bool:
        export = self._require_export()
        return export.rva <= function_rva < export.rva + export.size

    def _require_export(self) -> ExportDirectory:
        if self.export is None:
            raise LookupError(f"{self.path.name}: no export directory")
        return self.export


class ModuleRepository:
    """
    Finds modules by the reconstructed module-name hash.

    Search is intentionally bounded, like the decompiled PEB traversal.
    By default at most 64 candidate PE files are inspected.
    """

    def __init__(self, search_directories: Iterable[Path], scan_limit: int = 64):
        self.search_directories = [Path(path).resolve() for path in search_directories]
        self.scan_limit = scan_limit
        self._path_cache: dict[int, Path] = {}
        self._image_cache: dict[Path, PEImage] = {}

    def register(self, path: Path) -> PEImage:
        image = self.load(path)
        self._path_cache[module_name_hash(path.name)] = image.path
        return image

    def load(self, path: Path) -> PEImage:
        resolved = Path(path).resolve()
        image = self._image_cache.get(resolved)
        if image is None:
            image = PEImage(resolved)
            self._image_cache[resolved] = image
        return image

    def find_by_hash(self, wanted_hash: int) -> PEImage:
        wanted_hash &= 0xFFFFFFFF

        cached_path = self._path_cache.get(wanted_hash)
        if cached_path is not None:
            return self.load(cached_path)

        candidates_checked = 0
        extensions = {".dll", ".exe", ".sys", ".ocx", ".cpl"}

        for directory in self.search_directories:
            if not directory.is_dir():
                continue

            try:
                entries = sorted(directory.iterdir(), key=lambda item: item.name.lower())
            except OSError:
                continue

            for path in entries:
                if candidates_checked >= self.scan_limit:
                    raise LookupError(
                        f"module hash 0x{wanted_hash:08X} not found before "
                        f"the {self.scan_limit}-module scan limit"
                    )

                if not path.is_file() or path.suffix.lower() not in extensions:
                    continue

                candidates_checked += 1
                try:
                    candidate_hash = module_name_hash(path.name)
                except ValueError:
                    continue

                if candidate_hash == wanted_hash:
                    self._path_cache[wanted_hash] = path.resolve()
                    return self.load(path)

        raise LookupError(f"module hash 0x{wanted_hash:08X} was not found")

    def find_by_forwarder_name(self, module_token: str) -> PEImage:
        wanted_hash = module_name_hash(module_token)

        # Fast path for already loaded or indexed modules.
        try:
            return self.find_by_hash(wanted_hash)
        except LookupError:
            pass

        # API-set forwarders may not exist as ordinary files in System32.
        # On modern Windows they normally resolve through the API-set schema.
        if module_token.lower().startswith(("api-ms-win-", "ext-ms-win-")):
            raise LookupError(
                f"{module_token}: API-set schema mapping is required; "
                "this standalone file parser does not emulate ApiSetMap"
            )

        raise LookupError(
            f"forwarded module {module_token!r} "
            f"(hash 0x{wanted_hash:08X}) was not found"
        )


class ExportResolver:
    def __init__(self, repository: ModuleRepository):
        self.repository = repository

    def resolve_module_hash(
        self,
        module_hash_value: int,
        symbol: str,
        depth_counter: int = 0,
        chain: tuple[str, ...] = (),
    ) -> ResolvedExport:
        if depth_counter > 4:
            raise RecursionError("forwarded-export recursion guard exceeded")

        image = self.repository.find_by_hash(module_hash_value)
        return self._resolve_image(image, symbol, depth_counter, chain)

    def resolve_path(
        self,
        module_path: Path,
        symbol: str,
        depth_counter: int = 0,
        chain: tuple[str, ...] = (),
    ) -> ResolvedExport:
        if depth_counter > 4:
            raise RecursionError("forwarded-export recursion guard exceeded")

        image = self.repository.register(module_path)
        return self._resolve_image(image, symbol, depth_counter, chain)

    def _resolve_image(
        self,
        image: PEImage,
        symbol: str,
        depth_counter: int,
        chain: tuple[str, ...],
    ) -> ResolvedExport:
        if symbol.startswith("#"):
            try:
                ordinal = int(symbol[1:], 10)
            except ValueError as exc:
                raise ValueError(f"invalid ordinal syntax: {symbol!r}") from exc
            function_rva, ordinal = image.function_rva_by_ordinal(ordinal)
        else:
            function_rva, ordinal = image.function_rva_by_name(symbol)

        if image.is_forwarder_rva(function_rva):
            forwarder = image.read_c_string(function_rva)
            next_chain = chain + (f"{image.path.name}!{symbol} -> {forwarder}",)

            if "." not in forwarder:
                raise PEFormatError(
                    f"{image.path.name}: malformed forwarder string {forwarder!r}"
                )

            module_token, forwarded_symbol = forwarder.rsplit(".", 1)
            target_image = self.repository.find_by_forwarder_name(module_token)

            # Named forwarder branch in the supplied decompilation uses a5 += 2.
            next_depth = depth_counter + 2
            if next_depth > 4:
                raise RecursionError(
                    "forwarded-export recursion guard exceeded: "
                    + " | ".join(next_chain)
                )

            return self._resolve_image(
                target_image,
                forwarded_symbol,
                next_depth,
                next_chain,
            )

        return ResolvedExport(
            module_path=image.path,
            module_hash=module_name_hash(image.path.name),
            query=symbol,
            ordinal=ordinal,
            function_rva=function_rva,
            virtual_address=image.image_base + function_rva,
            forwarded_from=chain,
        )


def default_search_directories(extra: Iterable[str]) -> list[Path]:
    directories: list[Path] = [Path(item) for item in extra]

    if os.name == "nt":
        system_root = Path(os.environ.get("SystemRoot", r"C:\Windows"))
        directories.extend(
            [
                system_root / "System32",
                system_root / "SysWOW64",
                Path.cwd(),
            ]
        )
    else:
        directories.append(Path.cwd())

    unique: list[Path] = []
    seen: set[Path] = set()
    for directory in directories:
        resolved = directory.resolve()
        if resolved not in seen:
            unique.append(resolved)
            seen.add(resolved)
    return unique


def parse_int(text: str) -> int:
    return int(text, 0)


def print_result(result: ResolvedExport) -> None:
    print(f"module_path  : {result.module_path}")
    print(f"module_hash  : 0x{result.module_hash:08X}")
    print(f"query        : {result.query}")
    print(f"ordinal      : {result.ordinal}")
    print(f"function_rva : 0x{result.function_rva:08X}")
    print(f"image_base   : 0x{result.virtual_address - result.function_rva:X}")
    print(f"static_va    : 0x{result.virtual_address:X}")

    if result.forwarded_from:
        print("forward_chain:")
        for item in result.forwarded_from:
            print(f"  {item}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="RVP4 module-hash PE export resolver"
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    hash_parser = subparsers.add_parser("hash", help="calculate a module-name hash")
    hash_parser.add_argument("module_name")

    path_parser = subparsers.add_parser(
        "resolve-path", help="resolve an export from a specified PE file"
    )
    path_parser.add_argument("module_path", type=Path)
    path_parser.add_argument("symbol", help="export name or ordinal such as #123")
    path_parser.add_argument(
        "--search-dir",
        action="append",
        default=[],
        help="directory used to locate forwarded modules; repeatable",
    )
    path_parser.add_argument("--scan-limit", type=int, default=64)

    hash_resolve_parser = subparsers.add_parser(
        "resolve-hash", help="locate a module by hash and resolve its export"
    )
    hash_resolve_parser.add_argument("module_hash", type=parse_int)
    hash_resolve_parser.add_argument("symbol", help="export name or ordinal such as #123")
    hash_resolve_parser.add_argument(
        "--search-dir",
        action="append",
        default=[],
        help="directory containing candidate modules; repeatable",
    )
    hash_resolve_parser.add_argument("--scan-limit", type=int, default=64)

    return parser


def main(argv: Optional[list[str]] = None) -> int:
    args = build_parser().parse_args(argv)

    try:
        if args.command == "hash":
            value = module_name_hash(args.module_name)
            print(f"0x{value:08X}")
            return 0

        directories = default_search_directories(args.search_dir)
        repository = ModuleRepository(directories, scan_limit=args.scan_limit)
        resolver = ExportResolver(repository)

        if args.command == "resolve-path":
            result = resolver.resolve_path(args.module_path, args.symbol)
        else:
            result = resolver.resolve_module_hash(args.module_hash, args.symbol)

        print_result(result)
        return 0

    except (OSError, ValueError, LookupError, RecursionError, PEFormatError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
