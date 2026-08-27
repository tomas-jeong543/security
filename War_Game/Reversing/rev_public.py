data = bytes.fromhex(
    "40E1DCD400000000"
    "E2DF83A100000000"
    "06E363C300000000"
    "68E2D2F900000000"
    "09241AC400000000"
    "FBC09B2A00000000"
    "B5224E9A00000000"
    "9AEF387D00000000"
    "9F925FB100000000"
    "EF7BD69E00000000"
    "E7EACD9900000000"
)

for i in range(0, len(data), 8):
    chunk = data[i:i+8]

    value = int.from_bytes(chunk, "little")

    print(f"{chunk.hex(' ')} -> {value:#018x}")