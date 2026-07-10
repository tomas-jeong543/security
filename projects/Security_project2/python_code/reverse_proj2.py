def dec(data):
    return bytes(b ^ 0x5A for b in data)

# p0 = (1060584212).to_bytes(4, "little") + (31328).to_bytes(2, "little")
# p1 = b"\t?(3;6`z"
# p2 = (673723673).to_bytes(4, "little") + b"?9.{P"
# p3 = b"\r(54=z)?(3;6tP"
# p4 = b"\x1B99?))z>?43?>tP"

#for i, p in enumerate([p0, p1, p2, p3, p4]):
#    print(i, dec(p))

eo = "08 0C 0A 68 05 09 1B 16 0E 05 68 6A 68 6C"
eo = bytes.fromhex(eo)
print(dec(eo).decode(errors="ignore"))