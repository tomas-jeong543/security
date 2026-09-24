ans = bytearray()
num_same = 1
same_char = False
reset = False

with open("secretMessage.enc", "rb") as f:
    #내부 함수의 메커니즘 2개 연속 같은 바이트 반복시에는 그 다음 바이트는 이전 연속된 두 바이트에 나오는 바이트값의 횟수고 그 다음에는 그와는 다른 바이트가 나온다.
    while True:
        data = f.read(1)
        if data == b'':
            break
        #연속된 바이트 값 처리
        if same_char:
            ins = ans[len(ans) - 1]
            dup_num =  int.from_bytes(data,"big")  
            ans.extend( [ins] * dup_num )

            same_char = False
            reset = True
            num_same = 1    
            continue

        if( len(ans) == 0 or reset or ( ans[len(ans) - 1] != int.from_bytes(data,"big") ) ):
            ans.append(int.from_bytes(data,"big"))
            num_same = 1
            if reset:
                reset = False
        elif(num_same == 1 and (ans[len(ans) - 1] ==  int.from_bytes(data,"big")) ):
             ans.append(int.from_bytes(data,"big"))
             num_same = 2
             same_char = True
           

print(len(ans))
#파일 출력
with open("secretMessage.raw", "wb") as f:
    f.write(ans)



