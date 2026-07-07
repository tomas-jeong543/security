
#ida를 이용해 xor의 정연산을 구함
def xor(msg):
    return bytes(x ^ (i + 1) for i,x in enumerate(msg))

#동일 + table의 주소를 ida의 주소를 16진수로 가져오는 방식을 이용해 구했다.
def add(msg):
    # ida를 이용해서 table의 주소가 magic1에 있고 이를 추적해서 데이터 부분에서 table정보를 가져오면 13374142가 나와서 그렇다
    table = bytes.fromhex('13374142')
    return bytes((x + table[i % 4]) & 0xFF for i, x in enumerate(msg))

#이 함수의 경우에는 복잡해 보여서 xdbg64를 이용해 동적분석을 breakpoint와 rip을 이용해 전후 결과를 비교한 결과
#앞 뒤 비트를 서로 맞바꾼 걸로 들어났다. 그걸 이용해서 swap함수의 정연산을 구했다.
def swap(msg):
    result = list(msg)
    for i in range(0, len(msg), 2):
        result[i], result[i + 1] = result[i + 1], result[i]

    return bytes(result)
    
#전체 정연산 과정으로 입력값의 길이가 정확히 16바이트인지 검증한 뒤, 앞서 정의한 xor → add → swap 과정을 순서대로 거쳐 암호문을 반환한다.
def encypt(msg : bytes):
    if len(msg) != 16:
        print('length != 16')
        return
    
    msg = xor(msg)
    msg = add(msg)
    msg = swap(msg)

    #print(msg.hex())
    return msg

# sol은 역산 혹은 그냥 브루토포스 함수

#역연산 선형일 때 주로 이용한다 그냥 했던 걸 거꾸로 스택처럼 하면 된다
def decpryt():
     #target 0의 메모리 주소를 가져온 거다. 여기에는 correct에 들어가기 위한 값들이 적혀있다.
    flag = bytearray(bytes.fromhex('9E65B3A1A68AB3B482629A897552687E'))
    
    for i in range(0, 16, 2):
        flag[i], flag[i + 1] = flag[i + 1], flag[i]
    table = bytes.fromhex('13374142')
    flag = bytes((x - table[i % 4]) & 0xFF for i, x in enumerate(flag))
    flag = bytes(x ^ (i + 1) for i, x in enumerate(flag))    

    print(flag)

#브루토포스 방법 모든 경우의 수를 조사 늘 가능한 방법은 아니다. 비선형이고 완전 탐색이 가능한 경우에 주로 이용한다
def brutoforce():
    #target 0의 메모리 주소를 가져온 거다. 여기에는 correct에 들어가기 위한 값들이 적혀있다.
    flag = bytearray(bytes.fromhex('9E65B3A1A68AB3B482629A897552687E'))
    #다양한 입력값에 따른 다양한 답을 저장하기 위한 매핑 테이블을 리스트로 구현했다.
    _map = [{} for _ in range(16)]
    bf  = [0] * 16
    for j in range(16):
        for i in range(256):
            #bf[j] = i가 안되는 이유는 그렇게 하면 결국 j = 0 인 경우 우리는 result[1]값을 변화시키고 result[0]값은 계속
            #그대로이기 때문에 그렇다. 그렇게 되면 아래의 _map[j][result[j]] = i 구문에 _map[0]이라는 사전에 단 하나의 Key만 생성하며, 
            # 마지막 i 값인 255로 계속 덮어쓰기만 합니다. result 배멸의 0번째 인덱스의 다양한 암호문 결과에 따른 입력값 매핑 테이블이 만들어지지 않으므로, 
            # 추후 flag[0]을 조회했을 때 데이터가 없거나 엉뚱한 값(255)이 나오게 된다. 그래서 애러가 난다
            bf[j + (1 if j % 2 == 0 else -1)] = i
            result = encypt(bytes(bf)) 
            _map[j][result[j]] = i
    #결과출력
    for j in range(16):
        twin = j + (1 if j % 2 == 0 else -1)
        print(f'{j + 1} 번째 자리 플래그', chr(_map[twin][flag[twin]]))
#encypt(b'0' * 16)
#decpryt()
brutoforce()