data = bytes.fromhex("ACF30C25A310B72516C6B7BC072502D5C61107C5000000000000000000000000")
ans = bytearray()


for i in range(21):
   for j in range(0,256):
      num = j
      num = num * -5
      num = num & 0xFF
      if(num == data[i]):
         ans.append(j)
         break

print(ans.decode())    