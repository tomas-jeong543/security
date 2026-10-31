#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EASILE Release EXE/PDB CRC32 updater (Windows, Python 3.10+, standard library only).

Examples (run from build\\Release):
    py -3 easile_crc_auto.py --exe GameClient.exe --pdb GameClient.pdb
    py -3 easile_crc_auto.py --exe GameClient.exe --pdb GameClient.pdb --source ..\\..\\src\\IntegrityGuard.cpp --check-source
    py -3 easile_crc_auto.py --exe GameClient.exe --pdb GameClient.pdb --source ..\\..\\src\\IntegrityGuard.cpp --write-source

IMPORTANT: The EXE and PDB MUST come from the same trusted, clean build. This reads
CRC values from EXE bytes, NOT from a potentially injected running process.
--write-source updates the specified C++ file (creating a .bak backup). A re-link
might change function locations/contents; run --check-source on the FINAL build.
"""

import argparse
import os
from pathlib import Path
import re
import shutil
import struct
import sys
import uuid
import zlib

TARGETS = {
    'GameWorld::Update': ('UPDATE_RVA', 'UPDATE_SIZE', 'EXPECTED_UPDATE_CRC'),
    'GameWorld::Initialize': ('HERO_RVA', 'HERO_SIZE', 'EXPECTED_HERO_CRC'),
}


def u16(data, pos):
    return struct.unpack_from('<H', data, pos)[0]


def u32(data, pos):
    return struct.unpack_from('<I', data, pos)[0]


class PEImage:
    def __init__(self, path):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        data = self.data
        if len(data) < 512 or data[:2] != b'MZ':
            raise ValueError('Not a PE executable (missing MZ)')
        pe = u32(data, 0x3C)
        if data[pe:pe + 4] != b'PE\0\0':
            raise ValueError('Invalid PE signature')
        self.nsections = u16(data, pe + 6)
        optional_header = pe + 24
        if u16(data, optional_header) != 0x20B:
            raise ValueError('Expected a 64-bit PE32+ EXE')
        self.image_size = u32(data, optional_header + 56)
        section_table = optional_header + u16(data, pe + 20)
        self.sections = []
        for i in range(self.nsections):
            offset = section_table + i * 40
            name = data[offset:offset + 8].split(b'\0')[0].decode('ascii', 'replace')
            virtual_size, rva, raw_size, raw_ptr = struct.unpack_from('<IIII', data, offset + 8)
            self.sections.append(dict(name=name, rva=rva, vsize=virtual_size,
                                      raw_size=raw_size, raw_ptr=raw_ptr))
        self.debug_rva, self.debug_size = struct.unpack_from('<II', data, optional_header + 112 + 6 * 8)
        self.reloc_rva, self.reloc_size = struct.unpack_from('<II', data, optional_header + 112 + 5 * 8)

    def raw_offset(self, rva, size=1):
        for section in self.sections:
            start = section['rva']
            if start <= rva and rva + size <= start + section['raw_size']:
                offset = section['raw_ptr'] + rva - start
                if offset + size <= len(self.data):
                    return offset
        raise ValueError(f'RVA 0x{rva:X} + size 0x{size:X} is not file-backed in the EXE')

    def get_codeview(self):
        if not self.debug_rva or self.debug_size < 28:
            raise ValueError('EXE does not contain a PE Debug Directory')
        offset = self.raw_offset(self.debug_rva, self.debug_size)
        for i in range(self.debug_size // 28):
            item = struct.unpack_from('<IIHHIIII', self.data, offset + i * 28)
            typ, size, ptr = item[4], item[5], item[7]
            if typ == 2 and self.data[ptr:ptr + 4] == b'RSDS':
                guid = uuid.UUID(bytes_le=self.data[ptr + 4:ptr + 20])
                age = u32(self.data, ptr + 20)
                pdb_path = self.data[ptr + 24:ptr + size].split(b'\0')[0].decode('utf-8', 'replace')
                return guid, age, pdb_path
        raise ValueError('EXE has no RSDS CodeView information; cannot match its PDB')

    def relocated_code_addresses(self):
        if not self.reloc_rva or not self.reloc_size:
            return []
        start = self.raw_offset(self.reloc_rva, self.reloc_size)
        pos = start
        end = start + self.reloc_size
        result = []
        while pos + 8 <= end:
            page, block_size = struct.unpack_from('<II', self.data, pos)
            if block_size < 8 or block_size % 2 or pos + block_size > end:
                raise ValueError('Malformed PE base relocation block')
            for i in range(pos + 8, pos + block_size, 2):
                item = u16(self.data, i)
                kind = item >> 12
                if kind:
                    result.append((page + (item & 0xFFF), kind))
            pos += block_size
        return result


class PDB7:
    """Minimal MSF 7/PDB module-symbol reader: S_GPROC32, S_LPROC32."""
    def __init__(self, path):
        self.path = Path(path)
        buf = self.path.read_bytes()
        if not buf.startswith(b'Microsoft C/C++ MSF 7.00'):
            raise ValueError('Not an MSF 7.00 PDB')
        page_size = u32(buf, 32)
        directory_size = u32(buf, 44)
        map_page = u32(buf, 52)
        if page_size < 512 or page_size > 65536 or page_size & (page_size - 1):
            raise ValueError('Unexpected PDB page size')

        def page(index):
            begin = index * page_size
            end = begin + page_size
            if end > len(buf):
                raise ValueError('PDB page index is out of range')
            return buf[begin:end]

        dir_count = (directory_size + page_size - 1) // page_size
        # MSF page-map entries are packed contiguously (sufficient for ordinary MSVC PDBs).
        map_bytes = b''.join(page(map_page + i) for i in range((dir_count * 4 + page_size - 1) // page_size))
        dir_pages = struct.unpack_from(f'<{dir_count}I', map_bytes, 0)
        directory = b''.join(page(i) for i in dir_pages)[:directory_size]
        count = u32(directory, 0)
        sizes = struct.unpack_from(f'<{count}I', directory, 4)
        pos = 4 + 4 * count
        self.streams = []
        for size in sizes:
            if size == 0xFFFFFFFF:
                self.streams.append(None)
                continue
            blocks = (size + page_size - 1) // page_size
            indices = struct.unpack_from(f'<{blocks}I', directory, pos)
            pos += 4 * blocks
            self.streams.append(b''.join(page(i) for i in indices)[:size])
        info = self.streams[1]
        if info is None or len(info) < 28:
            raise ValueError('Missing PDB info stream')
        self.age = u32(info, 8)
        self.guid = uuid.UUID(bytes_le=info[12:28])

    def functions(self, expected_names):
        dbi = self.streams[3]
        if dbi is None or len(dbi) < 64:
            raise ValueError('Missing DBI stream')
        module_info_size = struct.unpack_from('<i', dbi, 24)[0]
        if module_info_size < 0 or 64 + module_info_size > len(dbi):
            raise ValueError('Invalid PDB module-information length')
        end = 64 + module_info_size
        pos = 64
        matches = {name: [] for name in expected_names}
        while pos < end:
            if pos + 64 > end:
                raise ValueError('Truncated PDB module-information entry')
            symstream = u16(dbi, pos + 34)
            symbytes = u32(dbi, pos + 36)
            pos += 64
            module_end = dbi.find(b'\0', pos, end)
            if module_end < 0:
                raise ValueError('Missing PDB module name')
            module_name = dbi[pos:module_end].decode('utf-8', 'replace')
            pos = module_end + 1
            obj_end = dbi.find(b'\0', pos, end)
            if obj_end < 0:
                raise ValueError('Missing PDB object name')
            pos = (obj_end + 4) & ~3

            if symstream == 0xFFFF or symstream >= len(self.streams):
                continue
            symbols = self.streams[symstream]
            if symbols is None or symbytes > len(symbols):
                continue
            offset = 4  # C13 module-symbol signature
            while offset + 4 <= symbytes:
                length = u16(symbols, offset)
                kind = u16(symbols, offset + 2)
                record_end = offset + 2 + length
                if length < 2 or record_end > symbytes:
                    break
                if kind in (0x110F, 0x1110) and length >= 2 + 35:
                    body = symbols[offset + 4:record_end]
                    name = body[35:].split(b'\0')[0].decode('utf-8', 'replace')
                    if name in matches:
                        code_length = u32(body, 12)
                        section_offset = u32(body, 28)
                        segment = u16(body, 32)
                        matches[name].append((segment, section_offset, code_length, module_name))
                offset = record_end
        for name, results in matches.items():
            if len(results) != 1:
                raise ValueError(f'Expected exactly one PDB function {name!r}, found {len(results)}: {results}')
        return {name: values[0] for name, values in matches.items()}


def get_values(pe, pdb):
    exe_guid, exe_age, _ = pe.get_codeview()
    if (exe_guid, exe_age) != (pdb.guid, pdb.age):
        raise ValueError('EXE and PDB do NOT match (GUID/Age mismatch). Rebuild Release before measuring.')
    procs = pdb.functions(TARGETS)
    relocations = pe.relocated_code_addresses()
    result = {}
    for name, (segment, code_offset, length, _module) in procs.items():
        if segment < 1 or segment > len(pe.sections):
            raise ValueError(f'Invalid PE section in PDB for {name}: {segment}')
        rva = pe.sections[segment - 1]['rva'] + code_offset
        offset = pe.raw_offset(rva, length)
        hits = [(addr, kind) for addr, kind in relocations if rva <= addr < rva + length]
        if hits:
            raise ValueError(f'{name}: base relocation in protected code ({hits}); EXE CRC may differ from running memory')
        crc = zlib.crc32(pe.data[offset:offset + length]) & 0xFFFFFFFF
        result[name] = (rva, length, crc)
    return result, exe_guid, exe_age


def read_source_values(source):
    original = source.read_bytes()
    is_bom = original.startswith(b'\xef\xbb\xbf')
    text = original.decode('utf-8-sig')
    return original, text, is_bom


def source_pattern(var):
    return re.compile(r'(\bconstexpr\s+(?:uintptr_t|size_t|uint32_t)\s+' + re.escape(var) +
                      r'\s*=\s*)0[xX][0-9a-fA-F]+(\s*;)')


def update_or_check_source(source, values, write):
    original, text, bom = read_source_values(source)
    mismatches = []
    for name, (rva, length, crc) in values.items():
        for variable, value in zip(TARGETS[name], (rva, length, crc)):
            pattern = source_pattern(variable)
            matches = pattern.findall(text)
            if len(matches) != 1:
                raise ValueError(f'{source}: expected exactly one declaration of {variable}, found {len(matches)}')
            old = re.search(r'\b' + re.escape(variable) + r'\s*=\s*(0[xX][0-9a-fA-F]+)', text)
            if old and int(old.group(1), 16) != value:
                mismatches.append((variable, int(old.group(1), 16), value))
            replacement = f'0x{value:08X}' if 'CRC' in variable else f'0x{value:X}'
            text = pattern.sub(lambda m: m.group(1) + replacement + m.group(2), text)
    if not write:
        print('C++ source check:', 'MATCH' if not mismatches else 'MISMATCH')
        for variable, old, value in mismatches:
            print(f'  {variable}: source=0x{old:X}, EXE/PDB=0x{value:X}')
        return not mismatches
    newdata = (b'\xef\xbb\xbf' if bom else b'') + text.encode('utf-8')
    if newdata != original:
        backup = source.with_name(source.name + '.bak')
        shutil.copy2(source, backup)
        source.write_bytes(newdata)
        print(f'C++ constants updated: {source}\nBackup created: {backup}')
        for variable, old, value in mismatches:
            print(f'  {variable}: 0x{old:X} -> 0x{value:X}')
    else:
        print('C++ source already matches. No changes made.')
    return True


def main():
    ap = argparse.ArgumentParser(description='Automatically measure EASILE Update/Initialize RVA, SIZE and CRC using a matching EXE/PDB')
    ap.add_argument('--exe', default='GameClient.exe', help='Clean Release executable')
    ap.add_argument('--pdb', default='GameClient.pdb', help='Matching Release PDB')
    ap.add_argument('--source', help='Path to IntegrityGuard.cpp (optional)')
    group = ap.add_mutually_exclusive_group()
    group.add_argument('--write-source', action='store_true', help='Update IntegrityGuard.cpp constants; creates a .bak backup')
    group.add_argument('--check-source', action='store_true', help='Compare the C++ constants against EXE/PDB')
    args = ap.parse_args()
    if (args.write_source or args.check_source) and not args.source:
        ap.error('--source is required with --write-source or --check-source')
    pe, pdb = PEImage(args.exe), PDB7(args.pdb)
    values, guid, age = get_values(pe, pdb)
    print('EXE:', pe.path.resolve())
    print('PDB:', pdb.path.resolve())
    print(f'Matching PDB: OK (GUID={guid}, Age={age})')
    print(f'PE Image Size: 0x{pe.image_size:X}')
    print('CRC source: EXE on disk (not running process memory)')
    for name, (rva, size, crc) in values.items():
        print(f'{name}: RVA=0x{rva:X}, SIZE=0x{size:X} ({size} bytes), CRC32=0x{crc:08X}')
    print('\nC++ settings:')
    for name, (rva, size, crc) in values.items():
        a, b, c = TARGETS[name]
        print(f'constexpr uintptr_t {a} = 0x{rva:X};')
        print(f'constexpr size_t {b} = 0x{size:X};')
        print(f'constexpr uint32_t {c} = 0x{crc:08X};')
    if args.check_source or args.write_source:
        ok = update_or_check_source(Path(args.source), values, args.write_source)
        if not ok:
            return 2
    if args.write_source:
        print('\nIMPORTANT: Rebuild Release after editing C++ constants, then rerun with --check-source.')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, struct.error, UnicodeError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        sys.exit(1)
