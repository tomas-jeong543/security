data = input("data: ")

stats = {}

ans_head = "AB"
ans = []
for item in data.split(","):
    key, value = item.split(":")
    stats[key.strip()] = int(value.strip())

goal = int(stats["HP"]).to_bytes(2,"big") + int(stats["DEX"]).to_bytes(1, "big") + int(stats["END"]).to_bytes(1,"big") + int(stats["INT"]).to_bytes(1,"big") + int(stats["VIT"]).to_bytes(1,"big") + int(stats["AGI"]).to_bytes(1,"big") + int(stats["STR"]).to_bytes(1,"big")   

goal_int = int.from_bytes(goal)
print(goal)
print(goal_int)
det_check = False
start_num = 2


while start_num != goal_int:
    #print(goal_int)
    if goal_int % 2 == 0:
        goal_int = goal_int // 2
        ans.insert(0, 'B')
    else:
        goal_int = goal_int -1        
        ans.insert(0, 'A')

ans_body = "".join(ans)
print(ans_head + ans_body)

ans_full = ans_head + ans_body

check_num = 0
for i in range(len(ans_full)):
    if ans_full[i] == 'A':
        check_num += 1
    else:
        check_num = check_num * 2    

if(check_num == int.from_bytes(goal)):
    print("correct")       