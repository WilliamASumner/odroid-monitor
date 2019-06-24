#!/usr/bin/python
from time import sleep
from sys import argv
x = 3
try:
        x = int(argv[1])
except:
        pass
while (x > 0):
        print(str(x) + "...")
        x -= 1
        sleep(1)
print("DONE")
