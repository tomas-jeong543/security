#Python에서 TCP나 UDP 네트워크 통신을 하기 위한 socket 모듈을 불러온다.
import socket
#socket.AF_INET      IPv4 주소 사용
#socket.SOCK_STREAM TCP 통신 사용
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 9999))
#이름 설정
nickname = b'Hello'
#이름에 따른 길이 설정 리틀 앤디안으로 설정
payload = b'GG\x01\x01' + len(nickname).to_bytes(2, 'little') + nickname
#서버에 패킷을 전송
sock.send(payload)
#서버가 보내는 데이터를 최대 1024바이트까지 읽는다. 여기서 1024 = 한 번에 받을 최대 크기다
data = sock.recv(1024)

print(data)
#무한 반복으로 호출
import time
while True:
    sock.send(b'GG\x01\x03\x03\x00\x00\xfa\xff')
    data = sock.recv(1024)

    print(data)
    time.sleep(0.1)

