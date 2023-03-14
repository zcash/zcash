#!/usr/bin/python
import fileinput
import statistics as stat

metrics=[]
with fileinput.input() as finput:
    for line in finput:
        metrics.append(float(line))

print("median: ", stat.median(metrics))
print("min: ", min(metrics))
print("max: ", max(metrics))
print("q1: ", stat.median_low(metrics))
print("q3: ", stat.median_high(metrics)) 