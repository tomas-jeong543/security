check = bytes.fromhex("ADD8CBCB9D97CBC492A1D2D7D2D6A8A5DCC7ADA3A1984C00")

ans = bytearray()
detend = True

for i in range(0,256):
    startnum = i
    
    detend = True
    for j in range(0, 24):
        if startnum <= check[j]:
            
            ans.append(startnum)
            startnum = check[j] - startnum
        else:
            #print( i , " ",  startnum, " " , check[j])
            detend = False
            ans = bytearray()
            break    
    if detend:
        break

print("ans: ", ans.decode())            
print( ans.decode())   