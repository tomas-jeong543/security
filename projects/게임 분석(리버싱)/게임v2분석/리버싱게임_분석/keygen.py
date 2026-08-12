def rol16(value, n):
    return ((value << n) | (value >> (16 - n))) & 0xFFFF


text = input("문자열 입력: ")
length = len(text) + 8



data = b'\x47\x47\x02\x01' + len(text).to_bytes(2, 'little') + b'\x00\x00' + text.encode()

key = int.from_bytes(b'\xC0\xDE', byteorder='big')

#key = 0xC0DE

for byte in data:
    #print(byte.to_bytes())
    key = rol16(byte + key, 3) ^ 0x5A5A


print(key.to_bytes(2,'little').hex())