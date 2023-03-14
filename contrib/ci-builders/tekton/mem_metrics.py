#!/usr/bin/python
import re
import fileinput
import statistics as stat

metrics=[]
with fileinput.input() as finput:
    for line in finput:
        #strip off the rate metric and other unneeded characters
        line = line.strip('\n')
        data = re.split(r'[:()]', line)
        data = [i for i in data if i]
        #remove heaptrack overhead string
        if(data[0] == "peak RSS "):
            del data[1]    
        metrics.append(data)

for n in metrics:
    print(n[0], n[1])