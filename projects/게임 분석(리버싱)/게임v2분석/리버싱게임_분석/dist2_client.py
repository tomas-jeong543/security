import socket
import time
import pygame
import math

def rol16(value, n):
    return ((value << n) | (value >> (16 - n))) & 0xFFFF

def keygen_1(text):

    #length = len(text) + 8

    data = b'\x47\x47\x02\x01' + len(text).to_bytes(2, 'little') + b'\x00\x00' + text

    key = int.from_bytes(b'\xC0\xDE', byteorder='big')

    #key = 0xC0DE

    for byte in data:
        #print(byte.to_bytes())
        key = rol16(byte + key, 3) ^ 0x5A5A


    return key.to_bytes(2,'little')

def keygen_2(data):
    
    key = int.from_bytes(b'\xC0\xDE', byteorder='big')
    for byte in data:
            key = rol16(byte + key, 3) ^ 0x5A5A
    
    return key.to_bytes(2,'little')

def keygen_3(data):
    
    key = int.from_bytes(b'\xC0\xDE', byteorder='big')
    for i, byte in enumerate(data):
        if i == len(data) - 1:
                key = key + byte
                return key.to_bytes(2,'little')     
        else:    
            key = rol16(byte + key, 3) ^ 0x5A5A
    
    #return key.to_bytes(2,'little')    
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 9999))


#이름 설정
nickname = b'Bot'
#이름에 따른 길이 설정 리틀 앤디안으로 설정
payload = b'GG\x02\x01' + len(nickname).to_bytes(2, 'little') + keygen_1(nickname) + nickname
#sock.settimeout(5.0)
sock.send(payload)

pygame.init()
screen = pygame.display.set_mode((1000, 700))
clock = pygame.time.Clock()

running = True
recv_buf = b""
NUM = 32
MY_ID = -1
KEY_UP = 1
KEY_DOWN = 2
KEY_LEFT = 4
KEY_RIGHT = 8
players = {}
angle_bytes = 0x00
walls = []
while running:
    for e in pygame.event.get():
            if e.type == pygame.QUIT:
                running = False

    recv_buf += sock.recv(8192)

    while len(recv_buf) >= 8:

        # 패킷 시작 확인
        if recv_buf[:2] != b"GG":
            pos = recv_buf.find(b"GG")

            if pos == -1:
                recv_buf = b""
                break

            recv_buf = recv_buf[pos:]

            if len(recv_buf) < 8:
                break

        payload_len = int.from_bytes(
            recv_buf[4:6],
            "little"
        )

        packet_len = payload_len + 8

        # 패킷 전체가 아직 안 들어옴
        if len(recv_buf) < packet_len:
            break

        packet = recv_buf[:packet_len]
        recv_buf = recv_buf[packet_len:]

        op = packet[3]           
        print("op: ", op)
        if op == 5:
            NUM = packet[12]
            
            if MY_ID != -1:

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
                posloc = 0
                mouse_x, mouse_y = pygame.mouse.get_pos()

                for i in range(0, NUM ):
                    ins_x, ins_y, ins_hp = packet[10*(i ) + 14: 10*(i ) + 16],  packet[10*(i ) + 16: 10*(i ) + 18],   packet[10*(i ) + 18: 10*(i ) + 20]
                    ins_y = int.from_bytes(ins_y, 'little', signed=True)
                    ins_x = int.from_bytes(ins_x, 'little', signed=True)
                    hp =  int.from_bytes(ins_hp, 'little', signed=True)
                    player_id =   int.from_bytes(packet[10*(i ) + 13: 10*(i ) + 14])
                    players[player_id] = (ins_x, ins_y, hp)

                    if(packet[10 * i + 13] == MY_ID):
                        posloc = i

               
                cur_x, cur_y = packet[10*(posloc ) + 14: 10*(posloc ) + 16],  packet[10*(posloc ) + 16: 10*(posloc ) + 18]
                cur_y = int.from_bytes(cur_y, 'little', signed=True)
                cur_x = int.from_bytes(cur_x, 'little', signed=True)
                print("cur_x: ", cur_x,  "cur_y: ", cur_y, " mouse_x: ", mouse_x, "mouse_y: ", mouse_y)
 
                ins = math.atan2(mouse_y - cur_y, mouse_x - cur_x)
                angle = int(180.0 * ins / math.pi)
                angle_bytes = angle.to_bytes(2, byteorder='little', signed=True)

                payload_move_ins = bytearray(payload)
                payload_move_ins[0:8] = b'GG\x02\x03\x03\x00\x00\x00'
                payload_move_ins[9:11] = angle_bytes
                payload_move_ins[8] = keyState

                payload_shoot_ins = bytearray(10)
                payload_shoot_ins[0:8] = b'GG\x02\x04\x02\x00\x00\x00'
                payload_shoot_ins[8:10] = angle_bytes
                     
                payload_move = b'GG\x02\x03\x03\x00' + keygen_2(payload_move_ins) + keyState.to_bytes() + angle_bytes    
                sock.send(payload_move)
            
                payload_shoot = b'GG\x02\x04\x02\x00' + keygen_2(payload_shoot_ins) + angle_bytes
                sock.send(payload_shoot)

                screen.fill((255, 255, 255))
                                
                for id, coord in players.items():
                    x, y, hp = coord
                    color = (255, 0, 0) if id != MY_ID else (0, 255, 0)
                    pygame.draw.circle(screen, color, (x, y), 10)           

                    for wall_x, wall_y, wall_width, wall_height in walls:
                                pygame.draw.rect(
                                screen,
                                (0, 0, 0),  # 벽 색상: 검정
                                pygame.Rect(wall_x, wall_y, wall_width, wall_height)
                            )
                                   # 체력바 설정
                    bar_width  = 40
                    bar_height = 5
               
                    bar_x = x - bar_width // 2
                    bar_y = y - 20
               
                    # 체력바 배경
                    pygame.draw.rect(
                        screen,
                        (80, 80, 80),
                        (bar_x, bar_y, bar_width, bar_height)
                    )
               
                    # 현재 체력
                    pygame.draw.rect(
                            screen,
                            (0, 255, 0),
                            (bar_x, bar_y, bar_width * hp / 100, bar_height)
                    )
                    
                                
                if MY_ID in players:
                    pygame.draw.line(screen, (255,0,0), (mouse_x, mouse_y), (cur_x, cur_y), 5)
                pygame.display.flip()    
        elif op == 7:
              print(NUM)
              for i in range(NUM):
                  print(nickname  , ": ", packet[17 * (i ) + 10 : 17 * (i) + 13])
                  if(nickname.hex() == packet[17 * (i ) + 10 :17 * (i) + 13].hex()):
                      MY_ID = packet[17 * i + 9]
                      break
        elif op == 2:
            print(packet[13])
            for i in range(0,7):
                #print( int.from_bytes(packet[8*(i) + 14 : 8*(i) + 16],'little'), " ", int.from_bytes(packet[8*(i) + 16 : 8*(i) + 18],'little')," ",  int.from_bytes(packet[8*(i) + 18 : 8*(i) + 20],'little'), " ",  int.from_bytes(packet[8*(i) + 20 : 8*(i) + 22],'little'))
                x = int.from_bytes(packet[8*(i) + 14 : 8*(i) + 16], "little")
                y = int.from_bytes(packet[8*(i) + 16 : 8*(i) + 18], "little")
                width = int.from_bytes(packet[8*(i) + 18 : 8*(i) + 20], "little")
                height = int.from_bytes(packet[8*(i) + 20 : 8*(i) + 22], "little")
                walls.append((x, y, width, height)) 
              