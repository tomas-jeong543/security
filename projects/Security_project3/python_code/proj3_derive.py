import struct

MASK32 = 0xFFFFFFFF


def u32(x: int) -> int:
    return x & MASK32


def rotl32(x: int, n: int) -> int:
    x &= MASK32
    n &= 31
    return u32((x << n) | (x >> (32 - n)))


def rotl8(x: int, n: int) -> int:
    x &= 0xFF
    n &= 7
    return ((x << n) | (x >> (8 - n))) & 0xFF


def mix32(x: int) -> int:
    x = u32(x)

    t = u32((x >> 16) ^ x)
    t = u32(t * 0x7FEB352D)
    t = u32((t >> 15) ^ t)

    t = u32(t * 0x846CA68B)
    return u32((t >> 16) ^ t)


def hash_name(name: bytes) -> int:
    h = 0x811C9DC5

    for b in name:
        h = rotl32(
            u32((b ^ h) * 0x01000193),
            5
        )
        h = u32(h ^ 0x9E3779B9)

    h ^= u32(len(name) * 0x045D9F3B)
    return mix32(h)


def quarter(a: int, b: int, c: int, d: int) -> tuple[int, int, int, int]:
    a = u32(a + b)
    d = rotl32(d ^ a, 16)

    c = u32(c + d)
    b = rotl32(b ^ c, 12)

    a = u32(a + b)
    d = rotl32(d ^ a, 8)

    c = u32(c + d)
    b = rotl32(b ^ c, 7)

    return a, b, c, d


def derive(name: str) -> bytes:
    name_bytes = name.encode("utf-8")
    h = hash_name(name_bytes)
    name_len = len(name_bytes)

    s0 = u32(h ^ 0x61707865)
    s1 = u32(rotl32(h, 7) ^ 0x3320646E)
    s2 = mix32(h ^ 0x79622D32)
    s3 = u32(name_len * 0x9E3779B1) ^ 0x6B206574

    for i in range(6):
        s0, s1, s2, s3 = quarter(s0, s1, s2, s3)

        s0 ^= u32(i * 0xA5A5A5A5 + 0x13579BDF)
        s0 = u32(s0)

        rotation = 3 * i + 6
        s2 = rotl32(h ^ s2, rotation)

    words = [
        u32(s0 ^ mix32(s2)),
        u32(s1 + rotl32(s3, 3)),
        u32(s2 ^ rotl32(s0, 11)),
        u32(s3 + mix32(s1)),
    ]

    result = bytearray(struct.pack("<4I", *words))

    for i in range(16):
        x = result[i] ^ ((29 * i + 83) & 0xFF)
        result[i] = rotl8(x, 3)

    return bytes(result)


def format_serial(data: bytes) -> str:
    hex_text = data.hex().upper()
    return "-".join(
        hex_text[i:i + 8]
        for i in range(0, 32, 8)
    )


name = input("Name: ")

derived = derive(name)

print("Raw :", derived.hex().upper())
print("Serial:", format_serial(derived))