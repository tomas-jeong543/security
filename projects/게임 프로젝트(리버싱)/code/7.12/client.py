import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 9999))
#이름 설정
nickname = b'Hello'
#이름에 따른 길이 설정 리틀 앤디안으로 설정
payload = b'GG\x01\x01' + len(nickname).to_bytes(2, 'little') + nickname

sock.send(payload)

data = sock.recv(1024)

print(data)
import time
while True:
    sock.send(b'GG\x01\x03\x03\x00\x00\xfa\xff')
    data = sock.recv(1024)

    print(data)
    time.sleep(0.1)

