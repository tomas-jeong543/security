
#data가 엄청 길 때 유용하다
#data1 = open('chall3.exe','rb').read()
#data1 = data1[0x2400:0x2400 + 24]


data = bytes.fromhex("4960677463674266807869697B996D8868949F8D4DA59D45")
flag = []


for i in range(24):
    x = (data[i] - 2*i) ^ i
    print(chr(x), end ='')

#더 간단한 방법

print(bytes((data[i] - 2* i) ^ i) for i in range(24))
