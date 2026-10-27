#!/usr/bin/env python3
"""Read CRC32 from a running EASILE GameClient process (Windows x64).

Usage:
  py -3-64 measure_easile_memory_crc.py
  py -3-64 measure_easile_memory_crc.py --pid 1234
  py -3-64 measure_easile_memory_crc.py --name GameClient.exe
"""

import argparse
import ctypes
import os
import struct
import sys
import zlib
from ctypes import wintypes

REGIONS = (
    ("GameWorld::Update", 0x144C0, 0x13BB),
    ("GameWorld::Initialize", 0x10D70, 0x685),
)

if sys.platform == "win32":
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    ULONG_PTR = ctypes.c_uint64 if ctypes.sizeof(ctypes.c_void_p) == 8 else ctypes.c_uint32

    class PROCESSENTRY32W(ctypes.Structure):
        _fields_ = [
            ("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD),
            ("th32ProcessID", wintypes.DWORD), ("th32DefaultHeapID", ULONG_PTR),
            ("th32ModuleID", wintypes.DWORD), ("cntThreads", wintypes.DWORD),
            ("th32ParentProcessID", wintypes.DWORD), ("pcPriClassBase", wintypes.LONG),
            ("dwFlags", wintypes.DWORD), ("szExeFile", wintypes.WCHAR * 260),
        ]

    class MODULEENTRY32W(ctypes.Structure):
        _fields_ = [
            ("dwSize", wintypes.DWORD), ("th32ModuleID", wintypes.DWORD),
            ("th32ProcessID", wintypes.DWORD), ("GlblcntUsage", wintypes.DWORD),
            ("ProccntUsage", wintypes.DWORD), ("modBaseAddr", ctypes.POINTER(ctypes.c_ubyte)),
            ("modBaseSize", wintypes.DWORD), ("hModule", wintypes.HMODULE),
            ("szModule", wintypes.WCHAR * 256), ("szExePath", wintypes.WCHAR * 260),
        ]

    kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel32.Process32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    kernel32.Process32FirstW.restype = wintypes.BOOL
    kernel32.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    kernel32.Process32NextW.restype = wintypes.BOOL
    kernel32.Module32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(MODULEENTRY32W)]
    kernel32.Module32FirstW.restype = wintypes.BOOL
    kernel32.Module32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(MODULEENTRY32W)]
    kernel32.Module32NextW.restype = wintypes.BOOL
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                            ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    kernel32.ReadProcessMemory.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL

    INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
    TH32CS_SNAPPROCESS = 0x2
    TH32CS_SNAPMODULE = 0x8
    TH32CS_SNAPMODULE32 = 0x10
    PROCESS_VM_READ = 0x0010
    PROCESS_QUERY_LIMITED_INFORMATION = 0x1000


def winerror(action):
    raise OSError(f"{action}: {ctypes.WinError(ctypes.get_last_error())}")


def snapshot(flags, pid):
    handle = kernel32.CreateToolhelp32Snapshot(flags, pid)
    if not handle or handle == INVALID_HANDLE_VALUE:
        winerror("CreateToolhelp32Snapshot")
    return handle


def find_pids(exe_name):
    handle = snapshot(TH32CS_SNAPPROCESS, 0)
    try:
        entry = PROCESSENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        found = []
        if kernel32.Process32FirstW(handle, ctypes.byref(entry)):
            while True:
                if entry.szExeFile.casefold() == exe_name.casefold():
                    found.append(int(entry.th32ProcessID))
                if not kernel32.Process32NextW(handle, ctypes.byref(entry)):
                    break
        return found
    finally:
        kernel32.CloseHandle(handle)


def get_main_module(pid):
    handle = snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    try:
        entry = MODULEENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        if not kernel32.Module32FirstW(handle, ctypes.byref(entry)):
            winerror("Module32FirstW")
        # ToolHelp's first module is the process executable.
        address = ctypes.cast(entry.modBaseAddr, ctypes.c_void_p).value
        return address, int(entry.modBaseSize), entry.szExePath
    finally:
        kernel32.CloseHandle(handle)


def read_memory(handle, address, length):
    buf = ctypes.create_string_buffer(length)
    nread = ctypes.c_size_t()
    if not kernel32.ReadProcessMemory(handle, ctypes.c_void_p(address), buf,
                                       length, ctypes.byref(nread)) or nread.value != length:
        winerror(f"ReadProcessMemory(0x{address:X}, {length} bytes)")
    return buf.raw


def main():
    parser = argparse.ArgumentParser(description="Running GameClient.exe memory CRC32 (Windows x64)")
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--pid", type=int, help="PID of the running game")
    source.add_argument("--name", default="GameClient.exe", help="Executable name (default: GameClient.exe)")
    args = parser.parse_args()

    if sys.platform != "win32":
        parser.error("This script must run on Windows.")
    if ctypes.sizeof(ctypes.c_void_p) != 8:
        parser.error("Use 64-bit Python for the x64 game (for example: py -3-64).")

    if args.pid is not None:
        if args.pid <= 0:
            parser.error("--pid must be a positive integer")
        pid = args.pid
    else:
        pids = find_pids(args.name)
        if not pids:
            parser.error(f"No running process named {args.name!r}; start the game first")
        if len(pids) > 1:
            parser.error(f"Multiple {args.name} processes found: {pids}; use --pid")
        pid = pids[0]

    module_base, module_size, path = get_main_module(pid)
    handle = kernel32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        winerror("OpenProcess (try matching permissions or Run as administrator)")
    try:
        # Confirm module is a PE image and validate ImageBase is the process executable.
        head = read_memory(handle, module_base, 0x1000)
        if head[:2] != b"MZ":
            raise ValueError("Main module does not have an MZ header")
        pe_off = struct.unpack_from("<I", head, 0x3C)[0]
        if pe_off + 0x58 > len(head) or head[pe_off:pe_off+4] != b"PE\0\0":
            raise ValueError("Invalid PE header or header offset")
        opt_off = pe_off + 24
        if struct.unpack_from("<H", head, opt_off)[0] != 0x20B:
            raise ValueError("Expected a PE32+ (64-bit) executable")
        image_size = struct.unpack_from("<I", head, opt_off + 56)[0]
        print(f"PID: {pid}\nEXE: {path}\nLoaded ImageBase: 0x{module_base:X}")
        print(f"Image Size: 0x{image_size:X}\n")
        for name, rva, size in REGIONS:
            if size <= 0 or rva < 0 or rva + size > image_size or rva + size > module_size:
                raise ValueError(f"{name}: RVA/size is outside loaded image")
            va = module_base + rva
            buf = read_memory(handle, va, size)
            crc = zlib.crc32(buf) & 0xFFFFFFFF
            print(f"{name}: RVA=0x{rva:X}, SIZE=0x{size:X} ({size} bytes)")
            print(f"  VA=0x{va:X}, CRC32=0x{crc:08X}")
    finally:
        kernel32.CloseHandle(handle)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
