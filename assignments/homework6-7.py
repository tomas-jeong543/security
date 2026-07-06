

data = bytes.fromhex('050804013C7D7A78287C742B7D3534346A3763626F3B386C6C643F3D6F580050575055045715')

ans = "".join([chr(data[i] ^ (i + 67)) for i in range(38)])

print("ans:")
print(ans)