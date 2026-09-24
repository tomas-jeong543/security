import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 9999))
#이름 설정
nickname = b'Bot'
#이름에 따른 길이 설정 리틀 앤디안으로 설정
payload = b'GG\x01\x01' + len(nickname).to_bytes(2, 'little') + nickname

sock.send(payload)

data = sock.recv(1024)

print(data)
import time
#이걸 통해 움직이는 방향을 파악할 수 있다
KEY_UP = 1
KEY_DOWN = 2
KEY_LEFT = 4
KEY_RIGHT = 8
keyState = 0

# while True:
#     keyState |= KEY_UP
#     sock.send(b'GG\x01\x03\x03\x00' + keyState.to_bytes(1)+ b'\xfa\xff')
#     data = sock.recv(1024)

#     print(data)
#     time.sleep(0.1)


import pygame

pygame.init()
screen = pygame.display.set_mode((640, 480))
clock = pygame.time.Clock()

index = 0
angles = range(0, 360, 5)

running = True
while running:
    for e in pygame.event.get():
        if e.type == pygame.QUIT:
            running = False

    keyState = 0
    keys = pygame.key.get_pressed()
    if keys[pygame.K_w]:
        keyState |= KEY_UP
    
    if keys[pygame.K_s]:
        keyState |= KEY_DOWN
    
    if keys[pygame.K_a]:
        keyState |= KEY_LEFT
    
    if keys[pygame.K_d]:
        keyState |= KEY_RIGHT

    sock.send(b'GG\x01\x03\x03\x00' + keyState.to_bytes(1) + b'\xfa\xff')
    data = sock.recv(1024)
    
    sock.send(b'GG\x01\x04\x02\x00' + angles[index % len(angles)].to_bytes(2, 'little'))        
    
    # for angle in range(0,360, 15):
    #     sock.send(b'GG\x01\x04\x02\x00' + angle.to_bytes(2, 'little'))
    #     data = sock.recv(1024)
    #sock.send(b'GG\x01\x04\x02\x00' + (180).to_bytes(2, 'little'))        
    index += 1

    clock.tick(60)