pumpkin = [124, 112, 59, 73, 167, 100, 105, 75, 59, 23, 16, 181, 165, 104, 43, 49, 118, 71, 112, 169, 43, 53]
pie = 1
for i in range(100):
    for j in range(len(pumpkin)):
        pumpkin[j] ^= pie
        pie = ((pie ^ 0xff) + (j * 10)) & 0xff;

hex_string = bytes(pumpkin).hex()
print(hex_string)