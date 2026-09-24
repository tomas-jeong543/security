from itertools import cycle

plain = input('Enter the secret message: ')
key = b'SECRET_50F281A92D'

result = bytes(x ^ y for x, y in zip(plain.encode(), cycle(key)))

print(result.hex().upper())

# 출력 결과:
# 1231636075666A1B007F1C09006D19662C366506333720371557295C5650615B5764312A2C3F203071
expresult = 0x1231636075666A1B007F1C09006D19662C366506333720371557295C5650615B5764312A2C3F203071
byte_length = (expresult.bit_length() + 7) // 8
expresult = expresult.to_bytes(byte_length,'big')
# x ^ key = result, key ^ result = x라는 사실을 이용하면 쉽게 문제를 해결할 수 있다

ans = bytes(x ^ y for x, y in zip(cycle(key), expresult))
ans = ans.decode()
print(ans)