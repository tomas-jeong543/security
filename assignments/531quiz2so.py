from itertools import cycle
import os

plain = input('Enter the secret message: ')
key = os.urandom(14)

plain = '[TOP SECRET] : ' + plain
result = bytes(x ^ y for x, y in zip(plain.encode(), cycle(key)))

print(result.hex().upper())

# 출력 결과:
# 35E32F4E19BA1F00841BC8831B994EF12C5F7E926321E63AABEE02C70D82562D0EDE6D27AB

#아까전이랑 유사한 문제지만 여기서 차이점은 key가 난수라는 거고 힌트는 '[TOP SECRET] : '을 이용해
#키를 찾고 그 키 xor result를 통해 plain값을 찾을 수 있다.
#
plain_ex = '[TOP SECRET] :'

expresult_or = 0x35E32F4E19BA1F00841BC8831B994EF12C5F7E926321E63AABEE02C70D82562D0EDE6D27AB
byte_length = (expresult_or.bit_length() + 7) // 8
expresult = expresult_or.to_bytes(byte_length, 'big')
expresult = expresult[0:14]
expresult_or = expresult_or.to_bytes(byte_length , 'big')

expkey = bytes(x ^ y for x, y in zip(plain_ex.encode(), expresult))
ans = bytes(x ^ y for x, y in zip(cycle(expkey), expresult_or))
print(ans.decode())



