
def ROR_1(x, shift):
    shift %= 8
    rol = ((x >> shift) | (x << (8 - shift))) & 0xFF
    return rol
data = bytes.fromhex("52DFB360F18B1CB557D19F384B29D9267FC9A3E953184FB86ACB87585B391E00")
ans = bytearray()

for i in range(0,31):
    ans.append( ROR_1(data[i] ^ i, i & 7))

print(ans.decode())    
