def fnv1a_32(data: bytes) -> int:
    h = 2166136261

    for b in data:
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF

    return h


def ror32(x: int, n: int) -> int:
    x &= 0xFFFFFFFF
    return ((x >> n) | (x << (32 - n))) & 0xFFFFFFFF


def make_serial(name: str) -> str:
    name = name.lower()
    secret = "RVP2_SALT_2026"

    buf = f"{name}:{secret}".encode("ascii")

    h = fnv1a_32(buf)

    x = (h ^ 0xA5C3F19B) & 0xFFFFFFFF
    y = (ror32(h, 25) + 0x13579BDF) & 0xFFFFFFFF

    part1 = h & 0xFFFF
    part2 = x & 0xFFFF
    part3 = y & 0xFFFF
    part4 = (y ^ x) & 0xFFFF

    return f"{part1:04X}-{part2:04X}-{part3:04X}-{part4:04X}"
print(make_serial("Tom"))
