import math
# p,q의 값을 소인수 분해로 구하는 과정을 코드로 작성 결과로  p =65287   q = 65419라는 사실을 알아냈다. 
n1 = 4271010253
n2 = 201326609
p =65287   
q = 65419

def check_prime(n):
    for i in range(2, int(math.sqrt(n))):
        if n % i == 0:
            return False
    return True    

for i in range(2, int(math.sqrt(n1))):
    if(n1 % i == 0 and check_prime(i) and check_prime(n1 // i)):
        print(i," ",  n1 // i)
        break

#p와 q값을 통해서 pi_n을 계산함
pi_n = (p -1) * (q - 1)

# d = e**(-1) mod pi_n 를 통해서 복호화 키를 계산함
decrypt = pow(n2,-1,pi_n)
print(decrypt)
# decyrpt 가 1384538333라는 걸 파악함