import random
import sys

def expr(depth):
    if depth==1 or random.random()<1.0/(2**depth-1): 
        return str(int(random.random() * 100) + 1)+".0"
    return '(' + expr(depth-1) + random.choice(['+','-','*','/']) + expr(depth-1) + ')'

print (expr( int(sys.argv[1]) ))
