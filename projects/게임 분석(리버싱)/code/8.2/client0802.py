import socket
import math

PACKET_PLAYER_COORD = 5
PACKET_WALL_INFO    = 2
PLAYER_ID = 0

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 9999))

nickname = b'Hello'
payload = b'GG\x01\x01' + len(nickname).to_bytes(2, 'little') + nickname

sock.send(payload)

data = sock.recv(1024)


WALL_INFO = []
if data[3] == PACKET_WALL_INFO:
    PLAYER_ID = data[6]
    wall_count = data[11]

    offset = 12
    for i in range(wall_count):
        x = int.from_bytes(data[offset:offset+2], 'little');  offset += 2
        y = int.from_bytes(data[offset:offset+2], 'little');  offset += 2
        w = int.from_bytes(data[offset:offset+2], 'little');  offset += 2
        h = int.from_bytes(data[offset:offset+2], 'little');  offset += 2
        WALL_INFO.append((x,y,w,h))

print(WALL_INFO)
import time

KEY_UP = 1
KEY_DOWN = 2
KEY_LEFT = 4
KEY_RIGHT = 8


import pygame

pygame.init()
screen = pygame.display.set_mode((1000, 700))
clock = pygame.time.Clock()

index = 0
angles = range(0, 360, 5)


running = True
players = {}
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

    if data[3] == PACKET_PLAYER_COORD:
        player_count = data[10]
        data = data[11:]

        players = {}
        for i in range(player_count):
            player_id = data[i*10]
            x = int.from_bytes(data[i*10+1:i*10+3],   'little')
            y = int.from_bytes(data[i*10+3:i*10+5], 'little')
            players[player_id] = (x, y)

        # print(players)

    screen.fill((255, 255, 255))

    for id, coord in players.items():
        x, y = coord
        print(PLAYER_ID, id, x, y)

        color = (255, 0, 0) if id != PLAYER_ID else (0, 255, 0)
        pygame.draw.circle(screen, color, (x, y), 10)


    angle = 180
    if PLAYER_ID in players:
        x, y = pygame.mouse.get_pos()
        my_x, my_y = players[PLAYER_ID]

        pygame.draw.line(screen, (255,0,0), (x, y), (my_x, my_y), 5)

        angle = 180 * math.atan2(y - my_y, x - my_x) / math.pi
        angle = int(angle) 
        if angle < 0:
            angle = -angle + 180
        print(angle)

        sock.send(b'GG\x01\x04\x02\x00' + angle.to_bytes(2, 'little'))        
    

    for wall_info in WALL_INFO:
        pygame.draw.rect(screen, (0, 0, 0), wall_info)

    pygame.display.flip()
    clock.tick(60)