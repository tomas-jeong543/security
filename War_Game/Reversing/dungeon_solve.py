from pwn import * 
import re 
 
HOST = "host3.dreamhack.games" 
PORT = 11198 
 
 
def make_spell(stats): 
    # HP 2byte + 나머지 각각 1byte 
    goal = ( 
        stats["HP"].to_bytes(2, "big") 
        + stats["DEX"].to_bytes(1, "big") 
        + stats["END"].to_bytes(1, "big") 
        + stats["INT"].to_bytes(1, "big") 
        + stats["VIT"].to_bytes(1, "big") 
        + stats["AGI"].to_bytes(1, "big") 
        + stats["STR"].to_bytes(1, "big") 
    ) 
 
    goal_int = int.from_bytes(goal, "big") 
 
    ans = [] 
 
    # AB를 실행하면 
    # 0 -> A -> 1 -> B -> 2 
    # 
    # 따라서 2에서 goal_int까지 가는 연산을 역으로 찾는다. 
    while goal_int != 2: 
        if goal_int % 2 == 0: 
            goal_int //= 2 
            ans.append("B") 
        else: 
            goal_int -= 1 
            ans.append("A") 
 
    ans.reverse() 
 
    return "AB" + "".join(ans) 
 
 
def parse_stats(line): 
    """ 
    예: 
    [INFO] HP: 56455, STR: 215, AGI: 49, 
           VIT: 196, INT: 189, END: 129, DEX: 84 
    """ 
 
    matches = re.findall( 
        rb"(HP|STR|AGI|VIT|INT|END|DEX):\s*(\d+)", 
        line 
    ) 
 
    stats = {} 
 
    for key, value in matches: 
        stats[key.decode()] = int(value) 
 
    return stats 
 
 
io = remote(HOST, PORT) 
 
while True: 
    try: 
        line = io.recvline() 
    except EOFError: 
        break 
 
    print(line.decode(errors="ignore"), end="") 
 
    if b"[INFO]" in line: 
        stats = parse_stats(line) 
 
        print("[+] parsed:", stats) 
 
        spell = make_spell(stats) 
 
        print("[+] spell:", spell) 
 
        # 서버가 Cast your spell!: 을 출력할 때까지 기다림 
        io.recvuntil(b"Cast your spell!:") 
 
        # 즉시 정답 전송 
        io.sendline(spell.encode()) 
 
# 플래그 또는 마지막 출력 확인 
io.interactive()