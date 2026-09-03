
def change_func(s, val, opt):
    if opt == 3:
        val = val % 8
        for i in range(len(s)):
            s[i] = ((s[i] >> (8 - val)) | (s[i] << val)) & 0xFF
    elif opt == 1:
        val = val & 0xFF
        for i in range(len(s)):
            s[i] = (s[i] - val) & 0xFF            
    elif opt == 2:
        for i in range(len(s)):
            s[i] = (s[i] ^ val) & 0xFF

    return s   

with open("output.bin", "rb") as f:
    output_bin = f.read()

with open("program.bin", "rb") as f:
    program_bin = f.read()

print(program_bin)
print(len(program_bin))
plen = len(program_bin) 
slen = len(output_bin)

idx = plen - 1
ans = bytearray()

for byte in output_bin:
    ans.append(byte)

for i in range(0,plen -2 , 2):
    val =  program_bin[idx - i ]
    val = val & 0xFF
    opt = program_bin[idx - i - 1]
    #print(idx - i, idx - i - 1)

    ans = change_func(ans,val, opt)
# for i in range(2,plen , 2):
#     val =  program_bin[i + 1 ]
#     val = val & 0xFF
#     opt = program_bin[i]
#     ans = change_func(ans,val, opt)

#print(ans.decode())