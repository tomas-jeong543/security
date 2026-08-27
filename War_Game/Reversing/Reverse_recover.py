encryption = bytes.fromhex("DEADBEEF")
idx = 0

decrypted = bytearray()

with open("encrypted", "rb") as f:
    while chunk := f.read(16):
        for b in chunk:
           
            b -= 19
            b = b ^ encryption[idx % 4]
            #이 마스킹 때문에 꽤나 애를 먹었다.
            b = b & 0xFF 
            decrypted.append(b)
            idx += 1            
        

with open("flag.png", "wb") as f:
    f.write(decrypted)        


