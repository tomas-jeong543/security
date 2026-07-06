from itertools import cycle

# secret_pdf = open('./top_secret.pdf', 'rb').read()

# def gen_key(seed):
#     key = []
#     for i in range(256):
#         key.append(seed & 0xFF)
#         seed = (seed * 1103515245 + 12345) & 0xFFFF_FFFF
    
#     return bytes(key)

# key = gen_key(2587912859102)
# print(key)

# encrypted = bytes(x ^ y for x, y in zip(secret_pdf, cycle(key)))
# open('./encrypted.pdf.ransom', 'wb').write(encrypted)

#이것 또한 1번 문제와 동일한 문제로 차이점은 그냥 문자열에 하던 걸 pdf파일에다가 똑같이 한거라는 차이 
# 밖에 없다. 
encrypted_pdf = open('./encrypted.pdf.ransom', 'rb').read()

def gen_key(seed):
     key = []
     for i in range(256):
         key.append(seed & 0xFF)
         seed = (seed * 1103515245 + 12345) & 0xFFFF_FFFF
    
     return bytes(key)

key = gen_key(2587912859102)
decoded = bytes(x ^ y for x, y in zip(encrypted_pdf, cycle(key)))
open('./top_secret.pdf', 'wb').write(decoded)

