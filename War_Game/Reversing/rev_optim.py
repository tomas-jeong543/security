#39바이트 
def calfunc(s):
    return

data = bytes.fromhex("220c6a33204455fb390074013c4156d704316528205156d70b217c14255b6ce10837651234464e")
encrypt = bytes.fromhex("6644117755223388")
ans = bytearray()

for i in range(39):
    ans.append(data[i] ^ encrypt[i % 8])



print(ans.decode())
