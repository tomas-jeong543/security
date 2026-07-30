import socket
import math

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 9999))

nickname = b'Hello'
payload = b'GG\x01\x01' + len(nickname).to_bytes(2, 'little') + nickname

sock.send(payload)

data = sock.recv(1024)
PLAYER_ID = data[6]

# print(data)
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

PACKET_PLAYER_COORD = 5
PACKET_WALL_COORD = 2


walls = []
if data[3] == PACKET_WALL_COORD:
    wall_count = data[11]
    wall_bytes = data[12:12 + wall_count * 8]

    print("wall info")
    print("wall count:", wall_count)

    for i in range(wall_count):
        wall = wall_bytes[i * 8:(i + 1) * 8]
        x = int.from_bytes(wall[0:2], "little")
        y = int.from_bytes(wall[2:4], "little")
        width = int.from_bytes(wall[4:6], "little")
        height = int.from_bytes(wall[6:8], "little")

        walls.append((x, y, width, height))
    print(walls)

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

    if data[3] == PACKET_WALL_COORD:
        print("wall info")
    elif data[3] == PACKET_PLAYER_COORD:
        player_count = data[10]
        data = data[11:]

        players = {}
        for i in range(player_count):
            player_id = data[i*10]
            x = int.from_bytes(data[i*10+1:i*10+3],   'little')
            y = int.from_bytes(data[i*10+3:i*10+5], 'little')
            players[player_id] = (x, y)

        print(players)
    
    screen.fill((255, 255, 255))

    for id, coord in players.items():
        x, y = coord
        #print(PLAYER_ID, id, x, y)

        color = (255, 0, 0) if id != PLAYER_ID else (0, 255, 0)
        pygame.draw.circle(screen, color, (x, y), 10)


    angle = 180
    if PLAYER_ID in players:
        x, y = pygame.mouse.get_pos()
        my_x, my_y = players[PLAYER_ID]

        pygame.draw.line(screen, (255,0,0), (x, y), (my_x, my_y), 5)
        # 벽 그리기
        for wall_x, wall_y, wall_width, wall_height in walls:
            pygame.draw.rect(
            screen,
            (0, 0, 0),  # 벽 색상: 검정
            pygame.Rect(wall_x, wall_y, wall_width, wall_height)
            )
        #angle = 180 * math.atan2(y - my_y, x - my_x) / math.pi
        #angle = int(angle) 
        #if angle < 0:
        #   angle = -angle + 180
        #print(angle)
        angle = int(math.degrees(math.atan2(y - my_y, x - my_x))) % 360
        sock.send(b'GG\x01\x04\x02\x00' + angle.to_bytes(2, 'little'))        
    

    pygame.display.flip()
    clock.tick(60)