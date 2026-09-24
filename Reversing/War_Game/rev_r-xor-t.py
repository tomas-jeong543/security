data = "C@qpl==Bppl@<=pG<>@l>@Blsp<@l@AArqmGr=B@A>q@@B=GEsmC@ArBmAGlA=@q"
data = data.encode()
#print(data.hex(" "))

ans = bytearray()
ans_print = bytearray()
for i in range(0,64):
    ans.append( (data[i] ^ 3) & 0xFF)

for i in range(0, 32):
    ans[i], ans[63 - i] = ans[63 - i], ans[i]

for i in range(0,64):
    for j in range(0,256):
        num = j
        num  = ((num + 13) & 0x7F)
        if num == ans[i]:
            ans_print.append(j)
            break

print(ans_print.decode())