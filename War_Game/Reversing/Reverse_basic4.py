check = bytes.fromhex("242713C6C61316E647F5269647F54627132626C656F5C3C3F5E3E300")

answer = bytearray()
for num in check:
    answer.append((num >> 4 & 0XFF) | (num << 4 & 0XFF))

print(answer.decode())

# x << 4 | x >> 4 -> abcd efgh => efgh abcd 와 동일 즉 efgh 0000 | 0000 abcd와 동일하다고 보면 된다.




