data = bytes.fromhex(
    "40E1DCD400000000E2DF83A10000000006E363C30000000068E2D2F90000000009241AC400000000FBC09B2A00000000B5224E9A000000009AEF387D000000009F925FB100000000EF7BD69E00000000E7EACD9900000000"
)
# n1 = p * q
n1 = 4271010253
# n2 = e즉 공개 키
n2 = 201326609
#d = private 키 Rev_public2.py에서 소인수 분해를 통함 실제로 숫자가 충분히 큰 경우에는 구하는 게 거의 불가능에 가깝다
decrypt = 1384538333

dec = decrypt.to_bytes(4,"little")
#데이터의 길이
datalen = len(data) // 4

#4바이트 씩 ciphertext를 가져와서 이를 private키를 가지고 복호화를 한다.
for i in range(0, datalen):
    data_chunk = int.from_bytes(data[4 * i : 4 * i + 4],"little")
    ans_byte = pow(data_chunk, decrypt, n1).to_bytes(4,"little")
    print(ans_byte.decode(), end = "")
